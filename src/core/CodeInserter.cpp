#include "CodeInserter.h"
#include <algorithm>
#include <QRegularExpression>

CodeInserter::CodeInserter() {}

QString CodeInserter::insert(const QString& sourceCode,
                             const QList<TrackedVariable>& variables,
                             const QList<TrackedOperation>& operations,
                             bool hasMain,
                             const QMap<QString, QList<ParamInfo>>& functionParams,
                             const QList<CallSiteInfo>& callSites) {
    m_insertions.clear();
    m_variables = variables;  // 存储变量列表供后续使�?
    // 1. 在每个变量定义后插入跟踪代码
    for (int i = 0; i < variables.size(); ++i) {
        const auto& var = variables[i];
        Insertion ins;
        ins.offset = var.defEndOffset;
        ins.text = "\n    " + generateTrackingCode(var);
// 按变量定义顺�?
    m_insertions.append(ins);
    }

// 2. 替换操作为viz调用（从后往前替换，避免偏移�?
    QList<TrackedOperation> sortedOps = operations;
    std::sort(sortedOps.begin(), sortedOps.end(),
        [](const TrackedOperation& a, const TrackedOperation& b) {
            return a.beginOffset > b.beginOffset;  // 降序
        });

    for (const auto& op : sortedOps) {
        Insertion ins;
        ins.offset = op.beginOffset;
        ins.order = 1000 + op.beginOffset;

        // 设置替换长度：用viz调用替换原有操作代码
        ins.replaceLen = op.endOffset - op.beginOffset;

        switch (op.type) {
        case TrackedOperation::Swap:
            ins.text = generateSwapCode(op);
            break;
        case TrackedOperation::Compare:
            ins.text = generateCompareCode(op);
            break;
        case TrackedOperation::ArrayAssign:
            ins.text = generateAssignCode(op);
            break;
        case TrackedOperation::ValueAssign:
            ins.text = generateAssignCode(op);
            break;
        case TrackedOperation::StlReverse:
            ins.text = generateReverseCode(op);
            break;
        case TrackedOperation::StlFill:
            ins.text = generateFillCode(op);
            break;
        case TrackedOperation::StlVecResize:
            ins.text = generateVecResizeCode(op);
            break;
        case TrackedOperation::StlSort:
            ins.text = generateSortCode(op);
            break;
        case TrackedOperation::StlPushBack:
        case TrackedOperation::StlPopBack:
        case TrackedOperation::StlInsert:
        case TrackedOperation::StlErase:
        case TrackedOperation::StlClear:
        case TrackedOperation::StlEmplaceBack:
            ins.text = generateContainerCode(op);
            break;

        case TrackedOperation::StlAt:
            ins.text = generateAtCode(op);
            break;
        case TrackedOperation::StlBinarySearch:
            continue;  // binary_search 返回 bool，不演示可视化，保留原始代码
        case TrackedOperation::StlFind:
            ins.text = generateFindCode(op);
            break;
        case TrackedOperation::StlCopy:
            ins.text = generateCopyCode(op);
            break;
        case TrackedOperation::StlContainerSwap:
            ins.text = generateContainerSwapCode(op);
            break;
        case TrackedOperation::StlRemove:
            ins.text = generateRemoveCode(op);
            break;
        case TrackedOperation::StlReplace:
            ins.text = generateReplaceCode(op);
            break;
        case TrackedOperation::StlRotate:
            ins.text = generateRotateCode(op);
            break;

        case TrackedOperation::CrossCompare:
        case TrackedOperation::CrossAssign:
            ins.text = generateCrossOpCode(op);
            break;

        default:
            continue;
        }

        m_insertions.append(ins);
    }

// 3. 添加头文�?
    Insertion headerIns;
    headerIns.offset = 0;
    headerIns.text = "#include \"viz_api.h\"\n";
// 最先插�?
    m_insertions.append(headerIns);

    // 4. 如果有main函数，生成包装；否则生成DLL入口+测试数据
    if (hasMain) {
        Insertion dllIns;
        dllIns.offset = sourceCode.length();
        dllIns.text = "\n\n"
            "extern \"C\" __declspec(dllexport) int run_algorithm(std::vector<VizEvent>* buffer) {\n"
            "    viz::setEventBuffer(buffer);\n"
            "    return main();\n"
            "}\n";
        dllIns.order = 9999;
        m_insertions.append(dllIns);
    } else {
        // 算法函数模式：生成包装main
        QString wrapperMain = generateWrapperMain(variables);
        Insertion wrapperIns;
        wrapperIns.offset = sourceCode.length();
        wrapperIns.text = "\n\n" + wrapperMain;
        wrapperIns.order = 9999;
        m_insertions.append(wrapperIns);
    }

    // 5. 值参数生命周期：在函数退出点插入 destroyArray 调用
    insertParamCleanup(sourceCode, variables);

    // 6. 引用参数别名：在函数调用点插入 pushAlias/popAlias
    if (!callSites.isEmpty()) {
        insertCallAliases(sourceCode, callSites, functionParams);
    }

    return applyInsertions(sourceCode);
}

