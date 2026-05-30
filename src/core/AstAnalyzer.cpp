#include "AstAnalyzer.h"
#include <QDir>
#include <QTemporaryFile>
#include <QJsonParseError>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <functional>

// 调试日志文件（写入临时目录，方便排查）
static QFile* g_debugLog = nullptr;
static QTextStream* g_debugStream = nullptr;

static void initDebugLog() {
    if (g_debugLog) return;
    QString path = QDir::temp().filePath("algo_viz_analyzer_debug.log");
    g_debugLog = new QFile(path);
    if (g_debugLog->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        g_debugStream = new QTextStream(g_debugLog);
        *g_debugStream << "=== AstAnalyzer Debug Log ===\n";
        g_debugStream->flush();
        qDebug() << "Debug log:" << path;
    }
}

#define DBG_LOG(msg) do { if (g_debugStream) { *g_debugStream << msg << "\n"; g_debugStream->flush(); } } while(0)

AstAnalyzer::AstAnalyzer(QObject* parent) 
    : QObject(parent), m_hasMain(false) {}

bool AstAnalyzer::analyze(const QString& code) {
    initDebugLog();
    DBG_LOG("=== analyze() called, source len=" << code.length() << " ===");
    DBG_LOG("SOURCE:\n" << code.toStdString().c_str());
    m_variables.clear();
    m_operations.clear();
    m_hasMain = false;
    m_sourceCode = code;
    m_sourceUtf8 = code.toUtf8();  // 用于byte→char offset转换
    
    // 1. 调用Clang生成AST JSON
    QString astJson = runClangAstDump(code);
    if (astJson.isEmpty()) {
        m_lastError = "Failed to run Clang AST dump";
        return false;
    }
    
    // 2. 解析JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(astJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        m_lastError = "AST JSON parse error: " + parseError.errorString();
        qDebug() << "JSON parse error at:" << parseError.offset;
        qDebug() << "Raw JSON:" << astJson.left(500);
        return false;
    }
    
    // 3. 遍历AST
    QJsonObject root = doc.object();

    // 遍历所有用户自定义 FunctionDecl 节点（main + 自定义函数如 bubbleSort）
    // 通过 range 偏移过滤：只遍历在源码范围内的函数声明，排除系统头文件函数
    if (root.contains("inner")) {
        QJsonArray topNodes = root["inner"].toArray();
        for (const QJsonValue& topNode : topNodes) {
            QJsonObject obj = topNode.toObject();
            if (obj["kind"].toString() == "FunctionDecl") {
                // 过滤：函数的 range 必须在源码范围内，排除系统头文件中的函数
                QJsonObject range = obj["range"].toObject();
                if (range.contains("begin")) {
                    int beginOff = byteToCharOffset(range["begin"].toObject()["offset"].toInt());
                    int endOff = byteToCharOffset(range["end"].toObject()["offset"].toInt());
                    if (beginOff >= 0 && beginOff < m_sourceCode.length() && endOff > 0) {
                        qDebug() << "Traversing function:" << obj["name"].toString();
                        traverseAst(obj);
                    }
                }
            }
        }
    }
    
    qDebug() << "AST Analysis complete:";
    qDebug() << "  Variables:" << m_variables.size();
    for (const auto& v : m_variables) {
        qDebug() << "    -" << v.name << "type:" << v.type 
                 << "defEndOffset:" << v.defEndOffset;
    }
    qDebug() << "  Operations:" << m_operations.size();
    for (const auto& op : m_operations) {
        qDebug() << "    - type:" << (int)op.type << "array:" << op.arrayName 
                 << "i1:" << op.index1 << "i2:" << op.index2
                 << "offset:" << op.beginOffset << "-" << op.endOffset;
    }
    qDebug() << "  Has main:" << m_hasMain;
    
    return true;
}

QString AstAnalyzer::runClangAstDump(const QString& code) {
    // 创建临时源文件
    QTemporaryFile tempFile(QDir::tempPath() + "/ast_input_XXXXXX.cpp");
    if (!tempFile.open()) {
        m_lastError = "Cannot create temp file for Clang";
        return "";
    }
    tempFile.write(code.toUtf8());
    tempFile.close();
    
    // 查找Clang
    QString clangPath = findClangPath();
    
    // 调用Clang AST dump
    QStringList arguments;
    arguments << "-Xclang" << "-ast-dump=json";
    arguments << "-fsyntax-only";
    arguments << "-std=c++17";
    arguments << tempFile.fileName();
    
    QProcess process;
    process.start(clangPath, arguments);
    process.waitForFinished(15000);  // 15秒超时
    
    if (process.exitCode() != 0) {
        m_lastError = "Clang failed: " + QString::fromUtf8(process.readAllStandardError());
        return "";
    }
    
    return QString::fromUtf8(process.readAllStandardOutput());
}

void AstAnalyzer::traverseAst(const QJsonObject& node) {
    QString kind = node["kind"].toString();
    
    if (kind == "FunctionDecl") {
        // 找到函数体的 { 位置（用于后续 ParmVarDecl 的跟踪代码插入）
        QJsonObject funcRange = node["range"].toObject();
        int funcBegin = extractCharOffset(funcRange, "begin");
        int bracePos = m_sourceCode.indexOf('{', funcBegin);
        if (bracePos >= 0) {
            m_currentFunctionBodyOffset = bracePos;
        }
        
        visitFunctionDecl(node);
        // 只遍历函数体内部的节点，不继续递归到系统头文件的声明
        if (node.contains("inner")) {
            QJsonArray inner = node["inner"].toArray();
            for (const QJsonValue& child : inner) {
                if (child.isObject()) {
                    traverseAst(child.toObject());
                }
            }
        }
        
        m_currentFunctionBodyOffset = -1;  // 离开函数后重置
        return;  // 不继续外层递归，避免进入系统头文件
    }

    if (kind == "VarDecl" || kind == "ParmVarDecl") {
        // 检查该 VarDecl 是否是 binary_search/find 赋值语句
        // 例如：bool found = binary_search(...) 或 auto it = find(...)
        // 若是，在 VarDecl 层面处理，避免 CallExpr 层面生成错误的块语句
        if (kind == "VarDecl" && node.contains("inner")) {
            QJsonArray varInner = node["inner"].toArray();
            for (const QJsonValue& child : varInner) {
                QJsonObject childObj = child.toObject();
                // 穿透 ImplicitCastExpr / MaterializeTemporaryExpr 等包装层
                std::function<QJsonObject(const QJsonObject&)> unwrap =
                    [&](const QJsonObject& n) -> QJsonObject {
                    QString k = n["kind"].toString();
                    if (k == "CallExpr") return n;
                    if ((k == "ImplicitCastExpr" || k == "MaterializeTemporaryExpr" ||
                         k == "CXXConstructExpr" || k == "CXXFunctionalCastExpr" ||
                         k == "ExprWithCleanups") && n.contains("inner")) {
                        QJsonArray inn = n["inner"].toArray();
                        for (const auto& c : inn) {
                            QJsonObject r = unwrap(c.toObject());
                            if (!r.isEmpty()) return r;
                        }
                    }
                    return QJsonObject();
                };
                QJsonObject callNode = unwrap(childObj);
                if (callNode.isEmpty()) continue;

                // 提取函数名
                QString callFuncName;
                if (callNode.contains("inner")) {
                    QJsonArray callInner = callNode["inner"].toArray();
                    if (!callInner.isEmpty()) {
                        std::function<void(const QJsonObject&)> findFn =
                            [&](const QJsonObject& obj) {
                            if (obj["kind"].toString() == "DeclRefExpr") {
                                callFuncName = obj["referencedDecl"].toObject()["name"].toString();
                            } else if (obj.contains("inner")) {
                                for (const auto& c : obj["inner"].toArray()) findFn(c.toObject());
                            }
                        };
                        findFn(callInner[0].toObject());
                    }
                }

                if (callFuncName == "binary_search" || callFuncName == "find") {
                    // 记录目标变量信息，然后以 VarDecl 范围为替换范围处理 CallExpr
                    // otherArrayName: 目标变量名（如 found/it）
                    // otherIndex: 目标变量类型（如 bool/auto）
                    QString targetVarName = node["name"].toString();
                    QJsonObject typeObj = node["type"].toObject();
                    QString targetVarType = typeObj["qualType"].toString();

                    // 计算 VarDecl 语句的范围（从变量类型关键字到分号）
                    QJsonObject varRange = node["range"].toObject();
                    int varBegin = extractCharOffset(varRange, "begin");
                    // 找到语句结束的分号
                    int semiPos = m_sourceCode.indexOf(';', varBegin);
                    int varEnd = (semiPos >= 0) ? semiPos + 1 : extractCharOffset(varRange, "end") + varRange["end"].toObject()["tokLen"].toInt();

                    // 调用 visitStlBinarySearch/visitStlFind，内部会填 op
                    // 但我们需要在之后修正替换范围和目标变量信息
                    // 记录调用前的操作数量，以便找到新增的 op
                    int opCountBefore = m_operations.size();

                    QJsonArray callInner = callNode["inner"].toArray();
                    if (callFuncName == "binary_search") {
                        visitStlBinarySearch(callNode, callInner);
                    } else {
                        visitStlFind(callNode, callInner);
                    }

                    // 找到刚才添加的操作，修正其范围和目标变量信息
                    if (m_operations.size() > opCountBefore) {
                        TrackedOperation& newOp = m_operations.last();
                        newOp.beginOffset = varBegin;
                        newOp.endOffset = varEnd;
                        newOp.otherArrayName = targetVarName;  // 目标变量名
                        newOp.otherIndex = targetVarType;      // 目标变量类型
                        qDebug() << "  [VarDecl+STL] targetVar:" << targetVarName << "type:" << targetVarType
                                 << "range:" << varBegin << "-" << varEnd;
                        DBG_LOG("[VarDecl+STL] func=" << callFuncName.toStdString().c_str()
                                << " targetVar=" << targetVarName.toStdString().c_str()
                                << " type=" << targetVarType.toStdString().c_str()
                                << " range=" << varBegin << "-" << varEnd);
                    }

                    // 已处理完毕，跳过默认的 visitVarDecl 和子节点递归
                    // （不进入其 inner 的 CallExpr，避免重复记录操作）
                    goto next_sibling;
                }
            }
        }
        visitVarDecl(node);
    } else     if (kind == "CallExpr") {
        visitUserCallExpr(node);  // 先检测用户函数调用点（别名生成）
        visitCallExpr(node);
    } else if (kind == "CXXMemberCallExpr") {
        visitMemberCallExpr(node);
    } else if (kind == "BinaryOperator" || kind == "CompoundAssignOperator") {
        visitBinaryOperator(node);
    }
    
    // 递归遍历子节点（仅在函数体内部时）
    if (node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        for (const QJsonValue& child : inner) {
            if (child.isObject()) {
                traverseAst(child.toObject());
            }
        }
    }
    next_sibling:;
}

