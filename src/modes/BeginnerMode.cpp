#include "BeginnerMode.h"
#include "../visualizers/ArrayVisualizer.h"
#include "../visualizers/HeapVisualizer.h"
#include "../algorithms/SortingAlgorithms.h"
#include "../algorithms/HeapSort.h"
#include "../algorithms/MergeSort.h"
#include "../resources/code_examples.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QProgressBar>
#include <QDialog>
#include <QDialogButtonBox>
#include <QRandomGenerator>
#include <QFont>

BeginnerMode::BeginnerMode(QWidget *parent)
    : QWidget(parent)
    , m_animCtrl(new AnimationController(this))
    , m_heapVisualizer(nullptr)
{
    setupUI();

    connect(m_animCtrl, &AnimationController::stepExecuted,
            this, &BeginnerMode::onStepExecuted);
    connect(m_animCtrl, &AnimationController::stateChanged,
            this, &BeginnerMode::onStateChanged);

    generateRandomData();
    updateCodeDisplay("冒泡排序");
}

void BeginnerMode::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 左侧面板
    QWidget *leftWidget = new QWidget(this);
    setupLeftPanel(leftWidget);
    leftWidget->setFixedWidth(320);

    // 中间面板
    QWidget *centerWidget = new QWidget(this);
    setupCenterPanel(centerWidget);

    // 右侧面板
    QWidget *rightWidget = new QWidget(this);
    setupRightPanel(rightWidget);
    rightWidget->setFixedWidth(200);

    mainLayout->addWidget(leftWidget);
    mainLayout->addWidget(centerWidget, 1);
    mainLayout->addWidget(rightWidget);
}

void BeginnerMode::setupLeftPanel(QWidget *parent)
{
    QVBoxLayout *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);

    // 算法选择
    QGroupBox *algoGroup = new QGroupBox("算法选择", parent);
    QVBoxLayout *algoLayout = new QVBoxLayout(algoGroup);

    m_algorithmCombo = new QComboBox();
    m_algorithmCombo->addItems({
        "冒泡排序", "选择排序", "插入排序", "快速排序", "堆排序", "归并排序"
    });
    algoLayout->addWidget(m_algorithmCombo);

    layout->addWidget(algoGroup);

    // 代码示例
    QGroupBox *codeGroup = new QGroupBox("算法代码示例", parent);
    QVBoxLayout *codeLayout = new QVBoxLayout(codeGroup);

    m_codeDisplay = new QTextEdit();
    m_codeDisplay->setReadOnly(true);
    m_codeDisplay->setFont(QFont("Consolas", 10));
    m_codeDisplay->setMinimumHeight(200);
    codeLayout->addWidget(m_codeDisplay);

    m_zoomCodeBtn = new QPushButton("🔍 放大查看代码");
    codeLayout->addWidget(m_zoomCodeBtn);

    layout->addWidget(codeGroup, 1);

    // 信号连接
    connect(m_algorithmCombo, &QComboBox::currentIndexChanged,
            this, &BeginnerMode::onAlgorithmChanged);
    connect(m_zoomCodeBtn, &QPushButton::clicked,
            this, &BeginnerMode::onZoomCodeClicked);
}