QString CodeInserter::generateTrackingCode(const TrackedVariable& var) {
    // 引用参数不需要 viz::array() 注册——事件通过别名路由到调用者变量
    if (var.isReferenceParam) return "";
    
    if (var.isVector) {
        return QString("viz::array(%1, \"%1\");").arg(var.name);
    } else if (var.isArray) {
        // 从类型中提取大小，如 "int [7]" -> "7"
        QRegularExpression sizeRegex(R"(\[\s*(\d+)\s*\])");
        QRegularExpressionMatch match = sizeRegex.match(var.type);
        QString sizeExpr = match.hasMatch() ? match.captured(1) : "7";
        return QString("viz::array(%1, %2, \"%1\");").arg(var.name).arg(sizeExpr);
    } else if (var.isString) {
        return QString("viz::string(%1.c_str(), %1.size(), \"%1\");").arg(var.name);
    }
    return "";
}

QString CodeInserter::generateSwapCode(const TrackedOperation& op) {
// 向量使用 .data()，原生数组直接使用名�?
    const TrackedVariable* var = findVar(op.arrayName);
    QString dataExpr = var ? getDataExpr(*var) : op.arrayName;
    return QString("viz::swap(%1, %2, %3, \"%4\")")
        .arg(dataExpr).arg(op.index1).arg(op.index2).arg(op.arrayName);
}

QString CodeInserter::generateCompareCode(const TrackedOperation& op) {
    const TrackedVariable* var = findVar(op.arrayName);
    QString dataExpr = var ? getDataExpr(*var) : op.arrayName;
    return QString("(viz::compare(%1, %2, %3, \"%4\") %5 0)")
        .arg(dataExpr).arg(op.index1).arg(op.index2).arg(op.arrayName).arg(op.opcode);
}

QString CodeInserter::generateAssignCode(const TrackedOperation& op) {
    const TrackedVariable* var = findVar(op.arrayName);
    QString dataExpr = var ? getDataExpr(*var) : op.arrayName;
    if (op.type == TrackedOperation::ArrayAssign) {
        return QString("viz::swap(%1, %2, %3, \"%4\")")
            .arg(dataExpr).arg(op.index1).arg(op.index2).arg(op.arrayName);
    }
    if (op.type == TrackedOperation::ValueAssign) {
        return QString("viz::setValue(%1, %2, %3, \"%4\")")
            .arg(dataExpr).arg(op.index1).arg(op.index2).arg(op.arrayName);
    }
    if (op.type == TrackedOperation::CrossAssign) {
        // 跨数组赋�? arr[i] = brr[j]
        // 先读�?brr[j] 的值，再写�?arr[i]
        const TrackedVariable* otherVar = findVar(op.otherArrayName);
        QString otherDataExpr = otherVar ? getDataExpr(*otherVar) : op.otherArrayName;
        return QString(
            "{\n"
            "    int _viz_cross_val = %1[%2];\n"
            "    viz::setValue(%3, %4, _viz_cross_val, \"%5\");\n"
            "    %5.data()[%4] = _viz_cross_val;\n"
            "}\n"
        ).arg(op.otherArrayName).arg(op.otherIndex)
         .arg(dataExpr).arg(op.index1).arg(op.arrayName);
    }
    return "";
}

QString CodeInserter::getDataExpr(const TrackedVariable& var) {
    if (var.isVector) {
        return var.name + ".data()";
    }
    return var.name;
}

QString CodeInserter::generateReverseCode(const TrackedOperation& op) {
    const TrackedVariable* var = findVar(op.arrayName);
    QString dataExpr = var ? getDataExpr(*var) : op.arrayName;

    // �?reverse(arr.begin(), arr.end()) 替换为带 viz::swap 的手动循�?    // index1 = begin 偏移（如 "0"�?    // index2 = end   偏移（如 "arr.size()"�?    // end 偏移需要减 1 才能得到最后一个有效索�?
    QString beginExpr = op.index1;   // �?"0"
    QString endExpr = op.index2;     // �?"arr.size()"

    // 处理 end 偏移�?1
    QString lastIdxExpr;
    if (endExpr == op.arrayName + ".size()") {
        lastIdxExpr = "(int)" + op.arrayName + ".size() - 1";
    } else {
        lastIdxExpr = "(int)(" + endExpr + ") - 1";
    }

    QString code = QString(
        "{\n"
        "    int _viz_l = %1;\n"
        "    int _viz_r = %2;\n"
        "    while (_viz_l < _viz_r) {\n"
        "        viz::swap(%3, _viz_l, _viz_r, \"%4\");\n"
        "        ++_viz_l;\n"
        "        --_viz_r;\n"
        "    }\n"
        "}\n"
    ).arg(beginExpr).arg(lastIdxExpr).arg(dataExpr).arg(op.arrayName);

    return code;
}

