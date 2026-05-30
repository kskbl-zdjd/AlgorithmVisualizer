#ifndef ANIMATIONCONTROLLER_H
#define ANIMATIONCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <vector>
#include <functional>
#include <QString>

enum class AnimationState {
    Stopped,
    Playing,
    Paused
};

// 一个动画步骤 = 执行动作 + 撤销动作 + 文字描述
struct AnimationStep {
    std::function<void()> execute;   // 执行此步骤
    std::function<void()> undo;      // 撤销此步骤（用于后退）
    QString description;             // 步骤的文字说明（显示在状态栏）
};

class AnimationController : public QObject
{
    Q_OBJECT

public:
    explicit AnimationController(QObject *parent = nullptr);

    // 动画控制
    void play();
    void pause();
    void stop();
    void stepForward();
    void stepBackward();

    // 速度控制 (0.1x ~ 5.0x)
    void setSpeed(double speed);
    double speed() const;

    // 步骤管理
    void addStep(const AnimationStep &step);
    void clearSteps();
    int currentStepIndex() const;  // 0-based，-1 表示未开始
    int totalSteps() const;

    AnimationState state() const;

signals:
    void stateChanged(AnimationState newState);
    void stepExecuted(int stepIndex, int totalSteps, const QString &description);
    void animationFinished();

private slots:
    void onTimerTimeout();

private:
    QTimer *m_timer;
    std::vector<AnimationStep> m_steps;
    int m_currentStep;          // -1 = 未执行任何步骤
    AnimationState m_state;
    double m_speed;
    int m_baseIntervalMs;       // 基础间隔 500ms
};

#endif // ANIMATIONCONTROLLER_H