void BeginnerMode::setupCenterPanel(QWidget *parent)
{
    QVBoxLayout *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *vizGroup = new QGroupBox("可视化区域", parent);
    QVBoxLayout *vizLayout = new QVBoxLayout(vizGroup);

    // 堆可视化组件（仅堆排序时显示）
    m_heapVisualizer = new HeapVisualizer();
    m_heapVisualizer->setVisible(false);
    m_heapVisualizer->setFixedHeight(250);
    vizLayout->addWidget(m_heapVisualizer);

    // 数组可视化组件（所有排序都显示）
    m_arrayVisualizer = new ArrayVisualizer();
    m_arrayVisualizer->setMinimumHeight(120);  // 确保有足够高度
    vizLayout->addWidget(m_arrayVisualizer, 1);

    // 状态栏
    m_statusLabel = new QLabel("就绪 - 选择算法后点击「开始」");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("padding: 6px; background: #e8f5e9; border-radius: 4px;");
    vizLayout->addWidget(m_statusLabel);

    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    vizLayout->addWidget(m_progressBar);

    layout->addWidget(vizGroup);

    // 堆索引显示/隐藏按钮（仅在堆排序时显示）
    m_toggleIndicesBtn = new QPushButton("显示/隐藏节点索引");
    m_toggleIndicesBtn->setVisible(false);
    connect(m_toggleIndicesBtn, &QPushButton::clicked, this, &BeginnerMode::onToggleHeapIndices);
    layout->addWidget(m_toggleIndicesBtn);
}

void BeginnerMode::setupRightPanel(QWidget *parent)
{
    QVBoxLayout *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);

    // 数据输入
    QGroupBox *dataGroup = new QGroupBox("数据设置", parent);
    QVBoxLayout *dataLayout = new QVBoxLayout(dataGroup);

    dataLayout->addWidget(new QLabel("输入数据（逗号分隔）："));
    m_dataInput = new QLineEdit();
    m_dataInput->setPlaceholderText("例如: 38,27,43,3,9,82,10");
    dataLayout->addWidget(m_dataInput);

    m_randomDataBtn = new QPushButton("🎲 生成随机数据");
    dataLayout->addWidget(m_randomDataBtn);

    layout->addWidget(dataGroup);

    // 动画控制
    QGroupBox *ctrlGroup = new QGroupBox("动画控制", parent);
    QVBoxLayout *ctrlLayout = new QVBoxLayout(ctrlGroup);

    m_startBtn = new QPushButton("▶ 开始");
    m_pauseBtn = new QPushButton("⏸ 暂停");
    m_stopBtn = new QPushButton("⏹ 停止");
    m_stepBackwardBtn = new QPushButton("⏪ 单步后退");
    m_stepForwardBtn = new QPushButton("⏩ 单步前进");

    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_stepForwardBtn->setEnabled(false);
    m_stepBackwardBtn->setEnabled(false);

    ctrlLayout->addWidget(m_startBtn);
    ctrlLayout->addWidget(m_pauseBtn);
    ctrlLayout->addWidget(m_stopBtn);
    ctrlLayout->addWidget(m_stepBackwardBtn);
    ctrlLayout->addWidget(m_stepForwardBtn);

    layout->addWidget(ctrlGroup);

    // 速度控制
    QGroupBox *speedGroup = new QGroupBox("速度控制", parent);
    QVBoxLayout *speedLayout = new QVBoxLayout(speedGroup);

    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(1, 50);
    m_speedSlider->setValue(10);
    speedLayout->addWidget(m_speedSlider);

    m_speedLabel = new QLabel("1.0x");
    m_speedLabel->setAlignment(Qt::AlignCenter);
    speedLayout->addWidget(m_speedLabel);

    layout->addWidget(speedGroup);

    layout->addStretch();

    // 信号连接
    connect(m_startBtn, &QPushButton::clicked, this, &BeginnerMode::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &BeginnerMode::onPauseClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &BeginnerMode::onStopClicked);
    connect(m_stepForwardBtn, &QPushButton::clicked, this, &BeginnerMode::onStepForward);
    connect(m_stepBackwardBtn, &QPushButton::clicked, this, &BeginnerMode::onStepBackward);
    connect(m_speedSlider, &QSlider::valueChanged, this, &BeginnerMode::onSpeedChanged);
    connect(m_randomDataBtn, &QPushButton::clicked, this, &BeginnerMode::generateRandomData);
}

// ========== 槽函数 ==========