QString CodeInserter::generateFillCode(const TrackedOperation& op) {
    const TrackedVariable* var = findVar(op.arrayName);
    QString dataExpr = var ? getDataExpr(*var) : op.arrayName;
    QString varName = op.arrayName;

    // index1 = begin 偏移（如 "0"），index2 = end 偏移（如 "arr.size()"）
    // opcode 存放填充值表达式（如 "0", "-1", "n"）
    QString beginExpr = op.index1.isEmpty() ? "0" : op.index1;
    QString endExpr = op.index2;
    QString valExpr  = op.opcode.isEmpty() ? "0" : op.opcode;

    // 将 end 偏移转换为 for 循环上界（exclusive）
    QString endIdxExpr;
    if (endExpr == varName + ".size()") {
        endIdxExpr = "(int)" + varName + ".size()";
    } else if (endExpr.isEmpty()) {
        endIdxExpr = "(int)" + varName + ".size()";
    } else {
        endIdxExpr = "(int)(" + endExpr + ")";
    }

    // 构建注释文本（显示在动画步骤标签中）
    QString commentText;
    if (beginExpr == "0" && (endExpr.isEmpty() || endExpr == varName + ".size()")) {
        commentText = QString("\"fill(%1.begin(), %1.end(), %2)\"").arg(varName).arg(valExpr);
    } else {
        commentText = QString("\"fill(%1.begin()+%2, %1.begin()+%3, %4)\"")
            .arg(varName).arg(beginExpr).arg(endExpr).arg(valExpr);
    }

    // 生成逐步 viz::setValue 循环，使动画能逐格显示填充过程
    QString code = QString(
        "{\n"
        "    viz::comment(%1);\n"
        "    int _viz_fill_val = (int)(%2);\n"
        "    int _viz_fill_begin = %3;\n"
        "    int _viz_fill_end   = %4;\n"
        "    for (int _viz_i = _viz_fill_begin; _viz_i < _viz_fill_end; ++_viz_i) {\n"
        "        viz::setValue(%5, _viz_i, _viz_fill_val, \"%6\");\n"
        "        %6.data()[_viz_i] = _viz_fill_val;\n"
        "    }\n"
        "}\n"
    ).arg(commentText).arg(valExpr).arg(beginExpr).arg(endIdxExpr).arg(dataExpr).arg(varName);

    return code;
}


QString CodeInserter::generateVecResizeCode(const TrackedOperation& op) {
// op.index1 = 新大小表达式（如 "0", "n", "5"�?    // op.index2 = 填充值表达式（可空，resize(n) �?0；resize(n,val) �?val�?
    QString newSizeExpr = op.index1.isEmpty() ? "0" : op.index1;
    QString fillValExpr = op.index2.isEmpty() ? "0" : op.index2;
    QString varName = op.arrayName;

    // 构�?viz::vecResize �?label 参数字符串，用于界面显示
    // 例如 "resize(0)"�?resize(3)"�?resize(5, -1)"
    QString labelStr;
    if (op.index2.isEmpty()) {
        labelStr = QString("\"resize(%1)\"").arg(newSizeExpr);
    } else {
        labelStr = QString("\"resize(%1, %2)\"").arg(newSizeExpr).arg(fillValExpr);
    }

    // 生成插桩代码�?    //   1. 计算新大小到临时变量，避免表达式副作�?    //   2. 调用 viz::vecResize 发�?VecResize 事件
    //   3. 实际执行 vector::resize
    QString code;
    if (op.index2.isEmpty()) {
        // resize(n) �?填充值默认为 0
        code = QString(
            "{\n"
            "    int _viz_rsz = (int)(%1);\n"
            "    viz::vecResize(_viz_rsz, 0, %2, \"%3\");\n"
            "    %3.resize(_viz_rsz);\n"
            "}\n"
        ).arg(newSizeExpr).arg(labelStr).arg(varName);
    } else {
        // resize(n, val)
        code = QString(
            "{\n"
            "    int _viz_rsz = (int)(%1);\n"
            "    int _viz_rsz_val = (int)(%2);\n"
            "    viz::vecResize(_viz_rsz, _viz_rsz_val, %3, \"%4\");\n"
            "    %4.resize(_viz_rsz, _viz_rsz_val);\n"
            "}\n"
        ).arg(newSizeExpr).arg(fillValExpr).arg(labelStr).arg(varName);
    }
    return code;
}

QString CodeInserter::generateSortCode(const TrackedOperation& op) {
    const TrackedVariable* var = findVar(op.arrayName);
    QString varName = op.arrayName;

    // index1 = begin offset, index2 = end offset, opcode = comparator
    QString beginExpr = op.index1.isEmpty() ? "0" : op.index1;
    QString endExpr   = op.index2.isEmpty() ? varName + ".size()" : op.index2;
    QString cmpExpr   = op.opcode;

    // 构建注释文本
    QString commentText;
    if (cmpExpr.isEmpty()) {
        if (beginExpr == "0" && endExpr == varName + ".size()") {
            commentText = QString("\"sort(%1.begin(), %1.end())\"").arg(varName);
        } else {
            commentText = QString("\"sort(%1.begin()+%2, %1.begin()+%3)\"")
                .arg(varName).arg(beginExpr).arg(endExpr);
        }
    } else {
        if (beginExpr == "0" && endExpr == varName + ".size()") {
            commentText = QString("\"sort(%1.begin(), %1.end(), %2)\"")
                .arg(varName).arg(cmpExpr);
        } else {
            commentText = QString("\"sort(%1.begin()+%2, %1.begin()+%3, %4)\"")
                .arg(varName).arg(beginExpr).arg(endExpr).arg(cmpExpr);
        }
    }

    // 构建原始 sort 调用
    QString sortCall;
    if (cmpExpr.isEmpty()) {
        sortCall = QString("std::sort(%1.begin() + %2, %1.begin() + %3)")
            .arg(varName).arg(beginExpr).arg(endExpr);
    } else {
        sortCall = QString("std::sort(%1.begin() + %2, %1.begin() + %3, %4)")
            .arg(varName).arg(beginExpr).arg(endExpr).arg(cmpExpr);
    }

    // 原子执行 std::sort()，然后逐格展示结果
    return QString(
        "{\n"
        "    viz::comment(%1);\n"
        "    %2;\n"
        "    for (int _viz_i = 0; _viz_i < (int)%3.size(); ++_viz_i) {\n"
        "        viz::setValue(_viz_i, %3[_viz_i], \"%3\");\n"
        "    }\n"
        "}\n"
    ).arg(commentText).arg(sortCall).arg(varName);
}


