#ifndef BEGINNERMODE_H
#define BEGINNERMODE_H

#include <QWidget>
#include "../core/AnimationController.h"

class ArrayVisualizer;
class HeapVisualizer;
class QComboBox;
class QTextEdit;
class QLabel;
class QProgressBar;
class QLineEdit;
class QPushButton;
class QSlider;
class QDialog;

class BeginnerMode : public QWidget
{
    Q_OBJECT

public:
    explicit BeginnerMode(QWidget *parent = nullptr);

private slots:
    void onAlgorithmChanged(int index);
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onStepForward();
    void onStepBackward();
    void onSpeedChanged(int value);
    void onStepExecuted(int stepIndex, int totalSteps, const QString &description);
    void onStateChanged(AnimationState newState);
    void onZoomCodeClicked();
    void onToggleHeapIndices();

private:
    void setupUI();
    void setupLeftPanel(QWidget *parent);
    void setupCenterPanel(QWidget *parent);
    void setupRightPanel(QWidget *parent);
    void updateCodeDisplay(const QString &algorithmName);
    void generateRandomData();
    void updateButtonStates(AnimationState state);

    // 左侧
    QComboBox *m_algorithmCombo;
    QTextEdit *m_codeDisplay;
    QPushButton *m_zoomCodeBtn;

    // 中间
    ArrayVisualizer *m_arrayVisualizer;
    HeapVisualizer *m_heapVisualizer;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_toggleIndicesBtn;

    // 右侧
    QLineEdit *m_dataInput;
    QPushButton *m_randomDataBtn;
    QPushButton *m_startBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_stepForwardBtn;
    QPushButton *m_stepBackwardBtn;
    QSlider *m_speedSlider;
    QLabel *m_speedLabel;

    // 动画引擎
    AnimationController *m_animCtrl;
};

#endif // BEGINNERMODE_H