void BeginnerMode::onAlgorithmChanged(int /*index*/)
{
    QString algo = m_algorithmCombo->currentText();
    updateCodeDisplay(algo);

    // 显示/隐藏堆可视化组件
    bool isHeapSort = (algo == "堆排序");
    m_heapVisualizer->setVisible(isHeapSort);
    m_toggleIndicesBtn->setVisible(isHeapSort);

    // 如果正在播放，先停止
    if (m_animCtrl->state() != AnimationState::Stopped) {
        m_animCtrl->stop();
    }
}

void BeginnerMode::onStartClicked()
{
    if (m_animCtrl->state() == AnimationState::Stopped) {
        // 解析输入数据
        QString input = m_dataInput->text().trimmed();
        std::vector<int> data;
        if (!input.isEmpty()) {
            QStringList parts = input.split(',');
            for (const QString &part : parts) {
                bool ok;
                int val = part.trimmed().toInt(&ok);
                if (ok) data.push_back(val);
            }
        }
        if (data.empty()) {
            data = m_arrayVisualizer->getData();
        }
        if (data.empty()) return;

        // 根据选择生成动画步骤
        QString algo = m_algorithmCombo->currentText();
        if (algo == "冒泡排序") {
            SortingAlgorithms::bubbleSort(m_arrayVisualizer, m_animCtrl, data);
        } else if (algo == "选择排序") {
            SortingAlgorithms::selectionSort(m_arrayVisualizer, m_animCtrl, data);
        } else if (algo == "插入排序") {
            SortingAlgorithms::insertionSort(m_arrayVisualizer, m_animCtrl, data);
        } else if (algo == "快速排序") {
            SortingAlgorithms::quickSort(m_arrayVisualizer, m_animCtrl, data);
        } else if (algo == "堆排序") {
            HeapSort::heapSort(m_heapVisualizer, m_arrayVisualizer, m_animCtrl, data);
        } else if (algo == "归并排序") {
            MergeSort::mergeSort(m_arrayVisualizer, m_animCtrl, data);
        }
    }

    m_animCtrl->play();
}

void BeginnerMode::onPauseClicked()
{
    if (m_animCtrl->state() == AnimationState::Playing) {
        m_animCtrl->pause();
    } else if (m_animCtrl->state() == AnimationState::Paused) {
        m_animCtrl->play();
    }
}

void BeginnerMode::onStopClicked()
{
    m_animCtrl->stop();
    m_statusLabel->setText("已停止");
    m_progressBar->setValue(0);
}

void BeginnerMode::onStepForward()
{
    if (m_animCtrl->state() == AnimationState::Stopped) {
        // 首次单步：需要先生成步骤
        onStartClicked();
        m_animCtrl->pause();
    } else if (m_animCtrl->state() == AnimationState::Playing) {
        m_animCtrl->pause();
    }
    m_animCtrl->stepForward();
}

void BeginnerMode::onStepBackward()
{
    if (m_animCtrl->state() == AnimationState::Playing) {
        m_animCtrl->pause();
    }
    m_animCtrl->stepBackward();
}

void BeginnerMode::onSpeedChanged(int value)
{
    double speed = value / 10.0;
    m_speedLabel->setText(QString("%1x").arg(speed, 0, 'f', 1));
    m_animCtrl->setSpeed(speed);
}

void BeginnerMode::onStepExecuted(int stepIndex, int totalSteps, const QString &description)
{
    m_statusLabel->setText(QString("步骤 %1/%2: %3")
        .arg(stepIndex + 1).arg(totalSteps).arg(description));

    int progress = (totalSteps > 0) ? (stepIndex + 1) * 100 / totalSteps : 0;
    m_progressBar->setValue(progress);
}

void BeginnerMode::onStateChanged(AnimationState newState)
{
    updateButtonStates(newState);

    switch (newState) {
    case AnimationState::Playing:
        m_pauseBtn->setText("⏸ 暂停");
        break;
    case AnimationState::Paused:
        m_pauseBtn->setText("▶ 继续");
        break;
    case AnimationState::Stopped:
        m_pauseBtn->setText("⏸ 暂停");
        break;
    }
}