QString CodeInserter::applyInsertions(const QString& sourceCode) {
    qDebug() << "=== applyInsertions: total insertions =" << m_insertions.size();
    for (const auto& ins : m_insertions) {
        qDebug() << "  ins offset=" << ins.offset << " replaceLen=" << ins.replaceLen
                 << " order=" << ins.order
                 << " textLen=" << ins.text.length()
                 << " textPreview=" << ins.text.left(80).replace('\n', '|');
    }

    // 将所有插入操作按 offset 升序排列
    auto sorted = m_insertions;
    std::sort(sorted.begin(), sorted.end(),
        [](const Insertion& a, const Insertion& b) {
            if (a.offset != b.offset) return a.offset < b.offset;  // 升序
            return a.order < b.order;
        });

    // 迭代构建结果：从原始源码逐段拼接，在对应 offset 处插入/替换
    QString result;
    int srcPos = 0;
    for (const auto& ins : sorted) {
        // 复制源码中从 srcPos 到 ins.offset 的部分
        if (ins.offset > srcPos) {
            result += sourceCode.mid(srcPos, ins.offset - srcPos);
        } else if (ins.offset < srcPos) {
            // 跳过（多个插入在相同或重叠位置，只取第一个）
            qDebug() << "  SKIP overlapping insertion at offset=" << ins.offset
                     << " (srcPos already at" << srcPos << ")";
            continue;
        }
        // 跳过被替换的原始代码
        srcPos = ins.offset + ins.replaceLen;
        // 插入新代码
        result += ins.text;
    }
    // 复制剩余的原始代码
    if (srcPos < sourceCode.length()) {
        result += sourceCode.mid(srcPos);
    }

    return result;
}

QString CodeInserter::generateWrapperMain(const QList<TrackedVariable>& variables) {
    QString code;
    code += "extern \"C\" __declspec(dllexport) int run_algorithm(std::vector<VizEvent>* buffer) {\n";
    code += "    viz::setEventBuffer(buffer);\n";

    // 生成测试数据
    for (const auto& var : variables) {
        if (!var.initValues.isEmpty()) {
            if (var.isVector) {
                code += QString("    %1 %2 = %3;\n")
                    .arg(var.type).arg(var.name).arg(var.initValues);
            } else if (var.isArray) {
                code += QString("    %1 %2 = %3;\n")
                    .arg(var.type).arg(var.name).arg(var.initValues);
            }
        } else {
            // 默认测试数据
            if (var.isVector) {
                code += QString("    std::vector<%1> %2 = {64, 34, 25, 12, 22, 11, 90};\n")
                    .arg(var.elementType).arg(var.name);
            } else if (var.isArray) {
                code += QString("    %1 %2[] = {64, 34, 25, 12, 22, 11, 90};\n")
                    .arg(var.elementType).arg(var.name);
            }
        }
    }

    code += "    return 0;\n";
    code += "}\n";

    return code;
}