void AstAnalyzer::visitFunctionDecl(const QJsonObject& node) {
    QString name = node["name"].toString();
    if (name == "main") {
        m_hasMain = true;
    }
    
    // 收集函数参数信息（用于后续别名生成）
    QList<ParamInfo> params;
    if (node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        for (const QJsonValue& child : inner) {
            QJsonObject childObj = child.toObject();
            if (childObj["kind"].toString() == "ParmVarDecl") {
                QString paramName = childObj["name"].toString();
                QJsonObject paramType = childObj["type"].toObject();
                QString qualType = paramType["qualType"].toString();
                bool isRef = qualType.contains('&');
                ParamInfo pi;
                pi.name = paramName;
                pi.isReference = isRef;
                params.append(pi);
                DBG_LOG("[FuncParam] " << name.toStdString().c_str()
                        << " param: " << paramName.toStdString().c_str()
                        << " ref=" << isRef);
            }
        }
    }
    if (!params.isEmpty()) {
        m_functionParams[name] = params;
    }
}

void AstAnalyzer::visitVarDecl(const QJsonObject& node) {
    QString name = node["name"].toString();
    if (name.isEmpty()) return;
    
    // 过滤编译器内部变量（以双下划线开头）
    // Clang 会为 range-based for、string 字面量等生成 __range1、__str 等内部变量
    if (name.startsWith("__")) {
        return;
    }
    
    // 过滤 std::string::npos 等特殊常量
    if (name == "npos") {
        return;
    }
    
    // 判断是否是函数参数
    bool isParm = (node["kind"].toString() == "ParmVarDecl");
    
    QJsonObject typeObj = node["type"].toObject();
    QString qualType = typeObj["qualType"].toString();
    
    // 过滤 qualType 中包含 npos 的声明
    if (qualType.contains("npos")) {
        return;
    }
    
    // 对于值参数，去掉 const 修饰符以便提取类型信息
    bool isRefParam = (isParm && qualType.contains('&'));
    
    QString cleanType = qualType;
    if (isParm) {
        cleanType.replace(QRegularExpression("^const\\s+"), "");   // 去掉开头的 const
    }
    
    TrackedVariable var;
    var.name = name;
    var.isVector = false;
    var.isString = false;
    var.isArray = false;
    var.isParameter = isParm;
    var.isReferenceParam = isRefParam;
    
    // 判断类型（使用清理后的类型字符串）
    if (cleanType.contains("vector")) {
        var.isVector = true;
        var.type = cleanType;
        
        // 提取元素类型：从 vector<int, ...> 中提取 int
        QRegularExpression elemRegex(R"(vector\s*<\s*(\w+))");
        QRegularExpressionMatch match = elemRegex.match(cleanType);
        if (match.hasMatch()) {
            var.elementType = match.captured(1);
        }
        
        // 提取初始化值
        if (node.contains("inner")) {
            QJsonArray inner = node["inner"].toArray();
            for (const QJsonValue& child : inner) {
                QJsonObject childObj = child.toObject();
                if (childObj["kind"].toString() == "InitListExpr" ||
                    childObj["kind"].toString() == "CXXConstructExpr") {
                    // 从源码中提取初始化值
                    QJsonObject range = childObj["range"].toObject();
                    int begin = extractCharOffset(range, "begin");
                    int end = extractCharOffset(range, "end");
                    int tokLen = range["end"].toObject()["tokLen"].toInt();
                    var.initValues = m_sourceCode.mid(begin, end + tokLen - begin);
                    break;
                }
            }
        }
        
        // 对于函数参数，跟踪代码插入到函数体开头（{ 之后）
        if (isParm && m_currentFunctionBodyOffset >= 0) {
            var.defEndOffset = m_currentFunctionBodyOffset + 1;
        } else {
            // 在原始源码中查找变量声明语句的分号位置
            QJsonObject range = node["range"].toObject();
            int beginOff = extractCharOffset(range, "begin");
            int semiPos = m_sourceCode.indexOf(';', beginOff);
            if (semiPos > 0) {
                var.defEndOffset = semiPos + 1;
            } else {
                int endOffset = extractCharOffset(range, "end");
                int tokLen = range["end"].toObject()["tokLen"].toInt();
                var.defEndOffset = endOffset + tokLen;
            }
        }
        
        m_variables.append(var);
        
    } else if (qualType.contains("string") || qualType.contains("basic_string")) {
        var.isString = true;
        var.type = cleanType;
        var.elementType = "char";
        
        // 对于函数参数，跟踪代码插入到函数体开头（{ 之后）
        if (isParm && m_currentFunctionBodyOffset >= 0) {
            var.defEndOffset = m_currentFunctionBodyOffset + 1;
        } else {
            QJsonObject range2 = node["range"].toObject();
            int beginOff2 = extractCharOffset(range2, "begin");
            int semiPos2 = m_sourceCode.indexOf(';', beginOff2);
            if (semiPos2 > 0) {
                var.defEndOffset = semiPos2 + 1;
            } else {
                int endOffset2 = extractCharOffset(range2, "end");
                int tokLen2 = range2["end"].toObject()["tokLen"].toInt();
                var.defEndOffset = endOffset2 + tokLen2;
            }
        }
        
        m_variables.append(var);
        
    } else if (cleanType.contains("int [") || cleanType.contains("int[]") ||
               cleanType.contains("float [") || cleanType.contains("double [")) {
        var.isArray = true;
        var.type = cleanType;
        
        // 提取元素类型
        QRegularExpression elemRegex(R"((int|float|double|char)\s*\[)");
        QRegularExpressionMatch match = elemRegex.match(cleanType);
        if (match.hasMatch()) {
            var.elementType = match.captured(1);
        }
        
        // 提取初始化值
        if (node.contains("inner")) {
            QJsonArray inner = node["inner"].toArray();
            for (const QJsonValue& child : inner) {
                QJsonObject childObj = child.toObject();
                if (childObj["kind"].toString() == "InitListExpr") {
                    QJsonObject range = childObj["range"].toObject();
                    int begin = extractCharOffset(range, "begin");
                    int end = extractCharOffset(range, "end");
                    int tokLen = range["end"].toObject()["tokLen"].toInt();
                    var.initValues = m_sourceCode.mid(begin, end + tokLen - begin);
                    break;
                }
            }
        }
        
        // 对于函数参数，跟踪代码插入到函数体开头（{ 之后）
        if (isParm && m_currentFunctionBodyOffset >= 0) {
            var.defEndOffset = m_currentFunctionBodyOffset + 1;
        } else {
            // 在原始源码中查找变量声明语句的分号位置
            QJsonObject range3 = node["range"].toObject();
            int beginOff3 = extractCharOffset(range3, "begin");
            int semiPos3 = m_sourceCode.indexOf(';', beginOff3);
            if (semiPos3 > 0) {
                var.defEndOffset = semiPos3 + 1;
            } else {
                int endOffset3 = extractCharOffset(range3, "end");
                int tokLen3 = range3["end"].toObject()["tokLen"].toInt();
                var.defEndOffset = endOffset3 + tokLen3;
            }
        }
        
        m_variables.append(var);
    }
}

void AstAnalyzer::visitCallExpr(const QJsonObject& node) {
    if (!node.contains("inner")) return;
    
    QJsonArray inner = node["inner"].toArray();
    if (inner.isEmpty()) return;
    
    // 获取调用函数名
    QString calledFuncName;
    QJsonObject range = node["range"].toObject();
    int dbgBegin = extractCharOffset(range, "begin");
    qDebug() << "[visitCallExpr] node at offset:" << dbgBegin << "inner size:" << inner.size();
    DBG_LOG("[visitCallExpr] node at offset=" << dbgBegin << " innerSize=" << inner.size());
    QJsonObject firstChild = inner[0].toObject();
    
    std::function<void(const QJsonObject&)> findFuncName = 
        [&](const QJsonObject& obj) {
        if (obj["kind"].toString() == "DeclRefExpr") {
            QJsonObject refDecl = obj["referencedDecl"].toObject();
            calledFuncName = refDecl["name"].toString();
        } else if (obj.contains("inner")) {
            QJsonArray childArray = obj["inner"].toArray();
            for (const QJsonValue& child : childArray) {
                findFuncName(child.toObject());
            }
        }
    };
    findFuncName(firstChild);
    qDebug() << "[visitCallExpr] calledFuncName:" << calledFuncName;
    DBG_LOG("[visitCallExpr] calledFuncName=" << calledFuncName.toStdString().c_str());
    
    // ==================== STL reverse 检测 ====================
    if (calledFuncName == "reverse") {
        visitStlReverse(node, inner);
        return;
    }
    
    // ==================== STL fill 检测 ====================
    if (calledFuncName == "fill") {
        visitStlFill(node, inner);
        return;
    }
    
    // ==================== STL sort 检测 ====================
    if (calledFuncName == "sort") {
        visitStlSort(node, inner);
        return;
    }

    // ==================== STL binary_search ====================
    if (calledFuncName == "binary_search") {
        visitStlBinarySearch(node, inner);
        return;
    }

    // ==================== STL find ====================
    if (calledFuncName == "find") {
        visitStlFind(node, inner);
        return;
    }

    // ==================== STL copy ====================
    if (calledFuncName == "copy") {
        visitStlCopy(node, inner);
        return;
    }

    // ==================== STL remove ====================
    if (calledFuncName == "remove") {
        visitStlRemove(node, inner);
        return;
    }

    // ==================== STL replace ====================
    if (calledFuncName == "replace") {
        visitStlReplace(node, inner);
        return;
    }

    // ==================== STL rotate ====================
    if (calledFuncName == "rotate") {
        visitStlRotate(node, inner);
        return;
    }

    // ==================== swap 检测 ====================
    if (calledFuncName != "swap") return;
    
    // swap有两个参数
    if (inner.size() < 3) return;

    // 先尝试容器级 swap: swap(v1, v2) 两个参数都是 tracked variable 名称
    {
        QString n1 = getReferencedName(inner[1].toObject());
        QString n2 = getReferencedName(inner[2].toObject());
        if (!n1.isEmpty() && !n2.isEmpty() && n1 != n2
            && isTrackedVariable(n1) && isTrackedVariable(n2)) {
            visitStlContainerSwap(node, inner);
            return;
        }
    }

    // 元素级 swap: swap(v[i], v[j])
    for (const auto& var : m_variables) {
        QString varName = var.name;
        
        bool arg1IsSubscript = isArraySubscriptOn(inner[1].toObject(), varName);
        bool arg2IsSubscript = isArraySubscriptOn(inner[2].toObject(), varName);
        
        if (arg1IsSubscript && arg2IsSubscript) {
            TrackedOperation op;
            op.type = TrackedOperation::Swap;
            op.arrayName = varName;
            op.index1 = getSubscriptIndex(inner[1].toObject());
            op.index2 = getSubscriptIndex(inner[2].toObject());
            
            QJsonObject range = node["range"].toObject();
            op.beginOffset = extractCharOffset(range, "begin");
            int endOffset = extractCharOffset(range, "end");
            int tokLen = range["end"].toObject()["tokLen"].toInt();
            op.endOffset = endOffset + tokLen;
            
            qDebug() << "  [Swap] array:" << op.arrayName << "i1:" << op.index1 << "i2:" << op.index2;
            m_operations.append(op);
            return;
        }
    }
}

