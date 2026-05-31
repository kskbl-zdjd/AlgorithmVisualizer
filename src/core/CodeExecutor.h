#ifndef CODE_EXECUTOR_H
#define CODE_EXECUTOR_H

#include <QObject>
#include <QString>
#include <QSet>
#include <QLibrary>
#include <QMap>
#include "../recorder/VizEvent.h"
#include "AstAnalyzer.h"

// 前向声明，避免循环依赖
class AnimationController;
class ArrayVisualizer;
#include <QMap>

class CodeExecutor : public QObject {
    Q_OBJECT

public:
    explicit CodeExecutor(QObject* parent = nullptr);
    ~CodeExecutor();

    // 编译并执行用户代码（Clang AST 分析 + 插桩 + 编译 + 执行）
    // 成功返回 true，失败返回 false，错误信息通过 error 参数返回
    bool execute(const QString& userCode, QString& error);

    // 清理资源（卸载 DLL、删除临时文件）
    void cleanup();

    // 获取事件列表（用于转换为动画步骤）
    const std::vector<VizEvent>& getEvents() const { return m_events; }

    // 将事件转换为动画步骤并填入 AnimationController
    // localVarNames: 面板中有本地变量（非函数参数）的变量名集合；
    //   当值参数与本地变量同名时，ArrayDestroy 不 hide 面板而是恢复本地变量的数据
    void convertEventsToSteps(QMap<QString, ArrayVisualizer*>& arrVizMap,
                              AnimationController* animCtrl,
                              const QSet<QString>& localVarNames = {});

    // 获取转换后的代码（用于调试）
    QString getTransformedCode() const { return m_transformedCode; }

    // 获取跟踪到的变量列表（用于多数组UI初始化）
    const QList<TrackedVariable>& getTrackedVariables() const { return m_trackedVariables; }

signals:
    void executionFinished(bool success, const QString& error);

private:
    bool generateWrapper(const QString& userCode, const QString& wrapperPath);
    bool compile(const QString& sourcePath, QString& error);
    bool runDll(QString& error);

    QLibrary* m_library = nullptr;
    QString m_tempDir;
    QString m_dllPath;
    std::vector<VizEvent> m_events;
    QString m_transformedCode;  // 调试用：保存转换后的代码
    QList<TrackedVariable> m_trackedVariables;  // AST 分析出的变量列表
};

#endif // CODE_EXECUTOR_H
