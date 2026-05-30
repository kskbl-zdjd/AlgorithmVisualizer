#ifndef DEVELOPERMODE_H
#define DEVELOPERMODE_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QProgressBar>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMap>
#include "../core/AnimationController.h"
#include "../visualizers/ArrayVisualizer.h"
#include "../core/CodeExecutor.h"

class DeveloperMode : public QWidget {
    Q_OBJECT

public:
    explicit DeveloperMode(QWidget* parent = nullptr);

private slots:
    void onRunClicked();
    void onStopClicked();
    void onEndDemoClicked();
    void onClearClicked();
    void onStepExecuted(int step, int total, const QString& desc);
    void onStateChanged(AnimationState state);

private:
    void setupUI();
    void updateComplexityDisplay();

    // 核心组件
    AnimationController* m_animCtrl;
    QMap<QString, ArrayVisualizer*> m_arrayVisualizers;  // 变量名→可视化器映射
    QMap<QString, QWidget*> m_arrayWrappers;              // 变量名→(label+viz)容器映射
    CodeExecutor* m_codeExecutor;

    // UI 组件
    QScrollArea* m_vizScrollArea;      // 可视化区域（滚动）
    QWidget* m_vizContainer;            // 可视化容器
    QVBoxLayout* m_vizLayout;           // 可视化布局（动态添加 ArrayVisualizer）
    QPlainTextEdit* m_codeEditor;
    QPushButton* m_runButton;
    QPushButton* m_stopButton;
    QPushButton* m_endDemoButton;
    QPushButton* m_clearButton;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QLabel* m_complexityLabel;
    QSlider* m_speedSlider;
    QLabel* m_errorLabel;
};

#endif // DEVELOPERMODE_H