void AstAnalyzer::visitUserCallExpr(const QJsonObject& node) {
    // 检测用户自定义函数的调用点（用于生成别名 push/pop）
    if (!node.contains("inner")) return;
    QJsonArray inner = node["inner"].toArray();
    if (inner.isEmpty()) return;

    // 提取被调用函数名
    QString calledFuncName;
    std::function<void(const QJsonObject&)> findFn =
        [&](const QJsonObject& obj) {
        if (obj["kind"].toString() == "DeclRefExpr") {
            calledFuncName = obj["referencedDecl"].toObject()["name"].toString();
        } else if (obj.contains("inner")) {
            for (const auto& c : obj["inner"].toArray()) {
                findFn(c.toObject());
                if (!calledFuncName.isEmpty()) return;
            }
        }
    };
    findFn(inner[0].toObject());
    
    // 只关心有引用参数的用户函数
    if (calledFuncName.isEmpty() || !m_functionParams.contains(calledFuncName)) return;
    
    const auto& params = m_functionParams[calledFuncName];
    bool hasRefParam = false;
    for (const auto& p : params) {
        if (p.isReference) { hasRefParam = true; break; }
    }
    if (!hasRefParam) return;
    
    // 收集实参表达式（跳过第一个 inner 元素，它是函数名引用）
    QStringList argExprs;
    for (int i = 1; i < inner.size(); i++) {
        QJsonObject argNode = inner[i].toObject();
        // 尝试直接获取变量名
        QString argExpr = getReferencedName(argNode);
        if (argExpr.isEmpty()) {
            // 回退：从源码中获取完整表达式文本
            QJsonObject argRange = argNode["range"].toObject();
            int aBegin = extractCharOffset(argRange, "begin");
            int aEnd = extractCharOffset(argRange, "end");
            int aTokLen = argRange["end"].toObject()["tokLen"].toInt();
            argExpr = m_sourceCode.mid(aBegin, aEnd + aTokLen - aBegin);
        }
        argExprs.append(argExpr);
    }
    
    // 记录调用点
    QJsonObject range = node["range"].toObject();
    int beginOff = extractCharOffset(range, "begin");
    int endOff = extractCharOffset(range, "end");
    int tokLen = range["end"].toObject()["tokLen"].toInt();
    
    // 扩展 endOffset 到语句末尾的分号
    int semiPos = m_sourceCode.indexOf(';', endOff + tokLen - 1);
    int actualEnd = (semiPos >= 0) ? semiPos + 1 : endOff + tokLen;
    
    CallSiteInfo cs;
    cs.funcName = calledFuncName;
    cs.argExprs = argExprs;
    cs.beginOffset = beginOff;
    cs.endOffset = actualEnd;
    m_callSites.append(cs);
    
    DBG_LOG("[CallSite] " << calledFuncName.toStdString().c_str()
            << " offset=" << beginOff << "-" << actualEnd
            << " args=" << argExprs.join(",").toStdString().c_str());
}

void AstAnalyzer::visitMemberCallExpr(const QJsonObject& node) {
    // 处理 CXXMemberCallExpr（如 v.push_back(x)、v.pop_back() 等）
    // CXXMemberCallExpr 的 inner 结构：
    //   [0]: MemberExpr（方法引用，如 push_back）
    //   [1+]: 显式参数 / 隐式 this
    if (!node.contains("inner")) return;
    QJsonArray inner = node["inner"].toArray();
    if (inner.isEmpty()) return;

    // 提取方法名（从 MemberExpr 节点）
    QString memberName;
    std::function<void(const QJsonObject&)> findMemberName =
        [&](const QJsonObject& obj) {
        if (obj["kind"].toString() == "MemberExpr") {
            memberName = obj["name"].toString();
        } else if (obj.contains("inner")) {
            for (const auto& child : obj["inner"].toArray()) {
                findMemberName(child.toObject());
                if (!memberName.isEmpty()) return;
            }
        }
    };
    findMemberName(inner[0].toObject());
    if (memberName.isEmpty()) return;

    // 只处理容器操作
    static QStringList containerFuncs = {"push_back", "pop_back", "insert", "erase", "clear", "resize",
                                           "emplace_back", "at"};
    if (!containerFuncs.contains(memberName)) return;

    // 获取容器变量名
    QString varName = extractContainerObjectName(node);
    if (varName.isEmpty() || !isTrackedVariable(varName)) return;

    TrackedOperation op;
    if (memberName == "push_back")      op.type = TrackedOperation::StlPushBack;
    else if (memberName == "pop_back")  op.type = TrackedOperation::StlPopBack;
    else if (memberName == "insert")    op.type = TrackedOperation::StlInsert;
    else if (memberName == "erase")     op.type = TrackedOperation::StlErase;
    else if (memberName == "clear")     op.type = TrackedOperation::StlClear;
    else if (memberName == "resize")    op.type = TrackedOperation::StlVecResize;
    else if (memberName == "emplace_back") op.type = TrackedOperation::StlEmplaceBack;
    else if (memberName == "at")        op.type = TrackedOperation::StlAt;
    op.arrayName = varName;

    // 提取参数：
    // 此 Clang 版本将 "this" 对象嵌套在 MemberExpr（inner[0]）内部（如
    // DeclRefExpr(vv)），不作为独立的 inner child。因此 inner[1] 直接就是
    // 第一个显式参数（可能包裹在 ImplicitCastExpr/MaterializeTemporaryExpr 中）。
    // extractSourceExpr 会递归穿透这些包装节点提取实际表达式。
    //
    // 注意：erase/insert 的第一个参数是复杂迭代器表达式（如 vv.end()-1），
    // 目前不做自动提取，留空让 CodeInserter 用近似 fallback。
    if (memberName == "push_back") {
        // push_back(val): inner[1] = 值表达式
        if (inner.size() > 1) {
            op.index1 = extractSourceExpr(inner[1].toObject());
        }
    } else if (memberName == "emplace_back") {
        // emplace_back(args...): inner[1] = 构造参数
        if (inner.size() > 1) {
            op.index1 = extractSourceExpr(inner[1].toObject());
        }
    } else if (memberName == "at") {
        // at(i): inner[1] = 索引表达式
        if (inner.size() > 1) {
            op.index1 = extractSourceExpr(inner[1].toObject());
        }
    } else if (memberName == "insert") {
        // insert(pos, val): inner[1]=iterator(如 begin()+2), inner[2]=value
        // 也支持 insert(pos, count, val): inner[1]=pos, inner[2]=count, inner[3]=val
        if (inner.size() > 1) {
            // 尝试 AST 方式提取迭代器偏移
            op.index1 = extractIteratorOffset(inner[1].toObject(), varName);
            if (op.index1.isEmpty()) {
                // AST 穿透失败 → 从源码文本中 parsing 偏移量
                // 例如：vv.begin()+2 → 提取 "2" ; vv.begin() → "0"
                QString rawPos = extractSourceExpr(inner[1].toObject());
                if (rawPos.isEmpty() && inner.size() > 1) {
                    // extractSourceExpr 也失败 → 直接从原始源码截取
                    QJsonObject posRange = inner[1].toObject()["range"].toObject();
                    int pBegin = extractCharOffset(posRange, "begin");
                    int pEnd = extractCharOffset(posRange, "end");
                    int pTokLen = posRange["end"].toObject()["tokLen"].toInt();
                    rawPos = m_sourceCode.mid(pBegin, pEnd + pTokLen - pBegin);
                }
                if (!rawPos.isEmpty()) {
                    // 匹配 begin()/rbegin() 后跟 +N 或 -N 的模式
                    QRegularExpression re(R"((?:begin|rbegin)\(\s*\)\s*\+\s*(\S+))");
                    QRegularExpressionMatch m = re.match(rawPos);
                    if (m.hasMatch()) {
                        op.index1 = m.captured(1);  // 如 "2"
                    } else {
                        // 匹配 end()/rend() 后跟 -N 的模式
                        QRegularExpression reEnd(R"((?:end|rend)\(\s*\)\s*-\s*(\S+))");
                        QRegularExpressionMatch me = reEnd.match(rawPos);
                        if (me.hasMatch()) {
                            op.index1 = varName + ".size() - " + me.captured(1);
                        }
                    }
                }
            }
        }
        if (inner.size() > 3) {
            // insert(pos, count, val): count 存 index2, val 存 opcode
            op.index2 = extractSourceExpr(inner[2].toObject());
            op.opcode = extractSourceExpr(inner[3].toObject());
        } else if (inner.size() > 2) {
            // insert(pos, val): val 存 index2
            op.index2 = extractSourceExpr(inner[2].toObject());
        }
    } else if (memberName == "resize") {
        // resize(n) 或 resize(n, val)
        // inner[1] = 新大小表达式, inner[2](可选) = 填充值
        if (inner.size() > 1) {
            op.index1 = extractSourceExpr(inner[1].toObject());  // 新大小
        }
        if (inner.size() > 2) {
            op.index2 = extractSourceExpr(inner[2].toObject());  // 填充值（可选）
        }
    } else if (memberName == "erase") {
        // erase(pos) 或 erase(first, last)
        // 使用与 insert 相同的迭代器偏移提取逻辑
        if (inner.size() > 1) {
            op.index1 = extractIteratorOffset(inner[1].toObject(), varName);
            if (op.index1.isEmpty()) {
                QString rawPos = extractSourceExpr(inner[1].toObject());
                if (rawPos.isEmpty()) {
                    QJsonObject posRange = inner[1].toObject()["range"].toObject();
                    int pBegin = extractCharOffset(posRange, "begin");
                    int pEnd = extractCharOffset(posRange, "end");
                    int pTokLen = posRange["end"].toObject()["tokLen"].toInt();
                    rawPos = m_sourceCode.mid(pBegin, pEnd + pTokLen - pBegin);
                }
                if (!rawPos.isEmpty()) {
                    QRegularExpression re(R"((?:begin|rbegin)\(\s*\)\s*\+\s*(\S+))");
                    QRegularExpressionMatch m = re.match(rawPos);
                    if (m.hasMatch()) {
                        op.index1 = m.captured(1);
                    } else {
                        QRegularExpression reEnd(R"((?:end|rend)\(\s*\)\s*-\s*(\S+))");
                        QRegularExpressionMatch me = reEnd.match(rawPos);
                        if (me.hasMatch()) {
                            op.index1 = varName + ".size() - " + me.captured(1);
                        }
                    }
                }
            }
        }
        // erase(first, last): 提取第二个迭代器参数
        if (inner.size() > 2) {
            op.index2 = extractIteratorOffset(inner[2].toObject(), varName);
            if (op.index2.isEmpty()) {
                QString rawPos = extractSourceExpr(inner[2].toObject());
                if (rawPos.isEmpty()) {
                    QJsonObject posRange = inner[2].toObject()["range"].toObject();
                    int pBegin = extractCharOffset(posRange, "begin");
                    int pEnd = extractCharOffset(posRange, "end");
                    int pTokLen = posRange["end"].toObject()["tokLen"].toInt();
                    rawPos = m_sourceCode.mid(pBegin, pEnd + pTokLen - pBegin);
                }
                if (!rawPos.isEmpty()) {
                    QRegularExpression re(R"((?:begin|rbegin)\(\s*\)\s*\+\s*(\S+))");
                    QRegularExpressionMatch m = re.match(rawPos);
                    if (m.hasMatch()) {
                        op.index2 = m.captured(1);
                    } else {
                        QRegularExpression reEnd(R"((?:end|rend)\(\s*\)\s*-\s*(\S+))");
                        QRegularExpressionMatch me = reEnd.match(rawPos);
                        if (me.hasMatch()) {
                            op.index2 = varName + ".size() - " + me.captured(1);
                        }
                    }
                }
            }
        }
    }

    QJsonObject range = node["range"].toObject();
    op.beginOffset = extractCharOffset(range, "begin");
    int endOffset = extractCharOffset(range, "end");
    int tokLen = range["end"].toObject()["tokLen"].toInt();
    op.endOffset = endOffset + tokLen;

    qDebug() << "  [STL Container]" << memberName << "array:" << op.arrayName
             << "idx1:" << op.index1 << "idx2:" << op.index2;
    m_operations.append(op);
}

