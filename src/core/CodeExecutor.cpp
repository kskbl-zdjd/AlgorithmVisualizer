#include "CodeExecutor.h"
#include "AstAnalyzer.h"
#include "CodeInserter.h"
#include "AnimationController.h"
#include "../visualizers/ArrayVisualizer.h"
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <QColor>
#include <QDebug>
#include <QRegularExpression>

// DLL 函数指针类型
typedef int (*RunAlgorithmFunc)(std::vector<VizEvent>*);

CodeExecutor::CodeExecutor(QObject* parent)
    : QObject(parent)
{
}

CodeExecutor::~CodeExecutor() {
    cleanup();
}

bool CodeExecutor::execute(const QString& userCode, QString& error) {
    // 1. 清理上次执行
    cleanup();

    // 2. 创建新的临时目录
    m_tempDir = QDir::temp().filePath(
        QString("algo_viz_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz")));
    if (!QDir().mkpath(m_tempDir)) {
        error = "无法创建临时目录";
        return false;
    }

    // 3. 生成插桩代码（Clang AST 分析 + 插桩）
    QString wrapperPath = QDir(m_tempDir).filePath("wrapper.cpp");
    if (!generateWrapper(userCode, wrapperPath)) {
        error = "生成包装代码失败";
        return false;
    }

    // 4. 编译
    if (!compile(wrapperPath, error)) {
        return false;
    }

    // 5. 加载并执行 DLL
    if (!runDll(error)) {
        return false;
    }

    emit executionFinished(true, "");
    return true;
}

void CodeExecutor::cleanup() {
    // 卸载 DLL
    if (m_library) {
        m_library->unload();
        delete m_library;
        m_library = nullptr;
    }

    // 删除临时目录
    if (!m_tempDir.isEmpty() && QDir(m_tempDir).exists()) {
        QDir(m_tempDir).removeRecursively();
    }

    m_dllPath.clear();
    m_events.clear();
    m_transformedCode.clear();
    m_trackedVariables.clear();
}