QString CodeInserter::generateContainerCode(const TrackedOperation& op) {
    const TrackedVariable* var = findVar(op.arrayName);
    QString dataExpr = var ? getDataExpr(*var) : op.arrayName;
    QString varName = op.arrayName;

    switch (op.type) {
    case TrackedOperation::StlPushBack: {
        // push_back(x) �?记录 Resize 事件（带值） + 实际 push_back
        QString valueExpr = op.index1;  // �?"x" �?"42"
        if (valueExpr.isEmpty()) valueExpr = "0";
        return QString(
            "{\n"
            "    int _old_sz = (int)%1.size();\n"
            "    int _new_val = %2;\n"
            "    %1.push_back(_new_val);\n"
            "    viz::resize((int)%1.size(), _new_val, \"%1\");\n"
            "}\n"
        ).arg(varName).arg(valueExpr);
    }
    case TrackedOperation::StlPopBack: {
        // pop_back() �?记录 Resize 事件 + 实际 pop_back
        return QString(
            "{\n"
            "    int _old_sz = (int)%1.size();\n"
            "    %1.pop_back();\n"
            "    viz::resize((int)%1.size(), \"%1\");\n"
            "}\n"
        ).arg(varName);
    }
    case TrackedOperation::StlInsert: {
        // 动画流程：保存快�?�?真实 insert �?扩容(resize) �?逐格后移 �?写入插入�?        // insert(pos, val): index1=pos, index2=val
        // insert(pos, cnt, val): index1=pos, index2=cnt, opcode=val
        QString posExpr = op.index1.isEmpty() ? "0" : op.index1;
        if (!op.opcode.isEmpty()) {
            // insert(pos, count, val): index2=count, opcode=val
            QString cntExpr = op.index2.isEmpty() ? "1" : op.index2;
            QString valExpr = op.opcode;
            return QString(
                "{\n"
                "    int _viz_pos = (int)(%1);\n"
                "    int _viz_cnt = (int)(%2);\n"
                "    int _viz_val = (int)(%3);\n"
                "    int _viz_oldSz = (int)%4.size();\n"
                "    // 保存插入前的数据快照，用于逐格后移动画\n"
                "    std::vector<int> _viz_snap(%4);\n"
                "    // 执行真实 insert\n"
                "    %4.insert(%4.begin() + _viz_pos, _viz_cnt, _viz_val);\n"
                "    // 扩容动画（不传尾值，让新增格初始为空/0）\n"
                "    viz::resize((int)%4.size(), \"%4\");\n"
                "    // 逐格后移：从旧末尾向前，将旧值移到新位置\n"
                "    for (int _viz_i = _viz_oldSz - 1; _viz_i >= _viz_pos; --_viz_i)\n"
                "        viz::setValue(_viz_i + _viz_cnt, _viz_snap[_viz_i], \"%4\");\n"
                "    // 在插入位置写入新值\n"
                "    for (int _viz_j = 0; _viz_j < _viz_cnt; ++_viz_j)\n"
                "        viz::setValue(_viz_pos + _viz_j, _viz_val, \"%4\");\n"
                "}\n"
            ).arg(posExpr).arg(cntExpr).arg(valExpr).arg(varName);
        } else {
            // insert(pos, val): index2=val
            QString valExpr = op.index2.isEmpty() ? "0" : op.index2;
            return QString(
                "{\n"
                "    int _viz_pos = (int)(%1);\n"
                "    int _viz_val = (int)(%2);\n"
                "    int _viz_oldSz = (int)%3.size();\n"
                "    // 保存插入前的数据快照，用于逐格后移动画\n"
                "    std::vector<int> _viz_snap(%3);\n"
                "    // 执行真实 insert\n"
                "    %3.insert(%3.begin() + _viz_pos, _viz_val);\n"
                "    // 扩容动画（不传尾值，让新增格初始为空/0）\n"
                "    viz::resize((int)%3.size(), \"%3\");\n"
                "    // 逐格后移：从旧末尾向前，将旧值移到新位置\n"
                "    for (int _viz_i = _viz_oldSz - 1; _viz_i >= _viz_pos; --_viz_i)\n"
                "        viz::setValue(_viz_i + 1, _viz_snap[_viz_i], \"%3\");\n"
                "    // 在插入位置写入新值\n"
                "    viz::setValue(_viz_pos, _viz_val, \"%3\");\n"
                "}\n"
            ).arg(posExpr).arg(valExpr).arg(varName);
        }
    }
    case TrackedOperation::StlErase: {
        // erase(pos) �?erase(first, last)
        QString posExpr = op.index1.isEmpty() ? "0" : op.index1;
        if (!op.index2.isEmpty()) {
            // erase(first, last): 区间删除后逐格左移
            return QString(
                "{\n"
                "    int _viz_first = (int)(%1);\n"
                "    int _viz_last = (int)(%2);\n"
                "    int _viz_cnt = _viz_last - _viz_first;\n"
                "    %3.erase(%3.begin() + _viz_first, %3.begin() + _viz_last);\n"
                "    viz::resize((int)%3.size(), \"%3\");\n"
                "    for (int _viz_i = _viz_first; _viz_i < (int)%3.size(); ++_viz_i)\n"
                "        viz::setValue(_viz_i, %3[_viz_i], \"%3\");\n"
                "}\n"
            ).arg(posExpr).arg(op.index2).arg(varName);
        } else {
            // erase(pos): 单元素删除后逐格左移
            return QString(
                "{\n"
                "    int _viz_pos = (int)(%1);\n"
                "    %2.erase(%2.begin() + _viz_pos);\n"
                "    viz::resize((int)%2.size(), \"%2\");\n"
                "    for (int _viz_i = _viz_pos; _viz_i < (int)%2.size(); ++_viz_i)\n"
                "        viz::setValue(_viz_i, %2[_viz_i], \"%2\");\n"
                "}\n"
            ).arg(posExpr).arg(varName);
        }
    }
    case TrackedOperation::StlClear: {
        // clear() �?viz::clearContainer() + 实际 clear()
        return QString(
            "{\n"
            "    viz::clearContainer(\"%1\");\n"
            "    %1.clear();\n"
            "}\n"
        ).arg(varName);
    }
    case TrackedOperation::StlEmplaceBack: {
// emplace_back(args...) �?类似 push_back，但参数是构造参�?
    QString argsExpr = op.index1.isEmpty() ? "" : op.index1;
        return QString(
            "{\n"
            "    int _old_sz = (int)%1.size();\n"
            "    %1.emplace_back(%2);\n"
            "    viz::resize((int)%1.size(), %1.back(), \"%1\");\n"
            "}\n"
        ).arg(varName).arg(argsExpr);
    }
    default:
        return "";
    }
}

const TrackedVariable* CodeInserter::findVar(const QString& name) const {
    for (const auto& var : m_variables) {
        if (var.name == name) return &var;
    }
    return nullptr;
}

QString CodeInserter::generateAtCode(const TrackedOperation& op) {
    // at(i): 替换 v.at(i) �?(viz::mark(i, "at", "v"), v.data()[i])
// comma-operator �?C++17 保留右操作数值类别，因此可作 lvalue 用于赋值左�?
    QString idxExpr = op.index1.isEmpty() ? "0" : op.index1;
    return QString(
        "(viz::mark(%1, \"at\", \"%2\"), %2.data()[%1])"
    ).arg(idxExpr).arg(op.arrayName);
}