void AstAnalyzer::visitStlReverse(const QJsonObject& node, const QJsonArray& inner) {
    // reverse 需要 2 个参数（func + arg1 + arg2）
    if (inner.size() < 3) return;
    
    // 检查两个参数是否都是已追踪 vector 的 .begin() / .end() 调用
    // 支持形式：reverse(arr.begin(), arr.end()) / reverse(arr.begin() + 1, arr.end() - 1)
    
    for (const auto& var : m_variables) {
        if (!var.isVector && !var.isArray) continue;
        
        // 检查参数1是否引用了该变量
        QString arg1Name = getReferencedName(inner[1].toObject());
        if (arg1Name.isEmpty()) {
            // 可能在 CXXMemberCallExpr 内部
            arg1Name = extractVectorNameFromIterator(inner[1].toObject());
        }
        
        // 检查参数2
        QString arg2Name = getReferencedName(inner[2].toObject());
        if (arg2Name.isEmpty()) {
            arg2Name = extractVectorNameFromIterator(inner[2].toObject());
        }
        
        if (arg1Name != var.name || arg2Name != var.name) continue;
        
        // 确认参数确实是 begin()/end() 相关调用
        if (!isIteratorCall(inner[1].toObject()) || !isIteratorCall(inner[2].toObject()))
            continue;
        
        TrackedOperation op;
        op.type = TrackedOperation::StlReverse;
        op.arrayName = var.name;
        
        // 提取迭代器偏移表达式
        op.index1 = extractIteratorOffset(inner[1].toObject(), var.name);  // begin 偏移
        op.index2 = extractIteratorOffset(inner[2].toObject(), var.name);  // end 偏移
        
        QJsonObject range = node["range"].toObject();
        op.beginOffset = extractCharOffset(range, "begin");
        int endOffset = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        op.endOffset = endOffset + tokLen;
        
        qDebug() << "  [StlReverse] array:" << op.arrayName
                 << "beginOff:" << op.index1 << "endOff:" << op.index2;
        m_operations.append(op);
        return;
    }
}

void AstAnalyzer::visitStlFill(const QJsonObject& node, const QJsonArray& inner) {
    // fill(begin, end, value) 需要 4 个 inner 节点：函数引用 + 3 个参数
    // inner[0]: DeclRefExpr/ImplicitCastExpr(fill)
    // inner[1]: begin 迭代器参数
    // inner[2]: end 迭代器参数
    // inner[3]: 填充值表达式
    qDebug() << "[visitStlFill] ENTERED, inner size:" << inner.size();
    DBG_LOG("[visitStlFill] ENTERED, innerSize=" << inner.size());
    if (inner.size() < 4) { qDebug() << "[visitStlFill] REJECTED: inner.size() < 4"; DBG_LOG("[visitStlFill] REJECTED: inner.size() < 4"); return; }

    // 检查两个迭代器参数是否引用了同一个已追踪变量
    DBG_LOG("[visitStlFill] m_variables count=" << m_variables.size());
    for (const auto& var : m_variables) {
        DBG_LOG("[visitStlFill] checking var: name=" << var.name.toStdString().c_str() << " isVector=" << var.isVector << " isArray=" << var.isArray);
        if (!var.isVector && !var.isArray) continue;

        QString kind1 = inner[1].toObject()["kind"].toString();
        QString kind2 = inner[2].toObject()["kind"].toString();
        qDebug() << "[visitStlFill] arg1 kind:" << kind1 << "arg2 kind:" << kind2;
        DBG_LOG("[visitStlFill] arg1 kind=" << kind1.toStdString().c_str() << " arg2 kind=" << kind2.toStdString().c_str());
        
        // 提取参数1(begin)和参数2(end)中的变量名
        QString arg1Name = getReferencedName(inner[1].toObject());
        if (arg1Name.isEmpty())
            arg1Name = extractVectorNameFromIterator(inner[1].toObject());

        QString arg2Name = getReferencedName(inner[2].toObject());
        if (arg2Name.isEmpty())
            arg2Name = extractVectorNameFromIterator(inner[2].toObject());

        qDebug() << "[visitStlFill] var.name:" << var.name << "arg1Name:" << arg1Name << "arg2Name:" << arg2Name;
        DBG_LOG("[visitStlFill] varName=" << var.name.toStdString().c_str() << " arg1Name=" << arg1Name.toStdString().c_str() << " arg2Name=" << arg2Name.toStdString().c_str());
        DBG_LOG("[visitStlFill] extractIteratorOffset arg1=" << extractIteratorOffset(inner[1].toObject(), var.name).toStdString().c_str());
        DBG_LOG("[visitStlFill] extractIteratorOffset arg2=" << extractIteratorOffset(inner[2].toObject(), var.name).toStdString().c_str());
        
        if (arg1Name != var.name || arg2Name != var.name) { qDebug() << "[visitStlFill] SKIP: name mismatch"; DBG_LOG("[visitStlFill] SKIP: name mismatch"); continue; }

        // 至少 begin 参数需是迭代器调用
        bool isIter1 = isIteratorCall(inner[1].toObject());
        bool isIter2 = isIteratorCall(inner[2].toObject());
        qDebug() << "[visitStlFill] isIter1:" << isIter1 << "isIter2:" << isIter2;
        DBG_LOG("[visitStlFill] isIter1=" << (isIter1?"true":"false") << " isIter2=" << (isIter2?"true":"false"));
        if (!isIter1 && !isIter2) { qDebug() << "[visitStlFill] SKIP: not iterator call"; DBG_LOG("[visitStlFill] SKIP: not iterator call"); continue; }

        DBG_LOG("[visitStlFill] MATCHED! Proceeding to create operation.");
        TrackedOperation op;
        op.type = TrackedOperation::StlFill;
        op.arrayName = var.name;

        // 提取 begin/end 偏移表达式（同 reverse）
        op.index1 = extractIteratorOffset(inner[1].toObject(), var.name);  // begin 偏移
        op.index2 = extractIteratorOffset(inner[2].toObject(), var.name);  // end 偏移

        // 提取第三个参数（填充值），穿透包装节点
        op.opcode = extractSourceExpr(inner[3].toObject());

        QJsonObject range = node["range"].toObject();
        op.beginOffset = extractCharOffset(range, "begin");
        int endOffset = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        op.endOffset = endOffset + tokLen;

        qDebug() << "  [StlFill] array:" << op.arrayName
                 << "beginOff:" << op.index1 << "endOff:" << op.index2
                 << "val:" << op.opcode;
        DBG_LOG("  [StlFill] ACCEPTED: array=" << op.arrayName.toStdString().c_str()
                << " beginOff=" << op.index1.toStdString().c_str()
                << " endOff=" << op.index2.toStdString().c_str()
                << " val=" << op.opcode.toStdString().c_str()
                << " beginOffset=" << op.beginOffset
                << " endOffset=" << op.endOffset);
        m_operations.append(op);
        return;
    }
}