bool CodeExecutor::generateWrapper(const QString& userCode, const QString& wrapperPath) {
    // 确保目录存在
    QFileInfo fileInfo(wrapperPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }

    // ===== 阶段1：Clang AST 分析 =====
    AstAnalyzer analyzer;
    bool astOk = analyzer.analyze(userCode);
    
    if (astOk) {
        qDebug() << "AST Analysis:"
                 << "variables=" << analyzer.getVariables().size()
                 << "operations=" << analyzer.getOperations().size()
                 << "hasMain=" << analyzer.hasMainFunction();

        // 存储变量列表（用于多数组UI初始化）
        m_trackedVariables = analyzer.getVariables();

        // ===== 阶段2：基于 offset 精确插桩 =====
        CodeInserter inserter;
        m_transformedCode = inserter.insert(
            userCode,
            analyzer.getVariables(),
            analyzer.getOperations(),
            analyzer.hasMainFunction(),
            analyzer.getFunctionParams(),
            analyzer.getCallSites()
        );
    } else {
        qDebug() << "AST analysis failed:" << analyzer.getLastError();
        qDebug() << "Falling back to basic wrapper generation";
        
        // 回退方案：生成基础包装代码（无自动插桩）
        QList<TrackedVariable> emptyVars;
        QList<TrackedOperation> emptyOps;
        bool hasMain = userCode.contains(QRegularExpression(R"(\bint\s+main\s*\()"));
        
        CodeInserter inserter;
        m_transformedCode = inserter.insert(
            userCode,
            emptyVars,
            emptyOps,
            hasMain
        );
    }
    
    qDebug() << "=== Transformed Code ===\n" << m_transformedCode << "\n=======================";

    // 写转换后代码到已知文件方便调试
    {
        QString debugPath = QDir::temp().filePath("algo_viz_transformed_debug.cpp");
        QFile debugFile(debugPath);
        if (debugFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ds(&debugFile);
            ds << m_transformedCode;
            debugFile.close();
            qDebug() << "Transformed code also written to:" << debugPath;
        }
    }

    QFile file(wrapperPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << m_transformedCode;
    file.close();
    return true;
}

bool CodeExecutor::compile(const QString& sourcePath, QString& error) {
    // MinGW 编译器路径
    QString gppPath = "E:/QT/qt/Tools/mingw1310_64/bin/g++.exe";

    m_dllPath = QDir(m_tempDir).filePath("user_code.dll");

    // viz_api 文件路径
    QString vizApiDir = "E:/QTproject/algo-visualizer/src/recorder";
    QString vizApiCpp = vizApiDir + "/viz_api.cpp";

    QStringList arguments;
    arguments << "-shared";
    arguments << "-fPIC";
    arguments << "-o" << m_dllPath;
    arguments << sourcePath;           // wrapper.cpp
    arguments << vizApiCpp;            // viz_api.cpp（纯 C++ 实现）
    arguments << "-I" << vizApiDir;    // 头文件路径
    arguments << "-std=c++17";
    arguments << "-O2";
    // 注意：不需要 Qt 的头文件路径和库！

    QProcess process;
    process.setProgram(gppPath);
    process.setArguments(arguments);
    process.setWorkingDirectory(m_tempDir);

    process.start();
    if (!process.waitForFinished(30000)) {
        process.kill();
        error = "编译超时";
        return false;
    }

    if (process.exitCode() != 0) {
        error = QString("编译错误:\n%1").arg(QString(process.readAllStandardError()));
        return false;
    }

    return true;
}

bool CodeExecutor::runDll(QString& error) {
    // 加载 DLL
    m_library = new QLibrary(m_dllPath);
    if (!m_library->load()) {
        error = QString("加载 DLL 失败: %1").arg(m_library->errorString());
        return false;
    }

    // 获取函数指针
    RunAlgorithmFunc runFunc = (RunAlgorithmFunc)m_library->resolve("run_algorithm");
    if (!runFunc) {
        error = "无法解析 run_algorithm 函数";
        return false;
    }

    // 清空事件列表
    m_events.clear();

    // 执行用户代码
    try {
        int result = runFunc(&m_events);
        if (result != 0) {
            error = "用户代码执行返回错误";
            return false;
        }
    } catch (const std::exception& e) {
        error = QString("运行时异常: %1").arg(e.what());
        return false;
    } catch (...) {
        error = "未知运行时异常";
        return false;
    }

    return true;
}

void CodeExecutor::convertEventsToSteps(QMap<QString, ArrayVisualizer*>& arrVizMap,
                                        AnimationController* animCtrl,
                                        const QSet<QString>& localVarNames) {
    animCtrl->clearSteps();

    // 获取默认视觉器（第一个，用于回退）
    ArrayVisualizer* defaultViz = nullptr;
    if (!arrVizMap.isEmpty()) {
        defaultViz = arrVizMap.first();
    }

    // 本地变量快照：当本地变量与值参数共用面板时，
    // ArrayCreate 事件中先记录本地变量的数据快照，
    // ArrayDestroy 时（值参数销毁）恢复该快照而非 hide 面板
    // key: ArrayVisualizer*, value: 本地变量最近一次 ArrayCreate 的数据
    QMap<ArrayVisualizer*, std::vector<int>> localVarSnapshot;

    for (const auto& e : m_events) {
        AnimationStep step;

        // 根据 arrayName 路由到对应 ArrayVisualizer
        QString arrName(e.arrayName);
        ArrayVisualizer* arrViz = arrVizMap.value(arrName, nullptr);
        if (!arrViz) {
            // 回退：未找到对应数组的视觉器，使用默认
            arrViz = defaultViz;
        }
        if (!arrViz) continue;  // 没有可用视觉器，跳过

        QString stepPrefix;
        if (!arrName.isEmpty() && arrVizMap.size() > 1) {
            stepPrefix = QString("[%1] ").arg(arrName);
        }

        // 是否是"本地变量共用面板"的情况（面板名在 localVarNames 里）
        bool sharedWithLocal = localVarNames.contains(arrName);

        switch (e.type) {
        case VizEventType::ArrayCreate: {
            // 从固定数组构建 vector（安全跨 DLL 边界）
            std::vector<int> data;
            data.reserve(e.arrayDataCount);
            for (int i = 0; i < e.arrayDataCount; i++) {
                data.push_back(e.arrayData[i]);
            }

            // 如果面板与本地变量共享（值传递参数同名），记录快照
            if (sharedWithLocal) {
                localVarSnapshot[arrViz] = data;
            }

            step.execute = [arrViz, data, sharedWithLocal]() {
                // 对于本地变量的 ArrayCreate：面板已经可见，直接更新数据
                // 对于值参数（sharedWithLocal=false）的 ArrayCreate：show 面板并更新
                if (!sharedWithLocal) {
                    if (arrViz->parentWidget()) arrViz->parentWidget()->show();
                }
                arrViz->setData(data);
            };
            step.undo = [arrViz, sharedWithLocal]() {
                if (!sharedWithLocal) {
                    if (arrViz->parentWidget()) arrViz->parentWidget()->hide();
                }
                arrViz->setData({});
            };
            step.description = stepPrefix + QString("创建数组 %1（大小=%2）").arg(e.arrayName).arg(e.arrayDataCount);
            break;
        }

        case VizEventType::SetValue:
            step.execute = [arrViz, e]() {
                arrViz->resetAllBlockStates();
                arrViz->setBlockState(e.index1, BlockState::Active);
                arrViz->setValue(e.index1, e.value);
            };
            step.undo = [arrViz, e]() {
                arrViz->setValue(e.index1, 0);  // 恢复为0（初始值）
                arrViz->setBlockState(e.index1, BlockState::Normal);
            };
            step.description = stepPrefix + QString("设置 arr[%1] = %2").arg(e.index1).arg(e.value);
            break;

        case VizEventType::Swap:
            step.execute = [arrViz, e]() {
                arrViz->resetAllBlockStates();
                arrViz->setBlockState(e.index1, BlockState::Active);
                arrViz->setBlockState(e.index2, BlockState::Active);
                arrViz->swapValues(e.index1, e.index2);
            };
            step.undo = [arrViz, e]() {
                arrViz->swapValues(e.index1, e.index2);
                arrViz->setBlockState(e.index1, BlockState::Normal);
                arrViz->setBlockState(e.index2, BlockState::Normal);
            };
            step.description = stepPrefix + QString("交换 arr[%1] 和 arr[%2]").arg(e.index1).arg(e.index2);
            break;

        case VizEventType::Compare: {
            // 跨数组比较：comment 中包含对方数组名
            QString otherArr(e.comment);
            ArrayVisualizer* otherViz = nullptr;
            if (!otherArr.isEmpty()) {
                otherViz = arrVizMap.value(otherArr, nullptr);
            }
            step.execute = [arrViz, e, otherViz]() {
                arrViz->resetAllBlockStates();
                arrViz->setBlockState(e.index1, BlockState::Active);
                arrViz->setBlockState(e.index2, BlockState::Active);
                if (otherViz) {
                    otherViz->resetAllBlockStates();
                    otherViz->setBlockState(e.index1, BlockState::Active);
                    otherViz->setBlockState(e.index2, BlockState::Active);
                }
            };
            step.undo = [arrViz, e, otherViz]() {
                arrViz->setBlockState(e.index1, BlockState::Normal);
                arrViz->setBlockState(e.index2, BlockState::Normal);
                if (otherViz) {
                    otherViz->setBlockState(e.index1, BlockState::Normal);
                    otherViz->setBlockState(e.index2, BlockState::Normal);
                }
            };
            if (!otherArr.isEmpty()) {
                step.description = stepPrefix + QString("跨数组比较: %1[%2] vs %3[%4]")
                    .arg(arrName).arg(e.index1).arg(otherArr).arg(e.index2);
            } else {
                step.description = stepPrefix + QString("比较 arr[%1] 和 arr[%2]").arg(e.index1).arg(e.index2);
            }
            break;
        }

        case VizEventType::Mark:
            step.execute = [arrViz, e]() {
                arrViz->setArrows({{e.index1, QString(e.label)}});
            };
            step.undo = [arrViz]() {
                arrViz->clearArrows();
            };
            step.description = stepPrefix + QString("标记 %1 指向 %2").arg(e.label).arg(e.index1);
            break;

        case VizEventType::Unmark:
            step.execute = [arrViz]() {
                arrViz->clearArrows();
            };
            step.description = stepPrefix + "取消标记";
            break;

        case VizEventType::UnmarkAll:
            step.execute = [arrViz]() {
                arrViz->clearArrows();
            };
            step.description = stepPrefix + "清除所有标记";
            break;

        case VizEventType::RecurseEnter:
            step.execute = [arrViz, e]() {
                arrViz->setDashedBoxes({
                    {e.index1, e.index2, "处理中", QColor("#F44336")}
                });
            };
            step.undo = [arrViz]() {
                arrViz->clearDashedBoxes();
            };
            step.description = stepPrefix + QString("进入递归 [%1, %2]").arg(e.index1).arg(e.index2);
            break;

        case VizEventType::RecursePending:
            step.execute = [arrViz, e]() {
                arrViz->setDashedBoxes({
                    {e.index1, e.index2, "待处理", QColor("#4CAF50")}
                });
            };
            step.description = stepPrefix + QString("标记待处理 [%1, %2]").arg(e.index1).arg(e.index2);
            break;

        case VizEventType::RecurseLeave:
            step.execute = [arrViz]() {
                arrViz->clearDashedBoxes();
            };
            step.description = stepPrefix + "离开递归";
            break;

        case VizEventType::Highlight:
            step.execute = [arrViz, e]() {
                arrViz->resetAllBlockStates();
                arrViz->setBlockState(e.index1, BlockState::Active);
            };
            step.undo = [arrViz, e]() {
                arrViz->setBlockState(e.index1, BlockState::Normal);
            };
            step.description = stepPrefix + QString("高亮 %1").arg(e.index1);
            break;

        case VizEventType::Reset:
            step.execute = [arrViz]() {
                arrViz->resetAllBlockStates();
            };
            step.description = stepPrefix + "重置状态";
            break;

        case VizEventType::Comment:
            step.execute = nullptr;
            step.description = QString(e.comment);
            break;

        case VizEventType::ArrayDestroy: {
            // 值参数生命周期结束（函数退出）
            // 若面板与本地变量共享：恢复本地变量的数据快照，面板保持可见
            // 否则：隐藏面板
            std::vector<int> snapshot;
            bool hasSnapshot = localVarSnapshot.contains(arrViz);
            if (hasSnapshot) {
                snapshot = localVarSnapshot[arrViz];
            }

            if (hasSnapshot) {
                // 共享面板：恢复原始本地变量数据，面板保持可见
                step.execute = [arrViz, snapshot]() {
                    arrViz->resetAllBlockStates();
                    arrViz->setData(snapshot);
                };
                step.undo = [arrViz, snapshot]() {
                    // undo 时（倒播）：什么都不做，因为接下来会有对应 ArrayCreate 的 undo 恢复数据
                    // 不需要特别处理
                    (void)snapshot;
                };
                step.description = stepPrefix + QString("销毁参数副本 %1（函数退出）→ 恢复原数组").arg(QString(e.arrayName));
            } else {
                // 独立面板：直接隐藏
                step.execute = [arrViz]() {
                    if (arrViz->parentWidget()) arrViz->parentWidget()->hide();
                };
                step.undo = [arrViz]() {
                    if (arrViz->parentWidget()) arrViz->parentWidget()->show();
                };
                step.description = stepPrefix + QString("销毁数组 %1（函数退出）").arg(QString(e.arrayName));
            }
            break;
        }

        case VizEventType::Resize:
            step.execute = [arrViz, e]() {
                int newSz = e.newSize;
                int oldSz = e.oldSize;
                auto data = arrViz->getData();
                int actualOldSz = (int)data.size();
                if (actualOldSz < oldSz) {
                    oldSz = actualOldSz;
                }
                if (newSz > oldSz) {
                    data.resize(newSz, 0);
                    if (newSz == oldSz + 1) {
                        data[newSz - 1] = e.value;
                    }
                    arrViz->resize(newSz, data);
                } else if (newSz < oldSz) {
                    arrViz->resize(newSz);
                }
            };
            step.undo = [arrViz, e]() {
                int oldSz = e.oldSize;
                auto data = arrViz->getData();
                data.resize(oldSz, 0);
                arrViz->resize(oldSz, data);
            };
            step.description = stepPrefix + QString("数组大小: %1 → %2")
                .arg(e.oldSize).arg(e.newSize);
            break;

        case VizEventType::Clear:
            step.execute = [arrViz]() {
                arrViz->showClearState();
            };
            step.undo = [arrViz, e]() {
                int oldSz = e.oldSize;
                auto data = std::vector<int>(oldSz, 0);
                arrViz->resize(oldSz, data);
                arrViz->hideClearState();
            };
            step.description = stepPrefix + QString("清空数组（原大小: %1）").arg(e.oldSize);
            break;

        case VizEventType::VecResize: {
            int newSz  = e.newSize;
            int oldSz  = e.oldSize;
            int fillV  = e.value;
            QString lbl = QString(e.comment);
            step.execute = [arrViz, newSz, oldSz, fillV, lbl]() {
                auto data = arrViz->getData();
                int actualOld = (int)data.size();
                if (newSz == 0) {
                    arrViz->showResizeZeroState(lbl);
                } else if (newSz > actualOld) {
                    data.resize(newSz, fillV);
                    arrViz->hideResizeZeroState();
                    arrViz->resize(newSz, data);
                } else if (newSz < actualOld) {
                    data.resize(newSz);
                    arrViz->hideResizeZeroState();
                    arrViz->resize(newSz, data);
                }
            };
            step.undo = [arrViz, oldSz, lbl]() {
                if (oldSz == 0) {
                    arrViz->showResizeZeroState(lbl);
                } else {
                    auto data = std::vector<int>(oldSz, 0);
                    arrViz->hideResizeZeroState();
                    arrViz->resize(oldSz, data);
                }
            };
            if (newSz == 0) {
                step.description = stepPrefix + (lbl.isEmpty() ? "resize(0)" : lbl);
            } else if (newSz > oldSz) {
                step.description = stepPrefix + QString("扩容: %1 → %2（填充值 %3）").arg(oldSz).arg(newSz).arg(fillV);
            } else {
                step.description = stepPrefix + QString("缩容: %1 → %2").arg(oldSz).arg(newSz);
            }
            break;
        }

        default:
            continue;
        }

        animCtrl->addStep(step);
    }
}
