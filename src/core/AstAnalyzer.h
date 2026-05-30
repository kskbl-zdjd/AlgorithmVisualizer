#ifndef AST_ANALYZER_H
#define AST_ANALYZER_H

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QFile>

// 分析结果数据结构
struct TrackedVariable {
    QString name;           // 变量名
    QString type;           // 类型（vector<int>, int[], string）
    QString elementType;     // 元素类型（int, char等）
    bool isVector;          // 是否为vector
    bool isString;          // 是否为string
    bool isArray;           // 是否为原生数组
    bool isParameter;       // 是否为函数参数（ParmVarDecl）
    bool isReferenceParam;  // 是否为引用参数（包含&），不创建面板/插桩，但需被追踪
    QString initValues;      // 初始化值
    int defEndOffset;       // 定义语句结束的offset（用于插入跟踪代码）
};

// 函数参数信息（用于别名生成）
struct ParamInfo {
    QString name;
    bool isReference;
};

// 函数调用点信息（用于生成别名 push/pop）
struct CallSiteInfo {
    QString funcName;        // 被调用的函数名
    QStringList argExprs;    // 实参表达式列表（按顺序）
    int beginOffset;         // 调用表达式起始 offset
    int endOffset;           // 调用表达式结束 offset（右括号之后）
};

struct TrackedOperation {
    enum OpType {
        Swap,           // swap(v[i], v[j])
        Compare,        // v[i] > v[j]
        ArrayAssign,    // v[i] = v[j]
        ValueAssign,    // v[i] = expr
        ArrayRead,      // v[i] 被读取
        StlReverse,     // reverse(v.begin(), v.end()) — STL 算法
        StlFill,        // fill(v.begin(), v.end(), val) — STL 算法
        StlPushBack,    // v.push_back(x)
        StlPopBack,     // v.pop_back()
        StlInsert,      // v.insert(pos, x)
        StlErase,       // v.erase(pos) 或 v.erase(first, last)
        StlClear,       // v.clear()
        StlVecResize,   // v.resize(n) 或 v.resize(n, val)
        StlSort,        // sort(v.begin(), v.end()) 或 sort(v.begin(), v.end(), cmp)
        CrossCompare,    // arr[i] > brr[j]（跨数组比较，两数组名不同）
        CrossAssign,     // arr[i] = brr[j]（跨数组赋值，两数组名不同）
        StlAt,           // v.at(i)
        StlEmplaceBack,  // v.emplace_back(args...)
        StlBinarySearch, // binary_search(v.begin(), v.end(), val)
        StlFind,         // find(v.begin(), v.end(), val)
        StlCopy,         // copy(src.begin(), src.end(), dst.begin())
        StlContainerSwap,// swap(v1, v2) — 容器整体交换
        StlRemove,       // remove(v.begin(), v.end(), val)
        StlReplace,      // replace(v.begin(), v.end(), old_val, new_val)
        StlRotate        // rotate(v.begin(), mid, v.end())
    };
    
    OpType type;
    QString arrayName;      // 操作的数组/vector名
    int beginOffset;        // 操作开始位置
    int endOffset;          // 操作结束位置
    QString index1;         // 第一个索引表达式
    QString index2;         // 第二个索引表达式（比较/交换时）
    QString opcode;         // 运算符（>, <, ==等）

    // === 跨数组操作专用字段 ===
    QString otherArrayName;  // 跨数组操作时对方数组名（非空表示跨数组）
    QString otherIndex;      // 跨数组操作时对方的下标表达式
};

class AstAnalyzer : public QObject {
    Q_OBJECT
public:
    explicit AstAnalyzer(QObject* parent = nullptr);
    
    // 分析用户代码
    bool analyze(const QString& code);
    
    // 获取分析结果
    QList<TrackedVariable> getVariables() const { return m_variables; }
    QList<TrackedOperation> getOperations() const { return m_operations; }
    bool hasMainFunction() const { return m_hasMain; }
    QString getLastError() const { return m_lastError; }
    
    // 获取函数参数信息（用于别名生成）
    QMap<QString, QList<ParamInfo>> getFunctionParams() const { return m_functionParams; }
    QList<CallSiteInfo> getCallSites() const { return m_callSites; }
    
    // 查找Clang路径
    static QString findClangPath();
    
private:
    QList<TrackedVariable> m_variables;
    QList<TrackedOperation> m_operations;
    bool m_hasMain;
    QString m_lastError;
    QString m_sourceCode;       // 保存原始代码（QString，按字符计索引）
    QByteArray m_sourceUtf8;    // 原始代码的UTF-8字节数据，用于Clang byte offset→char offset转换
    int m_currentFunctionBodyOffset = -1;  // 当前正在遍历的函数的 { 位置
    
    // 函数参数信息（用于别名生成）
    QMap<QString, QList<ParamInfo>> m_functionParams;
    QList<CallSiteInfo> m_callSites;
    
    // Clang调用
    QString runClangAstDump(const QString& code);
    
    // AST遍历
    void traverseAst(const QJsonObject& node);
    void visitVarDecl(const QJsonObject& node);
    void visitCallExpr(const QJsonObject& node);
    void visitMemberCallExpr(const QJsonObject& node);
    void visitBinaryOperator(const QJsonObject& node);
    void visitFunctionDecl(const QJsonObject& node);
    void visitUserCallExpr(const QJsonObject& node);  // 检测用户函数调用点
    void visitStlReverse(const QJsonObject& node, const QJsonArray& inner);
    void visitStlFill(const QJsonObject& node, const QJsonArray& inner);
    void visitStlSort(const QJsonObject& node, const QJsonArray& inner);
    void visitStlBinarySearch(const QJsonObject& node, const QJsonArray& inner);
    void visitStlFind(const QJsonObject& node, const QJsonArray& inner);
    void visitStlCopy(const QJsonObject& node, const QJsonArray& inner);
    void visitStlContainerSwap(const QJsonObject& node, const QJsonArray& inner);
    void visitStlRemove(const QJsonObject& node, const QJsonArray& inner);
    void visitStlReplace(const QJsonObject& node, const QJsonArray& inner);
    void visitStlRotate(const QJsonObject& node, const QJsonArray& inner);
    
    // 赋值上下文检测：处理 binary_search/find 作为变量初始化器的情况
    void detectAssignmentContext(TrackedOperation& op, const QString& sourceCode);
    
    // 辅助：从AST节点提取信息
    QString getReferencedName(const QJsonObject& node);
    bool isArraySubscriptOn(const QJsonObject& node, const QString& varName);
    QString getSubscriptIndex(const QJsonObject& node);
    bool isTrackedVariable(const QString& name) const;
    TrackedVariable* findVariable(const QString& name);
    
    // STL 算法相关辅助
    QString extractVectorNameFromIterator(const QJsonObject& node);
    bool isIteratorCall(const QJsonObject& node);
    QString extractIteratorOffset(const QJsonObject& node, const QString& varName);
    QString extractSourceExpr(const QJsonObject& node);
    QString extractContainerObjectName(const QJsonObject& node);
    
    // 辅助：Byte offset → Character offset（Clang用byte offset，Qt QString用char offset）
    int byteToCharOffset(int byteOffset) const;
    
    // 辅助：从Clang range节点的begin/end中提取并转换为char offset
    int extractCharOffset(const QJsonObject& rangeObj, const QString& field) const;
    
    // 辅助：offset转行号列号
    int offsetToLine(int offset) const;
    int offsetToCol(int offset) const;
};

#endif // AST_ANALYZER_H