void AstAnalyzer::visitStlSort(const QJsonObject& node, const QJsonArray& inner) {
    // sort(begin, end) 或 sort(begin, end, comparator)
    // inner[0]: 函数引用
    // inner[1]: begin 迭代器
    // inner[2]: end 迭代器
    // inner[3] (可选): 比较器 (如 greater<int>())
    if (inner.size() < 3) return;

    // 检查两个迭代器参数是否引用了同一个已追踪变量
    for (const auto& var : m_variables) {
        if (!var.isVector && !var.isArray) continue;

        // 提取参数1(begin)和参数2(end)中的变量名
        QString arg1Name = getReferencedName(inner[1].toObject());
        if (arg1Name.isEmpty())
            arg1Name = extractVectorNameFromIterator(inner[1].toObject());

        QString arg2Name = getReferencedName(inner[2].toObject());
        if (arg2Name.isEmpty())
            arg2Name = extractVectorNameFromIterator(inner[2].toObject());

        if (arg1Name != var.name || arg2Name != var.name) continue;

        // 至少 begin 参数需是迭代器调用
        if (!isIteratorCall(inner[1].toObject()) && !isIteratorCall(inner[2].toObject()))
            continue;

        TrackedOperation op;
        op.type = TrackedOperation::StlSort;
        op.arrayName = var.name;

        // 提取 begin/end 偏移表达式
        op.index1 = extractIteratorOffset(inner[1].toObject(), var.name);  // begin 偏移
        op.index2 = extractIteratorOffset(inner[2].toObject(), var.name);  // end 偏移

        // 提取比较器（第三个参数，可选）
        // 识别：greater<int>() → 降序, less<int>() → 升序, 省略 → 升序
        if (inner.size() > 3) {
            // 提取比较器源码文本（如 "greater<int>()", "less<int>()", lambda 等）
            op.opcode = extractSourceExpr(inner[3].toObject());
            if (op.opcode.isEmpty()) {
                // 递归穿透包装节点尝试提取
                op.opcode = extractSourceExpr(inner[3].toObject());
            }
        }
        // 如果 opcode 为空，默认为升序（less）

        QJsonObject range = node["range"].toObject();
        op.beginOffset = extractCharOffset(range, "begin");
        int endOffset = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        op.endOffset = endOffset + tokLen;

        qDebug() << "  [StlSort] array:" << op.arrayName
                 << "beginOff:" << op.index1 << "endOff:" << op.index2
                 << "cmp:" << op.opcode;
        m_operations.append(op);
        return;
    }
}

// ==================== STL binary_search ====================
void AstAnalyzer::visitStlBinarySearch(const QJsonObject& node, const QJsonArray& inner) {
    if (inner.size() < 4) return;

    for (const auto& var : m_variables) {
        if (!var.isVector && !var.isArray) continue;

        QString arg1Name = getReferencedName(inner[1].toObject());
        if (arg1Name.isEmpty()) arg1Name = extractVectorNameFromIterator(inner[1].toObject());
        QString arg2Name = getReferencedName(inner[2].toObject());
        if (arg2Name.isEmpty()) arg2Name = extractVectorNameFromIterator(inner[2].toObject());

        if (arg1Name != var.name || arg2Name != var.name) continue;
        if (!isIteratorCall(inner[1].toObject()) && !isIteratorCall(inner[2].toObject()))
            continue;

        TrackedOperation op;
        op.type = TrackedOperation::StlBinarySearch;
        op.arrayName = var.name;
        op.index1 = extractIteratorOffset(inner[1].toObject(), var.name);
        op.index2 = extractIteratorOffset(inner[2].toObject(), var.name);
        op.opcode = extractSourceExpr(inner[3].toObject());

        QJsonObject range = node["range"].toObject();
        op.beginOffset = extractCharOffset(range, "begin");
        int endOffset = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        op.endOffset = endOffset + tokLen;

        // binary_search 返回 bool，不演示可视化，不进行赋值上下文检测
        // detectAssignmentContext(op, m_sourceCode);  // 已禁用：binary_search 不做可视化

        qDebug() << "  [StlBinarySearch] array:" << op.arrayName
                 << "begin:" << op.index1 << "end:" << op.index2
                 << "val:" << op.opcode;
        m_operations.append(op);
        return;
    }
}

// ==================== STL find ====================
void AstAnalyzer::visitStlFind(const QJsonObject& node, const QJsonArray& inner) {
    if (inner.size() < 4) return;

    for (const auto& var : m_variables) {
        if (!var.isVector && !var.isArray) continue;

        QString arg1Name = getReferencedName(inner[1].toObject());
        if (arg1Name.isEmpty()) arg1Name = extractVectorNameFromIterator(inner[1].toObject());
        QString arg2Name = getReferencedName(inner[2].toObject());
        if (arg2Name.isEmpty()) arg2Name = extractVectorNameFromIterator(inner[2].toObject());

        if (arg1Name != var.name || arg2Name != var.name) continue;
        if (!isIteratorCall(inner[1].toObject()) && !isIteratorCall(inner[2].toObject()))
            continue;

        TrackedOperation op;
        op.type = TrackedOperation::StlFind;
        op.arrayName = var.name;
        op.index1 = extractIteratorOffset(inner[1].toObject(), var.name);
        op.index2 = extractIteratorOffset(inner[2].toObject(), var.name);
        op.opcode = extractSourceExpr(inner[3].toObject());

        QJsonObject range = node["range"].toObject();
        op.beginOffset = extractCharOffset(range, "begin");
        int endOffset = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        op.endOffset = endOffset + tokLen;

        // 赋值上下文检测：若 find(...) 前面是 "X = "，扩展替换范围到整个声明语句
        detectAssignmentContext(op, m_sourceCode);

        qDebug() << "  [StlFind] array:" << op.arrayName
                 << "begin:" << op.index1 << "end:" << op.index2
                 << "val:" << op.opcode;
        m_operations.append(op);
        return;
    }
}

// ==================== 赋值上下文检测 ====================
// 当 binary_search/find 出现在赋值语句中（如 bool found = binary_search(...)），
// 需要将替换范围从 CallExpr 扩展到整个声明语句，并记录目标变量名/类型。
void AstAnalyzer::detectAssignmentContext(TrackedOperation& op, const QString& sourceCode) {
    // 从 beginOffset 向前扫描，跳过空白，查找 '=' 号
    int pos = op.beginOffset - 1;
    while (pos > 0 && (sourceCode[pos] == ' ' || sourceCode[pos] == '\t'))
        pos--;
    if (pos <= 0 || sourceCode[pos] != '=') return;  // 不是赋值上下文
    
    // 找到 '='，继续向前跳过空白，找到变量名结束位置
    pos--;
    while (pos > 0 && (sourceCode[pos] == ' ' || sourceCode[pos] == '\t'))
        pos--;
    
    // 提取变量名（从当前位置向前直到非标识符字符）
    int varNameEnd = pos + 1;
    while (pos > 0 && (sourceCode[pos].isLetterOrNumber() || sourceCode[pos] == '_'))
        pos--;
    int varNameStart = pos + 1;
    
    QString targetVarName = sourceCode.mid(varNameStart, varNameEnd - varNameStart);
    if (targetVarName.isEmpty()) return;
    
    // 向前找类型的开始位置
    while (pos > 0 && (sourceCode[pos] == ' ' || sourceCode[pos] == '\t'))
        pos--;
    
    // 从这位置继续向前，找到声明语句的开始（行首 或 前一个 ';'/'{' 之后）
    // 简单策略：回退到行首（或前一个语句结束位置）
    int stmtBegin = pos + 1;
    while (stmtBegin > 0) {
        QChar c = sourceCode[stmtBegin - 1];
        if (c == '\n' || c == ';' || c == '{' || c == '}') break;
        stmtBegin--;
    }
    // 跳过前导空白
    while (stmtBegin < varNameStart && (sourceCode[stmtBegin] == ' ' || sourceCode[stmtBegin] == '\t'))
        stmtBegin++;
    
    // 提取类型（从 stmtBegin 到 varNameStart 之间的文本）
    QString targetVarType = sourceCode.mid(stmtBegin, varNameStart - stmtBegin).trimmed();
    if (targetVarType.isEmpty()) return;
    
    // 找到语句结束的分号
    int semiPos = sourceCode.indexOf(';', op.endOffset);
    int varEnd = (semiPos >= 0) ? semiPos + 1 : op.endOffset;
    
    // 修正 op 的范围和目标变量信息
    op.beginOffset = stmtBegin;
    op.endOffset = varEnd;
    op.otherArrayName = targetVarName;
    op.otherIndex = targetVarType;
    
    qDebug() << "  [detectAssignment] func at offset" << op.beginOffset 
             << "-> varName:" << targetVarName << "type:" << targetVarType
             << "new range:" << op.beginOffset << "-" << op.endOffset;
    DBG_LOG("[detectAssignment] varName=" << targetVarName.toStdString().c_str()
            << " type=" << targetVarType.toStdString().c_str()
            << " range=" << op.beginOffset << "-" << op.endOffset);
}

// ==================== STL copy (跨容器) ====================
void AstAnalyzer::visitStlCopy(const QJsonObject& node, const QJsonArray& inner) {
    if (inner.size() < 4) return;

    QString srcBeginName = extractVectorNameFromIterator(inner[1].toObject());
    if (srcBeginName.isEmpty()) srcBeginName = getReferencedName(inner[1].toObject());
    QString srcEndName = extractVectorNameFromIterator(inner[2].toObject());
    if (srcEndName.isEmpty()) srcEndName = getReferencedName(inner[2].toObject());
    QString dstBeginName = extractVectorNameFromIterator(inner[3].toObject());
    if (dstBeginName.isEmpty()) dstBeginName = getReferencedName(inner[3].toObject());

    if (srcBeginName.isEmpty() || dstBeginName.isEmpty()) return;
    if (srcBeginName != srcEndName) return;
    if (!isTrackedVariable(srcBeginName) || !isTrackedVariable(dstBeginName)) return;

    TrackedOperation op;
    op.type = TrackedOperation::StlCopy;
    op.arrayName = srcBeginName;
    op.otherArrayName = dstBeginName;
    op.index1 = extractIteratorOffset(inner[1].toObject(), srcBeginName);
    op.index2 = extractIteratorOffset(inner[2].toObject(), srcBeginName);
    op.otherIndex = extractIteratorOffset(inner[3].toObject(), dstBeginName);

    QJsonObject range = node["range"].toObject();
    op.beginOffset = extractCharOffset(range, "begin");
    int endOffset = extractCharOffset(range, "end");
    int tokLen = range["end"].toObject()["tokLen"].toInt();
    op.endOffset = endOffset + tokLen;

    qDebug() << "  [StlCopy] src:" << op.arrayName << "(" << op.index1 << "-" << op.index2 << ")"
             << "dst:" << op.otherArrayName << "(" << op.otherIndex << ")";
    m_operations.append(op);
}