QString CodeInserter::generateBinarySearchCode(const TrackedOperation& op) {
    // binary_search: index1=begin, index2=end, opcode=search value
    // otherArrayName: 目标变量名（如 found），otherIndex: 目标变量类型（如 bool/auto）
    QString beginExpr = op.index1.isEmpty() ? "0" : op.index1;
    QString endExpr = op.index2.isEmpty() ? op.arrayName + ".size()" : op.index2;
    QString valExpr = op.opcode.isEmpty() ? "0" : op.opcode;

    // 若是赋值上下文（bool found = binary_search(...)），生成包含变量声明的完整块
    // otherArrayName 存目标变量名，otherIndex 存类型（如 bool/auto）
    bool isAssignment = !op.otherArrayName.isEmpty();
    QString targetVarType = op.otherIndex.isEmpty() ? "bool" : op.otherIndex;
    // 若类型含 'auto'，推断为 bool（binary_search 返回 bool）
    if (targetVarType == "auto" || targetVarType.isEmpty()) targetVarType = "bool";
    QString targetVarName = op.otherArrayName.isEmpty() ? "_viz_bs_result" : op.otherArrayName;

    QString innerCode = QString(
        "    int _viz_lo = (int)(%1);\n"
        "    int _viz_hi = (int)(%2);\n"
        "    int _viz_val = (int)(%3);\n"
        "    bool _viz_result = false;\n"
        "    while (_viz_lo < _viz_hi) {\n"
        "        int _viz_mid = _viz_lo + (_viz_hi - _viz_lo) / 2;\n"
        "        viz::mark(_viz_mid, \"mid\", \"%4\");\n"
        "        if (%4[_viz_mid] < _viz_val) {\n"
        "            _viz_lo = _viz_mid + 1;\n"
        "        } else if (%4[_viz_mid] > _viz_val) {\n"
        "            _viz_hi = _viz_mid;\n"
        "        } else {\n"
        "            _viz_result = true;\n"
        "            break;\n"
        "        }\n"
        "    }\n"
    ).arg(beginExpr).arg(endExpr).arg(valExpr).arg(op.arrayName);

    if (isAssignment) {
        // 生成包含变量声明的完整块：替换整个 "bool found = binary_search(...)" 语句
        return QString(
            "{\n"
            "%1"
            "    %2 %3 = _viz_result;\n"
            "}\n"
        ).arg(innerCode).arg(targetVarType).arg(targetVarName);
    } else {
        // 独立语句：只有 binary_search(...)，无赋值
        return QString(
            "{\n"
            "%1"
            "    _viz_result;\n"
            "}\n"
        ).arg(innerCode);
    }
}

QString CodeInserter::generateFindCode(const TrackedOperation& op) {
    // find: index1=begin, index2=end, opcode=search value
    // otherArrayName: 目标变量名（如 it），otherIndex: 目标变量类型（如 auto/vector<int>::iterator）
    QString beginExpr = op.index1.isEmpty() ? "0" : op.index1;
    QString endExpr = op.index2.isEmpty() ? op.arrayName + ".size()" : op.index2;
    QString valExpr = op.opcode.isEmpty() ? "0" : op.opcode;

    bool isAssignment = !op.otherArrayName.isEmpty();
    QString targetVarName = op.otherArrayName.isEmpty() ? "_viz_find_it" : op.otherArrayName;

    // find 返回迭代器（vector<int>::iterator），不适合用 auto 直接赋值整数索引
    // 我们将找到的索引存为 int，并定义目标变量为迭代器：arrName.begin() + _viz_i
    // 若目标变量类型是 auto，实际上是迭代器类型，我们用 auto 兼容
    QString targetVarType = op.otherIndex.isEmpty() ? "auto" : op.otherIndex;

    QString innerCode = QString(
        "    int _viz_val = (int)(%1);\n"
        "    int _viz_i = (int)(%2);\n"
        "    int _viz_end = (int)(%3);\n"
        "    for (; _viz_i < _viz_end; ++_viz_i) {\n"
        "        viz::mark(_viz_i, \"find\", \"%4\");\n"
        "        if (%4[_viz_i] == _viz_val) break;\n"
        "    }\n"
    ).arg(valExpr).arg(beginExpr).arg(endExpr).arg(op.arrayName);

    if (isAssignment) {
        // 生成包含变量声明的完整块：替换整个 "auto it = find(...)" 语句
        // find 返回迭代器，我们将结果声明为 auto（指向正确的迭代器位置）
        return QString(
            "{\n"
            "%1"
            "    %2 %3 = %4.begin() + _viz_i;\n"
            "}\n"
        ).arg(innerCode).arg(targetVarType).arg(targetVarName).arg(op.arrayName);
    } else {
        // 独立语句：只有 find(...)，无赋值
        return QString(
            "{\n"
            "%1"
            "    (%2.begin() + _viz_i);\n"
            "}\n"
        ).arg(innerCode).arg(op.arrayName);
    }
}

QString CodeInserter::generateCopyCode(const TrackedOperation& op) {
    // copy: arrayName=src, otherArrayName=dst
    QString srcName = op.arrayName;
    QString dstName = op.otherArrayName;
    QString srcBegin = op.index1.isEmpty() ? "0" : op.index1;
    QString srcEnd = op.index2.isEmpty() ? srcName + ".size()" : op.index2;
    QString dstBegin = op.otherIndex.isEmpty() ? "0" : op.otherIndex;

    return QString(
        "{\n"
        "    int _viz_src_beg = (int)(%1);\n"
        "    int _viz_src_end = (int)(%2);\n"
        "    int _viz_dst_beg = (int)(%3);\n"
        "    for (int _viz_i = 0; _viz_i < _viz_src_end - _viz_src_beg; ++_viz_i) {\n"
        "        int _viz_val = %4[_viz_src_beg + _viz_i];\n"
        "        viz::setValue(%5.data(), _viz_dst_beg + _viz_i, _viz_val, \"%6\");\n"
        "        %5[_viz_dst_beg + _viz_i] = _viz_val;\n"
        "    }\n"
        "    (%5.begin() + _viz_dst_beg);\n"
        "}\n"
    ).arg(srcBegin).arg(srcEnd).arg(dstBegin).arg(srcName).arg(dstName).arg(dstName);
}

