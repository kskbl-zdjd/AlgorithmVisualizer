#include "AnimationController.h"
#include <algorithm>

AnimationController::AnimationController(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_currentStep(-1)
    , m_state(AnimationState::Stopped)
    , m_speed(1.0)
    , m_baseIntervalMs(500)
{
    connect(m_timer, &QTimer::timeout, this, &AnimationController::onTimerTimeout);
}

void AnimationController::play()
{
    if (m_steps.empty()) return;

    m_state = AnimationState::Playing;
    emit stateChanged(m_state);

    int interval = std::max(20, static_cast<int>(m_baseIntervalMs / m_speed));
    m_timer->start(interval);
}

void AnimationController::pause()
{
    m_state = AnimationState::Paused;
    m_timer->stop();
    emit stateChanged(m_state);
}

void AnimationController::stop()
{
    m_timer->stop();

    // 回退所有已执行的步骤
    while (m_currentStep >= 0) {
        if (m_steps[m_currentStep].undo) {
            m_steps[m_currentStep].undo();
        }
        m_currentStep--;
    }

    m_state = AnimationState::Stopped;
    emit stateChanged(m_state);
    emit stepExecuted(0, static_cast<int>(m_steps.size()), QString());
}

void AnimationController::stepForward()
{
    if (m_currentStep + 1 >= static_cast<int>(m_steps.size())) {
        pause();
        emit animationFinished();
        return;
    }

    m_currentStep++;
    if (m_steps[m_currentStep].execute) {
        m_steps[m_currentStep].execute();
    }
    emit stepExecuted(
        m_currentStep,
        static_cast<int>(m_steps.size()),
        m_steps[m_currentStep].description
    );
}

void AnimationController::stepBackward()
{
    if (m_currentStep < 0) return;

    if (m_steps[m_currentStep].undo) {
        m_steps[m_currentStep].undo();
    }
    m_currentStep--;

    QString desc;
    if (m_currentStep >= 0) {
        desc = m_steps[m_currentStep].description;
    }
    emit stepExecuted(
        m_currentStep + 1,
        static_cast<int>(m_steps.size()),
        desc
    );
}

void AnimationController::setSpeed(double speed)
{
    m_speed = std::clamp(speed, 0.1, 5.0);
    if (m_timer->isActive()) {
        int interval = std::max(20, static_cast<int>(m_baseIntervalMs / m_speed));
        m_timer->setInterval(interval);
    }
}

double AnimationController::speed() const
{
    return m_speed;
}

void AnimationController::addStep(const AnimationStep &step)
{
    m_steps.push_back(step);
}

void AnimationController::clearSteps()
{
    stop();
    m_steps.clear();
    m_currentStep = -1;
}

int AnimationController::currentStepIndex() const
{
    return m_currentStep;
}

int AnimationController::totalSteps() const
{
    return static_cast<int>(m_steps.size());
}

AnimationState AnimationController::state() const
{
    return m_state;
}

void AnimationController::onTimerTimeout()
{
    stepForward();
}