// ==================== STL container swap ====================
void AstAnalyzer::visitStlContainerSwap(const QJsonObject& node, const QJsonArray& inner) {
    if (inner.size() < 3) return;

    QString name1 = getReferencedName(inner[1].toObject());
    QString name2 = getReferencedName(inner[2].toObject());
    if (!isTrackedVariable(name1) || !isTrackedVariable(name2)) return;

    TrackedOperation op;
    op.type = TrackedOperation::StlContainerSwap;
    op.arrayName = name1;
    op.otherArrayName = name2;

    QJsonObject range = node["range"].toObject();
    op.beginOffset = extractCharOffset(range, "begin");
    int endOffset = extractCharOffset(range, "end");
    int tokLen = range["end"].toObject()["tokLen"].toInt();
    op.endOffset = endOffset + tokLen;

    qDebug() << "  [StlContainerSwap]" << name1 << "<->" << name2;
    m_operations.append(op);
}

// ==================== STL remove ====================
void AstAnalyzer::visitStlRemove(const QJsonObject& node, const QJsonArray& inner) {
    if (inner.size() < 4) return;

    for (const auto& var : m_variables) {
        if (!var.isVector && !var.isArray) continue;

        QString arg1Name = getReferencedName(inner[1].toObject());
        if (arg1Name.isEmpty()) arg1Name = extractVectorNameFromIterator(inner[1].toObject());
        QString arg2Name = getReferencedName(inner[2].toObject());
        if (arg2Name.isEmpty()) arg2Name = extractVectorNameFromIterator(inner[2].toObject());

        if (arg1Name != var.name || arg2Name != var.name) continue;
        if (!isIteratorCall(inner[1].toObject()) && !isIteratorCall(inner[2].toObject()))
            continue;

        TrackedOperation op;
        op.type = TrackedOperation::StlRemove;
        op.arrayName = var.name;
        op.index1 = extractIteratorOffset(inner[1].toObject(), var.name);
        op.index2 = extractIteratorOffset(inner[2].toObject(), var.name);
        op.opcode = extractSourceExpr(inner[3].toObject());

        QJsonObject range = node["range"].toObject();
        op.beginOffset = extractCharOffset(range, "begin");
        int endOffset = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        op.endOffset = endOffset + tokLen;

        qDebug() << "  [StlRemove] array:" << op.arrayName
                 << "begin:" << op.index1 << "end:" << op.index2
                 << "val:" << op.opcode;
        m_operations.append(op);
        return;
    }
}

// ==================== STL replace ====================
void AstAnalyzer::visitStlReplace(const QJsonObject& node, const QJsonArray& inner) {
    if (inner.size() < 5) return;

    for (const auto& var : m_variables) {
        if (!var.isVector && !var.isArray) continue;

        QString arg1Name = getReferencedName(inner[1].toObject());
        if (arg1Name.isEmpty()) arg1Name = extractVectorNameFromIterator(inner[1].toObject());
        QString arg2Name = getReferencedName(inner[2].toObject());
        if (arg2Name.isEmpty()) arg2Name = extractVectorNameFromIterator(inner[2].toObject());

        if (arg1Name != var.name || arg2Name != var.name) continue;
        if (!isIteratorCall(inner[1].toObject()) && !isIteratorCall(inner[2].toObject()))
            continue;

        TrackedOperation op;
        op.type = TrackedOperation::StlReplace;
        op.arrayName = var.name;
        op.index1 = extractIteratorOffset(inner[1].toObject(), var.name);
        op.index2 = extractIteratorOffset(inner[2].toObject(), var.name);
        op.opcode = extractSourceExpr(inner[3].toObject());       // old value
        op.otherIndex = extractSourceExpr(inner[4].toObject());    // new value

        QJsonObject range = node["range"].toObject();
        op.beginOffset = extractCharOffset(range, "begin");
        int endOffset = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        op.endOffset = endOffset + tokLen;

        qDebug() << "  [StlReplace] array:" << op.arrayName
                 << "old:" << op.opcode << "new:" << op.otherIndex;
        m_operations.append(op);
        return;
    }
}

// ==================== STL rotate ====================
void AstAnalyzer::visitStlRotate(const QJsonObject& node, const QJsonArray& inner) {
    if (inner.size() < 4) return;

    for (const auto& var : m_variables) {
        if (!var.isVector && !var.isArray) continue;

        QString arg1Name = getReferencedName(inner[1].toObject());
        if (arg1Name.isEmpty()) arg1Name = extractVectorNameFromIterator(inner[1].toObject());
        QString arg2Name = getReferencedName(inner[2].toObject());
        if (arg2Name.isEmpty()) arg2Name = extractVectorNameFromIterator(inner[2].toObject());
        QString arg3Name = getReferencedName(inner[3].toObject());
        if (arg3Name.isEmpty()) arg3Name = extractVectorNameFromIterator(inner[3].toObject());

        if (arg1Name != var.name || arg2Name != var.name || arg3Name != var.name) continue;
        if (!isIteratorCall(inner[1].toObject()) && !isIteratorCall(inner[2].toObject()))
            continue;

        TrackedOperation op;
        op.type = TrackedOperation::StlRotate;
        op.arrayName = var.name;
        op.index1 = extractIteratorOffset(inner[1].toObject(), var.name);
        op.index2 = extractIteratorOffset(inner[2].toObject(), var.name);
        op.opcode = extractIteratorOffset(inner[3].toObject(), var.name);

        QJsonObject range = node["range"].toObject();
        op.beginOffset = extractCharOffset(range, "begin");
        int endOffset = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        op.endOffset = endOffset + tokLen;

        qDebug() << "  [StlRotate] array:" << op.arrayName
                 << "begin:" << op.index1 << "mid:" << op.index2 << "end:" << op.opcode;
        m_operations.append(op);
        return;
    }
}

// 辅助：从迭代器表达式（如 arr.begin(), arr.begin()+1）提取 vector 名
QString AstAnalyzer::extractVectorNameFromIterator(const QJsonObject& node) {
    QString kind = node["kind"].toString();
    
    // CXXOperatorCallExpr: 迭代器算术表达式如 v.begin() + 1
    // inner[0] = operator函数, inner[1] = LHS(迭代器), inner[2] = RHS(偏移)
    // 应递归到 LHS(inner[1]) 查找变量名，跳过 operator 函数本身(inner[0])
    if (kind == "CXXOperatorCallExpr" && node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        if (inner.size() >= 2) {
            QString n = extractVectorNameFromIterator(inner[1].toObject());
            if (!n.isEmpty()) return n;
            // 如果 LHS 没找到，尝试 RHS（如 1 + v.begin() 这种罕见情况）
            if (inner.size() >= 3) {
                n = extractVectorNameFromIterator(inner[2].toObject());
                if (!n.isEmpty()) return n;
            }
        }
        return "";
    }
    
    // 递归查找 DeclRefExpr
    QString name = getReferencedName(node);
    if (!name.isEmpty()) return name;
    
    if (!node.contains("inner")) return "";
    QJsonArray inner = node["inner"].toArray();
    for (const QJsonValue& child : inner) {
        QString n = extractVectorNameFromIterator(child.toObject());
        if (!n.isEmpty()) return n;
    }
    return "";
}

// 辅助：检查节点是否是迭代器调用（.begin() / .end() / .rbegin() / .rend()）
bool AstAnalyzer::isIteratorCall(const QJsonObject& node) {
    QString kind = node["kind"].toString();
    
    // CXXMemberCallExpr: arr.begin()
    if (kind == "CXXMemberCallExpr") {
        // 查找 MemberExpr
        if (!node.contains("inner")) return false;
        QJsonArray inner = node["inner"].toArray();
        for (const QJsonValue& child : inner) {
            QJsonObject childObj = child.toObject();
            if (childObj["kind"].toString() == "MemberExpr") {
                QString memberName = childObj["name"].toString();
                bool result = memberName.contains("begin") || memberName.contains("end") ||
                       memberName.contains("rbegin") || memberName.contains("rend");
                DBG_LOG("[isIteratorCall] CXXMemberCallExpr member=" << memberName.toStdString().c_str() << " -> " << (result?"true":"false"));
                return result;
            }
        }
        return false;
    }
    
    // BinaryOperator: arr.begin() + 1
    if (kind == "BinaryOperator") {
        if (!node.contains("inner")) return false;
        QJsonArray inner = node["inner"].toArray();
        if (inner.size() >= 2) {
            return isIteratorCall(inner[0].toObject()) || isIteratorCall(inner[1].toObject());
        }
        return false;
    }
    
    // UnaryOperator: arr.end() - 1（clang可能解析为 +(-1)）
    if (kind == "UnaryOperator") {
        if (!node.contains("inner")) return false;
        QJsonArray inner = node["inner"].toArray();
        if (!inner.isEmpty()) {
            return isIteratorCall(inner[0].toObject());
        }
        return false;
    }
    
    // CXXOperatorCallExpr: Clang 18+ 将迭代器 operator+/operator- 解析为此节点
    if (kind == "CXXOperatorCallExpr") {
        if (!node.contains("inner")) return false;
        QJsonArray inner = node["inner"].toArray();
        // inner[0] = operator函数, inner[1] = LHS, inner[2] = RHS
        if (inner.size() >= 2) {
            bool result = isIteratorCall(inner[1].toObject()) || isIteratorCall(inner[2].toObject());
            DBG_LOG("[isIteratorCall] CXXOperatorCallExpr -> " << (result?"true":"false"));
            return result;
        }
        return false;
    }
    
    // ImplicitCastExpr: 包装迭代器
    if (kind == "ImplicitCastExpr" && node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        if (!inner.isEmpty()) {
            bool result = isIteratorCall(inner[0].toObject());
            DBG_LOG("[isIteratorCall] ImplicitCastExpr -> " << (result?"true":"false"));
            return result;
        }
    }
    
    // MaterializeTemporaryExpr / CXXConstructExpr / CXXFunctionalCastExpr:
    // Clang 18+ 将迭代器表达式作为函数参数时会包裹这些透明节点
    if ((kind == "MaterializeTemporaryExpr" ||
         kind == "CXXConstructExpr" ||
         kind == "CXXFunctionalCastExpr") && node.contains("inner")) {
        QJsonArray inner2 = node["inner"].toArray();
        if (!inner2.isEmpty()) {
            bool result = isIteratorCall(inner2[0].toObject());
            qDebug() << "[isIteratorCall]" << kind << "-> recurse ->" << result;
            DBG_LOG("[isIteratorCall] " << kind.toStdString().c_str() << " -> recurse -> " << (result?"true":"false"));
            return result;
        }
    }
    
    DBG_LOG("[isIteratorCall] FALLTHROUGH for kind=" << kind.toStdString().c_str() << " -> false");
    return false;
}