QString CodeInserter::generateContainerSwapCode(const TrackedOperation& op) {
    // swap(v1, v2): 整体交换
    QString name1 = op.arrayName;
    QString name2 = op.otherArrayName;

    return QString(
        "{\n"
        "    std::swap(%1, %2);\n"
        "    viz::array(%1, \"%1\");\n"
        "    viz::array(%2, \"%2\");\n"
        "}\n"
    ).arg(name1).arg(name2);
}

QString CodeInserter::generateRemoveCode(const TrackedOperation& op) {
    // remove: index1=begin, index2=end, opcode=value to remove
    QString beginExpr = op.index1.isEmpty() ? "0" : op.index1;
    QString endExpr = op.index2.isEmpty() ? op.arrayName + ".size()" : op.index2;
    QString valExpr = op.opcode.isEmpty() ? "0" : op.opcode;

    return QString(
        "{\n"
        "    int _viz_val = (int)(%1);\n"
        "    auto _viz_it = std::remove(%2.begin() + %3, %2.begin() + %4, _viz_val);\n"
        "    int _viz_new_end = (int)(_viz_it - %2.begin());\n"
        "    // 更新有效区域的值\n"
        "    for (int _viz_i = 0; _viz_i < _viz_new_end; ++_viz_i)\n"
        "        viz::setValue(_viz_i, %2[_viz_i], \"%2\");\n"
        "    // std::remove 不改变容器 size，尾部元素逻辑删除但仍在容器中\n"
        "    for (int _viz_i = _viz_new_end; _viz_i < (int)%2.size(); ++_viz_i)\n"
        "        viz::mark(_viz_i, \"removed\", \"%2\");\n"
        "    _viz_it;\n"
        "}\n"
    ).arg(valExpr).arg(op.arrayName).arg(beginExpr).arg(endExpr);
}

QString CodeInserter::generateReplaceCode(const TrackedOperation& op) {
    // replace: index1=begin, index2=end, opcode=old_val, otherIndex=new_val
    QString beginExpr = op.index1.isEmpty() ? "0" : op.index1;
    QString endExpr = op.index2.isEmpty() ? op.arrayName + ".size()" : op.index2;
    QString oldVal = op.opcode.isEmpty() ? "0" : op.opcode;
    QString newVal = op.otherIndex.isEmpty() ? "0" : op.otherIndex;

    return QString(
        "{\n"
        "    int _viz_old = (int)(%1);\n"
        "    int _viz_new = (int)(%2);\n"
        "    for (int _viz_i = %3; _viz_i < %4; ++_viz_i) {\n"
        "        if (%5[_viz_i] == _viz_old) {\n"
        "            viz::setValue(_viz_i, _viz_new, \"%5\");\n"
        "            %5[_viz_i] = _viz_new;\n"
        "        }\n"
        "    }\n"
        "}\n"
    ).arg(oldVal).arg(newVal).arg(beginExpr).arg(endExpr).arg(op.arrayName);
}

QString CodeInserter::generateRotateCode(const TrackedOperation& op) {
    // rotate: index1=begin, index2=middle, opcode=end
    QString beginExpr = op.index1.isEmpty() ? "0" : op.index1;
    QString middleExpr = op.index2.isEmpty() ? "0" : op.index2;
    QString endExpr = op.opcode.isEmpty() ? op.arrayName + ".size()" : op.opcode;

    return QString(
        "{\n"
        "    int _viz_beg = (int)(%1);\n"
        "    int _viz_mid = (int)(%2);\n"
        "    int _viz_end = (int)(%3);\n"
        "    std::rotate(%4.begin() + _viz_beg, %4.begin() + _viz_mid, %4.begin() + _viz_end);\n"
        "    for (int _viz_i = _viz_beg; _viz_i < _viz_end; ++_viz_i)\n"
        "        viz::setValue(_viz_i, %4[_viz_i], \"%4\");\n"
        "    (%4.begin() + _viz_beg);\n"
        "}\n"
    ).arg(beginExpr).arg(middleExpr).arg(endExpr).arg(op.arrayName);
}

QString CodeInserter::generateCrossOpCode(const TrackedOperation& op) {
    const TrackedVariable* var = findVar(op.arrayName);
    QString dataExpr = var ? getDataExpr(*var) : op.arrayName;
    const TrackedVariable* otherVar = findVar(op.otherArrayName);
    QString otherDataExpr = otherVar ? getDataExpr(*otherVar) : op.otherArrayName;

    if (op.type == TrackedOperation::CrossCompare) {
        // 跨数组比较 arr[i] > brr[j]
        // viz::crossCompare 返回 void，不能直接用做 if 条件，需用逗号运算符
        // (viz::crossCompare(...), arr[i] > brr[j]) ——先记录可视化事件，再返回实际比较结果
        return QString(
            "(viz::crossCompare(%1, %2, %3, %4, \"%5\", \"%6\"), %5[%2] %7 %8[%4])"
        ).arg(dataExpr).arg(op.index1)
         .arg(otherDataExpr).arg(op.otherIndex)
         .arg(op.arrayName).arg(op.otherArrayName)
         .arg(op.opcode).arg(op.otherArrayName);
    }
    if (op.type == TrackedOperation::CrossAssign) {
        // 跨数组赋�? arr[i] = brr[j]
        // 先读�?brr[j] 的值，再用带指针的 setValue 写入 arr[i]
        return QString(
            "{\n"
            "    int _viz_cv = %1[%2];\n"
            "    viz::setValue(%3, %4, _viz_cv, \"%5\");\n"
            "    %1[%2] = _viz_cv;\n"
            "}\n"
        ).arg(op.otherArrayName).arg(op.otherIndex)
         .arg(dataExpr).arg(op.index1).arg(op.arrayName);
    }
    return "";
}