void BeginnerMode::onZoomCodeClicked()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("代码示例 - " + m_algorithmCombo->currentText());
    dialog->resize(700, 600);

    QVBoxLayout *layout = new QVBoxLayout(dialog);

    QTextEdit *codeView = new QTextEdit();
    codeView->setReadOnly(true);
    codeView->setFont(QFont("Consolas", 12));
    codeView->setPlainText(m_codeDisplay->toPlainText());
    layout->addWidget(codeView);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(btnBox, &QDialogButtonBox::rejected, dialog, &QDialog::accept);
    layout->addWidget(btnBox);

    dialog->exec();
    dialog->deleteLater();
}

void BeginnerMode::onToggleHeapIndices()
{
    if (m_heapVisualizer) {
        m_heapVisualizer->setShowIndices(!m_heapVisualizer->showIndices());
    }
}

void BeginnerMode::updateCodeDisplay(const QString &algorithmName)
{
    if (algorithmName == "冒泡排序") {
        m_codeDisplay->setPlainText(CodeExamples::BUBBLE_SORT);
    } else if (algorithmName == "选择排序") {
        m_codeDisplay->setPlainText(CodeExamples::SELECTION_SORT);
    } else if (algorithmName == "插入排序") {
        m_codeDisplay->setPlainText(CodeExamples::INSERTION_SORT);
    } else if (algorithmName == "快速排序") {
        m_codeDisplay->setPlainText(CodeExamples::QUICK_SORT);
    } else if (algorithmName == "堆排序") {
        m_codeDisplay->setPlainText(CodeExamples::HEAP_SORT_CODE);
    } else if (algorithmName == "归并排序") {
        m_codeDisplay->setPlainText(CodeExamples::MERGE_SORT_CODE);
    }
}

void BeginnerMode::generateRandomData()
{
    std::vector<int> data;
    int count = 8 + QRandomGenerator::global()->bounded(5); // 8~12个元素
    for (int i = 0; i < count; ++i) {
        data.push_back(QRandomGenerator::global()->bounded(1, 100));
    }
    m_arrayVisualizer->setData(data);

    QStringList strList;
    for (int val : data) {
        strList << QString::number(val);
    }
    m_dataInput->setText(strList.join(", "));

    // 停止当前动画
    if (m_animCtrl->state() != AnimationState::Stopped) {
        m_animCtrl->stop();
    }
    m_statusLabel->setText("已生成随机数据 - 点击「开始」运行排序");
    m_progressBar->setValue(0);
}

void BeginnerMode::updateButtonStates(AnimationState state)
{
    switch (state) {
    case AnimationState::Stopped:
        m_startBtn->setEnabled(true);
        m_pauseBtn->setEnabled(false);
        m_stopBtn->setEnabled(false);
        m_stepForwardBtn->setEnabled(true);
        m_stepBackwardBtn->setEnabled(false);
        m_algorithmCombo->setEnabled(true);
        m_dataInput->setEnabled(true);
        m_randomDataBtn->setEnabled(true);
        break;
    case AnimationState::Playing:
        m_startBtn->setEnabled(false);
        m_pauseBtn->setEnabled(true);
        m_stopBtn->setEnabled(true);
        m_stepForwardBtn->setEnabled(true);
        m_stepBackwardBtn->setEnabled(true);
        m_algorithmCombo->setEnabled(false);
        m_dataInput->setEnabled(false);
        m_randomDataBtn->setEnabled(false);
        break;
    case AnimationState::Paused:
        m_startBtn->setEnabled(false);
        m_pauseBtn->setEnabled(true);
        m_stopBtn->setEnabled(true);
        m_stepForwardBtn->setEnabled(true);
        m_stepBackwardBtn->setEnabled(true);
        m_algorithmCombo->setEnabled(false);
        m_dataInput->setEnabled(false);
        m_randomDataBtn->setEnabled(false);
        break;
    }
}