// 辅助：从迭代器表达式提取索引偏移字符串
// arr.begin()      → "0"
// arr.begin() + 1  → "1"
// arr.end()        → "arr.size()"   (因为 end-1 会由调用方处理)
// arr.end() - 1    → "arr.size() - 1"
QString AstAnalyzer::extractIteratorOffset(const QJsonObject& node, const QString& varName) {
    QString kind = node["kind"].toString();
    
    // 直接迭代器调用：arr.begin() / arr.end()
    if (kind == "CXXMemberCallExpr") {
        if (!node.contains("inner")) return "";
        QJsonArray inner = node["inner"].toArray();
        for (const QJsonValue& child : inner) {
            QJsonObject childObj = child.toObject();
            if (childObj["kind"].toString() == "MemberExpr") {
                QString memberName = childObj["name"].toString();
                if (memberName.contains("begin")) {
                    return "0";
                } else if (memberName.contains("end")) {
                    return varName + ".size()";
                }
            }
        }
        return "";
    }
    
    // BinaryOperator: arr.begin() + n  或 arr.end() - n
    if (kind == "BinaryOperator") {
        if (!node.contains("inner")) return "";
        QJsonArray inner = node["inner"].toArray();
        if (inner.size() < 2) return "";
        
        QJsonObject lhs = inner[0].toObject();
        QJsonObject rhs = inner[1].toObject();
        QString opcode = node["opcode"].toString();
        
        // 检查 lhs 是否是迭代器调用
        QString baseOffset = extractIteratorOffset(lhs, varName);
        if (baseOffset.isEmpty()) {
            // 可能是 rhs
            baseOffset = extractIteratorOffset(rhs, varName);
            if (!baseOffset.isEmpty()) {
                // 交换 lhs/rhs
                QJsonObject tmp = lhs; lhs = rhs; rhs = tmp;
            }
        }
        if (baseOffset.isEmpty()) return "";
        
        // 提取 rhs 的常量表达式
        QString rhsExpr = extractSourceExpr(rhs);
        if (rhsExpr.isEmpty()) return "";
        
        if (opcode == "+") {
            if (baseOffset == "0") return rhsExpr;
            return baseOffset + " + " + rhsExpr;
        } else if (opcode == "-") {
            if (baseOffset == "0") return "-" + rhsExpr;
            return baseOffset + " - " + rhsExpr;
        }
        return "";
    }
    
    // ImplicitCastExpr: 包装
    if (kind == "ImplicitCastExpr" && node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        if (!inner.isEmpty()) {
            return extractIteratorOffset(inner[0].toObject(), varName);
        }
    }
    
    // CXXConstructExpr: 迭代器构造包装 (erase/insert 参数会被包裹)
    if (kind == "CXXConstructExpr" && node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        if (!inner.isEmpty()) {
            return extractIteratorOffset(inner[0].toObject(), varName);
        }
    }
    
    // MaterializeTemporaryExpr: 临时对象包装
    if (kind == "MaterializeTemporaryExpr" && node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        if (!inner.isEmpty()) {
            return extractIteratorOffset(inner[0].toObject(), varName);
        }
    }
    
    // CXXFunctionalCastExpr: 函数式类型转换包装
    if (kind == "CXXFunctionalCastExpr" && node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        if (!inner.isEmpty()) {
            return extractIteratorOffset(inner[0].toObject(), varName);
        }
    }
    
    // CXXOperatorCallExpr: operator+ / operator- (Clang 可能将 BinaryOperator 折叠成此节点)
    if (kind == "CXXOperatorCallExpr") {
        if (!node.contains("inner")) return "";
        QJsonArray inner = node["inner"].toArray();
        if (inner.size() < 2) return "";
        
        // inner[0] = operator function (FunctionToPointerDecay), inner[1] = LHS, inner[2] = RHS
        QString opFuncName;
        // 查找 operator 名称（inner[0] 或更深层）
        std::function<QString(const QJsonObject&)> findOpName =
            [&](const QJsonObject& obj) -> QString {
            if (obj["kind"].toString() == "DeclRefExpr") {
                return obj["referencedDecl"].toObject()["name"].toString();
            }
            if (obj.contains("inner")) {
                for (const auto& c : obj["inner"].toArray()) {
                    QString n = findOpName(c.toObject());
                    if (!n.isEmpty()) return n;
                }
            }
            return "";
        };
        opFuncName = findOpName(inner[0].toObject());
        
        QJsonObject lhs = inner[1].toObject();
        QJsonObject rhs = inner[2].toObject();
        
        // 从 LHS 提取基础偏移
        QString baseOffset = extractIteratorOffset(lhs, varName);
        if (baseOffset.isEmpty()) {
            baseOffset = extractIteratorOffset(rhs, varName);
            if (!baseOffset.isEmpty()) {
                std::swap(lhs, rhs);
            }
        }
        if (baseOffset.isEmpty()) return "";
        
        QString rhsExpr = extractSourceExpr(rhs);
        if (rhsExpr.isEmpty()) return "";
        
        if (opFuncName.contains("operator+")) {
            if (baseOffset == "0") return rhsExpr;
            return baseOffset + " + " + rhsExpr;
        } else if (opFuncName.contains("operator-")) {
            if (baseOffset == "0") return "-" + rhsExpr;
            return baseOffset + " - " + rhsExpr;
        }
        return "";
    }
    
    return "";
}

// 辅助：从 AST 节点提取源码表达式文本
QString AstAnalyzer::extractSourceExpr(const QJsonObject& node) {
    if (node.isEmpty()) return "";
    QJsonObject range = node["range"].toObject();
    if (!range.isEmpty()) {
        int begin = extractCharOffset(range, "begin");
        int end = extractCharOffset(range, "end");
        int tokLen = range["end"].toObject()["tokLen"].toInt();
        QString result = m_sourceCode.mid(begin, end + tokLen - begin);
        if (!result.isEmpty()) return result;
    }
    // 节点无有效 range（如 MaterializeTemporaryExpr）或提取为空，
    // 递归穿透 inner 子节点查找有 range 的实际表达式
    if (node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        for (const auto& child : inner) {
            QString expr = extractSourceExpr(child.toObject());
            if (!expr.isEmpty()) return expr;
        }
    }
    return "";
}

void AstAnalyzer::visitBinaryOperator(const QJsonObject& node) {
    QString opcode = node["opcode"].toString();
    if (opcode.isEmpty()) return;
    
    if (!node.contains("inner") || node["inner"].toArray().size() < 2) return;
    
    QJsonArray inner = node["inner"].toArray();
    QJsonObject lhs = inner[0].toObject();
    QJsonObject rhs = inner[1].toObject();
    
    // 检查比较操作：v[i] > v[j]  或 跨数组 arr[i] > brr[j]
    if (opcode == ">" || opcode == "<" || opcode == ">=" || 
        opcode == "<=" || opcode == "==" || opcode == "!=") {
        
        for (const auto& var : m_variables) {
            QString varName = var.name;
            
            if (isArraySubscriptOn(lhs, varName) && isArraySubscriptOn(rhs, varName)) {
                // 同数组比较
                TrackedOperation op;
                op.type = TrackedOperation::Compare;
                op.arrayName = varName;
                op.opcode = opcode;
                op.index1 = getSubscriptIndex(lhs);
                op.index2 = getSubscriptIndex(rhs);
                
                QJsonObject range = node["range"].toObject();
                op.beginOffset = extractCharOffset(range, "begin");
                int endOffset = extractCharOffset(range, "end");
                int tokLen = range["end"].toObject()["tokLen"].toInt();
                op.endOffset = endOffset + tokLen;
                
                qDebug() << "  [Compare] array:" << op.arrayName << "op:" << op.opcode << "i1:" << op.index1 << "i2:" << op.index2;
                m_operations.append(op);
                return;
            }
        }

        // 跨数组比较：检查 LHS 和 RHS 是否属于不同的已追踪变量
        for (const auto& varA : m_variables) {
            if (!isArraySubscriptOn(lhs, varA.name)) continue;
            for (const auto& varB : m_variables) {
                if (varA.name == varB.name) continue;
                if (!isArraySubscriptOn(rhs, varB.name)) continue;

                TrackedOperation op;
                op.type = TrackedOperation::CrossCompare;
                op.arrayName = varA.name;
                op.index1 = getSubscriptIndex(lhs);
                op.otherArrayName = varB.name;
                op.otherIndex = getSubscriptIndex(rhs);
                op.opcode = opcode;

                QJsonObject range = node["range"].toObject();
                op.beginOffset = extractCharOffset(range, "begin");
                int endOffset = extractCharOffset(range, "end");
                int tokLen = range["end"].toObject()["tokLen"].toInt();
                op.endOffset = endOffset + tokLen;

                qDebug() << "  [CrossCompare]" << varA.name << "[" << op.index1 << "]"
                         << op.opcode << varB.name << "[" << op.otherIndex << "]";
                m_operations.append(op);
                return;
            }
        }
    }
    
    // 检查赋值操作：v[i] = v[j]  或  跨数组 arr[i] = brr[j]
    if (opcode == "=") {
        for (const auto& var : m_variables) {
            QString varName = var.name;
            
            if (isArraySubscriptOn(lhs, varName)) {
                TrackedOperation op;
                op.arrayName = varName;
                op.index1 = getSubscriptIndex(lhs);
                
                QJsonObject range = node["range"].toObject();
                op.beginOffset = extractCharOffset(range, "begin");
                int endOffset = extractCharOffset(range, "end");
                int tokLen = range["end"].toObject()["tokLen"].toInt();
                op.endOffset = endOffset + tokLen;
                
                if (isArraySubscriptOn(rhs, varName)) {
                    op.type = TrackedOperation::ArrayAssign;
                    op.index2 = getSubscriptIndex(rhs);
                } else {
                    // 检查 RHS 是否是对另一个已追踪变量的下标
                    bool crossFound = false;
                    for (const auto& varB : m_variables) {
                        if (varB.name == varName) continue;
                        if (isArraySubscriptOn(rhs, varB.name)) {
                            op.type = TrackedOperation::CrossAssign;
                            op.otherArrayName = varB.name;
                            op.otherIndex = getSubscriptIndex(rhs);
                            crossFound = true;
                            break;
                        }
                    }
                    if (!crossFound) {
                        op.type = TrackedOperation::ValueAssign;
                        // 提取 RHS 表达式源码文本（如 vv[i] = i 中的 "i"）
                        QJsonObject rhsRange = rhs["range"].toObject();
                        int rhsBegin = extractCharOffset(rhsRange, "begin");
                        int rhsEnd = extractCharOffset(rhsRange, "end");
                        int rhsTokLen = rhsRange["end"].toObject()["tokLen"].toInt();
                        op.index2 = m_sourceCode.mid(rhsBegin, rhsEnd + rhsTokLen - rhsBegin);
                    }
                }
                
                qDebug() << "  [Assign] type:" << (int)op.type << "array:" << op.arrayName 
                         << "i1:" << op.index1 << "i2:" << op.index2
                         << "other:" << op.otherArrayName << "[" << op.otherIndex << "]";
                m_operations.append(op);
                return;
            }
        }
    }
}