// �������Ӹ���λ�ÿ�ʼ���ҵ�ƥ��� } (brace counting)
static int findMatchingBrace(const QString& source, int openBracePos) {
    int depth = 1;
    int pos = openBracePos + 1;
    while (pos < source.length() && depth > 0) {
        if (source[pos] == '{') depth++;
        else if (source[pos] == '}') depth--;
        pos++;
    }
    return (depth == 0) ? pos - 1 : -1;
}

void CodeInserter::insertParamCleanup(const QString& sourceCode,
                                       const QList<TrackedVariable>& variables) {
    // 收集值参数（isParameter=true 且非引用参数）
    QList<TrackedVariable> valueParams;
    for (const auto& var : variables) {
        if (var.isParameter && !var.isReferenceParam) {
            valueParams.append(var);
        }
    }
    if (valueParams.isEmpty()) return;

    // ��ƫ��λ�÷��飺ͬλ�ö�������ϲ�Ϊһ������
    QMap<int, QStringList> cleanupByOffset;

    for (const auto& vp : valueParams) {
        // defEndOffset �Ǻ����� { ֮���λ�ã��� 1 �õ� { λ��
        int openBrace = vp.defEndOffset - 1;
        if (openBrace < 0 || openBrace >= sourceCode.length()) continue;
        if (sourceCode[openBrace] != '{') continue;

        int closeBrace = findMatchingBrace(sourceCode, openBrace);
        if (closeBrace < 0) continue;

        QString destroyCall = QString("viz::destroyArray(\"%1\");").arg(vp.name);

        // �� } ǰ��������
        cleanupByOffset[closeBrace].append(destroyCall);

        // ɨ�躯���������� return �ؼ���
        int searchPos = openBrace + 1;
        int funcEnd = closeBrace;
        while (searchPos < funcEnd) {
            int retPos = sourceCode.indexOf("return", searchPos);
            if (retPos < 0 || retPos >= funcEnd) break;

            // ��֤�Ƕ����ؼ���
            bool validBefore = (retPos == 0) ||
                (!sourceCode[retPos - 1].isLetterOrNumber() && sourceCode[retPos - 1] != '_');
            bool validAfter = (retPos + 6 >= sourceCode.length()) ||
                (!sourceCode[retPos + 6].isLetterOrNumber() && sourceCode[retPos + 6] != '_');

            if (validBefore && validAfter) {
                cleanupByOffset[retPos].append(destroyCall);
            }
            searchPos = retPos + 6;
        }
    }

    // ���ɲ��루order 5000 ȷ���ڲ����滻������
    for (auto it = cleanupByOffset.begin(); it != cleanupByOffset.end(); ++it) {
        Insertion ins;
        ins.offset = it.key();
        ins.text = it.value().join(" ");
        ins.order = 5000;
        m_insertions.append(ins);
    }
}

void CodeInserter::insertCallAliases(const QString& sourceCode,
                                      const QList<CallSiteInfo>& callSites,
                                      const QMap<QString, QList<ParamInfo>>& functionParams) {
    for (const auto& cs : callSites) {
        if (!functionParams.contains(cs.funcName)) continue;
        const auto& params = functionParams[cs.funcName];
        
        // 检查此函数是否有引用参数，且对应实参是追踪变量
        bool needsAlias = false;
        for (int i = 0; i < params.size(); i++) {
            if (params[i].isReference) {
                // 实参是追踪变量的简单名称（不是任意表达式）
                if (i < cs.argExprs.size()) {
                    // 简单参数名匹配即可
                    needsAlias = true;
                    break;
                }
            }
        }
        if (!needsAlias) continue;
        
        // 生成 alias push 代码（插入到调用语句之前）
        QString pushCode;
        for (int i = 0; i < params.size() && i < cs.argExprs.size(); i++) {
            if (params[i].isReference && !cs.argExprs[i].isEmpty()) {
                pushCode += QString("viz::pushAlias(\"%1\", \"%2\"); ")
                    .arg(params[i].name).arg(cs.argExprs[i]);
            }
        }
        
        // 生成 alias pop 代码（插入到调用语句之后）
        QString popCode;
        for (int i = 0; i < params.size() && i < cs.argExprs.size(); i++) {
            if (params[i].isReference && !cs.argExprs[i].isEmpty()) {
                popCode += QString("viz::popAlias(\"%1\"); ").arg(params[i].name);
            }
        }
        
        if (pushCode.isEmpty()) continue;
        
        // pushAlias 在调用之前（beginOffset），popAlias 在调用之后（endOffset）
        Insertion pushIns;
        pushIns.offset = cs.beginOffset;
        pushIns.text = pushCode;
        pushIns.order = 4900;  // 在 cleanup (5000) 之前
        m_insertions.append(pushIns);
        
        Insertion popIns;
        popIns.offset = cs.endOffset;
        popIns.text = popCode;
        popIns.order = 4901;
        m_insertions.append(popIns);
        
        qDebug() << "[CallAliases]" << cs.funcName
                 << "push:" << pushCode << "pop:" << popCode
                 << "offset:" << cs.beginOffset << "-" << cs.endOffset;
    }
}