QString AstAnalyzer::getReferencedName(const QJsonObject& node) {
    if (node["kind"].toString() == "DeclRefExpr") {
        QJsonObject refDecl = node["referencedDecl"].toObject();
        QString name = refDecl["name"].toString();
        // 跳过 operator 函数名（operator+, operator-, operator[], operator<, 等）
        // getReferencedName 用于查找变量名，不应返回 operator 重载函数名
        if (!name.startsWith("operator")) {
            return name;
        }
        return "";
    }
    if (node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        for (const QJsonValue& child : inner) {
            QString name = getReferencedName(child.toObject());
            if (!name.isEmpty()) return name;
        }
    }
    return "";
}

bool AstAnalyzer::isArraySubscriptOn(const QJsonObject& node, const QString& varName) {
    // 处理原生数组的下标访问：a[i]
    if (node["kind"].toString() == "ArraySubscriptExpr") {
        if (!node.contains("inner")) return false;
        QJsonArray inner = node["inner"].toArray();
        if (inner.isEmpty()) return false;

        QString baseName = getReferencedName(inner[0].toObject());
        return baseName == varName;
    }

    // 处理 vector 的下标访问：v[i]（通过 operator[]）
    if (node["kind"].toString() == "CXXOperatorCallExpr") {
        // 检查是否是 operator[] 调用
        if (!node.contains("inner")) return false;
        QJsonArray inner = node["inner"].toArray();
        if (inner.size() < 2) return false;  // 需要 [函数引用, 对象, 索引]

        // 第一个子节点是 operator[] 的引用
        QJsonObject funcRef = inner[0].toObject();
        QString funcName;
        // 递归查找函数名
        std::function<void(const QJsonObject&)> findOpName = [&](const QJsonObject& obj) {
            if (obj["kind"].toString() == "DeclRefExpr") {
                QJsonObject refDecl = obj["referencedDecl"].toObject();
                funcName = refDecl["name"].toString();
            } else if (obj.contains("inner")) {
                for (const auto& child : obj["inner"].toArray()) {
                    findOpName(child.toObject());
                }
            }
        };
        findOpName(funcRef);

        // 检查是否是 operator[]
        if (!funcName.contains("operator[]") && !funcName.contains("operator[]=")) {
            return false;
        }

        // 第二个子节点是被操作的对象（vector 本身）
        QString baseName = getReferencedName(inner[1].toObject());
        return baseName == varName;
    }

    // 处理隐式转换
    if (node["kind"].toString() == "ImplicitCastExpr" && node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        if (!inner.isEmpty()) {
            return isArraySubscriptOn(inner[0].toObject(), varName);
        }
    }

    // 处理 vector 的 at() 访问：v.at(i)
    if (node["kind"].toString() == "CXXMemberCallExpr") {
        if (!node.contains("inner")) return false;
        QJsonArray innerArr = node["inner"].toArray();
        if (innerArr.isEmpty()) return false;

        // 提取成员函数名
        QString memberName;
        std::function<void(const QJsonObject&)> findMember = [&](const QJsonObject& obj) {
            if (obj["kind"].toString() == "MemberExpr") memberName = obj["name"].toString();
            else if (obj.contains("inner"))
                for (const auto& c : obj["inner"].toArray())
                    findMember(c.toObject());
        };
        findMember(innerArr[0].toObject());
        if (memberName != "at") return false;

        // 提取容器变量名
        QString baseName = extractContainerObjectName(node);
        return baseName == varName;
    }
    
    return false;
}

QString AstAnalyzer::getSubscriptIndex(const QJsonObject& node) {
    QJsonObject subscriptNode;
    
    if (node["kind"].toString() == "ArraySubscriptExpr") {
        subscriptNode = node;
    } 
    // 处理 vector 的下标访问
    else if (node["kind"].toString() == "CXXOperatorCallExpr") {
        subscriptNode = node;
    }
    else if (node["kind"].toString() == "ImplicitCastExpr" && node.contains("inner")) {
        QJsonArray inner = node["inner"].toArray();
        for (const QJsonValue& child : inner) {
            QString kind = child.toObject()["kind"].toString();
            if (kind == "ArraySubscriptExpr" || kind == "CXXOperatorCallExpr" || kind == "CXXMemberCallExpr") {
                subscriptNode = child.toObject();
                break;
            }
        }
    }
    // 处理 vector 的 at() 访问：v.at(i)
    else if (node["kind"].toString() == "CXXMemberCallExpr") {
        subscriptNode = node;
    }

    if (subscriptNode.isEmpty() || !subscriptNode.contains("inner")) return "";

    QJsonArray inner = subscriptNode["inner"].toArray();
    
    // 对于 ArraySubscriptExpr: inner = [base, index]
    // 对于 CXXOperatorCallExpr: inner = [operator[], base, index]
    // 对于 CXXMemberCallExpr(at): inner = [MemberExpr, index, ...]
    int indexPos;
    if (subscriptNode["kind"].toString() == "CXXOperatorCallExpr") {
        if (inner.size() < 3) return "";
        indexPos = 2;  // 第三个元素是索引
    } else if (subscriptNode["kind"].toString() == "CXXMemberCallExpr") {
        if (inner.size() < 2) return "";
        return extractSourceExpr(inner[1].toObject());
    } else {
        if (inner.size() < 2) return "";
        indexPos = 1;  // 第二个元素是索引
    }

    QJsonObject indexNode = inner[indexPos].toObject();

    // 获取索引的源码文本（转换为char offset）
    QJsonObject range = indexNode["range"].toObject();
    int begin = extractCharOffset(range, "begin");
    int end = extractCharOffset(range, "end");
    int tokLen = range["end"].toObject()["tokLen"].toInt();

    return m_sourceCode.mid(begin, end + tokLen - begin);
}

bool AstAnalyzer::isTrackedVariable(const QString& name) const {
    for (const auto& var : m_variables) {
        if (var.name == name) return true;
    }
    return false;
}

TrackedVariable* AstAnalyzer::findVariable(const QString& name) {
    for (auto& var : m_variables) {
        if (var.name == name) return &var;
    }
    return nullptr;
}

int AstAnalyzer::offsetToLine(int offset) const {
    int line = 1;
    for (int i = 0; i < offset && i < m_sourceCode.length(); ++i) {
        if (m_sourceCode[i] == '\n') line++;
    }
    return line;
}

int AstAnalyzer::offsetToCol(int offset) const {
    int col = 1;
    for (int i = offset - 1; i >= 0; --i) {
        if (m_sourceCode[i] == '\n') break;
        col++;
    }
    return col;
}

int AstAnalyzer::byteToCharOffset(int byteOffset) const {
    if (byteOffset <= 0) return 0;
    if (byteOffset >= m_sourceUtf8.size()) return m_sourceCode.length();
    // 将UTF-8字节前缀解码为QString，其长度即为character offset
    return QString::fromUtf8(m_sourceUtf8.left(byteOffset)).length();
}

int AstAnalyzer::extractCharOffset(const QJsonObject& rangeObj, const QString& field) const {
    QJsonObject locObj = rangeObj[field].toObject();
    if (locObj.isEmpty()) return 0;
    int byteOff = locObj["offset"].toInt();
    return byteToCharOffset(byteOff);
}

QString AstAnalyzer::findClangPath() {
    // 1. 检查VS自带的Clang
    QStringList vsPaths = {
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/x64/bin/clang.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/x64/bin/clang.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/clang.exe",
        "C:/Program Files/Microsoft Visual Studio/2026/Community/VC/Tools/Llvm/x64/bin/clang.exe",
        "C:/Program Files/Microsoft Visual Studio/2026/Professional/VC/Tools/Llvm/x64/bin/clang.exe",
    };
    for (const auto& path : vsPaths) {
        if (QFile::exists(path)) return path;
    }
    
    // 2. 检查MSYS2/MinGW
    QStringList mingwPaths = {
        "C:/msys64/mingw64/bin/clang.exe",
        "C:/msys64/ucrt64/bin/clang.exe",
        "C:/msys64/clang64/bin/clang.exe",
        "C:/mingw64/bin/clang.exe",
    };
    for (const auto& path : mingwPaths) {
        if (QFile::exists(path)) return path;
    }
    
    // 3. 系统PATH
    return "clang";
}

// 辅助：从容器成员函数调用（如 v.push_back(x)）提取容器变量名
QString AstAnalyzer::extractContainerObjectName(const QJsonObject& node) {
    // v.push_back(x) 的 AST 结构：
    // CXXMemberCallExpr
    //   |- MemberExpr <-- name = "push_back"
    //   |- CXXThisExpr (隐式)
    //   |- ImplicitCastExpr
    //   |   `- DeclRefExpr <-- 这里才是 v（容器变量名）
    //   `- ... (参数)
    
    if (node["kind"].toString() != "CXXMemberCallExpr") return "";
    if (!node.contains("inner")) return "";
    
    QJsonArray inner = node["inner"].toArray();
    if (inner.isEmpty()) return "";
    
    // 遍历子节点，找 DeclRefExpr
    std::function<QString(const QJsonObject&)> findObjName =
        [&](const QJsonObject& obj) -> QString {
        if (obj["kind"].toString() == "DeclRefExpr") {
            QJsonObject refDecl = obj["referencedDecl"].toObject();
            return refDecl["name"].toString();
        }
        if (obj.contains("inner")) {
            QJsonArray childArr = obj["inner"].toArray();
            for (const QJsonValue& v : childArr) {
                QString n = findObjName(v.toObject());
                if (!n.isEmpty()) return n;
            }
        }
        return "";
    };
    
    return findObjName(inner[0].toObject());
}
