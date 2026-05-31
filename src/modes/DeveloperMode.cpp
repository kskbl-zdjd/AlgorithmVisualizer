#include "DeveloperMode.h"
#include "../recorder/VizRecorder.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QSet>

DeveloperMode::DeveloperMode(QWidget* parent)
    : QWidget(parent)
    , m_animCtrl(new AnimationController(this))
    , m_codeExecutor(new CodeExecutor(this))
{
    setupUI();

    // 连接信号
    connect(m_animCtrl, &AnimationController::stepExecuted,
            this, &DeveloperMode::onStepExecuted);
    connect(m_animCtrl, &AnimationController::stateChanged,
            this, &DeveloperMode::onStateChanged);
}

void DeveloperMode::setupUI() {
    // 主布局：三栏
    QHBoxLayout* mainLayout = new QHBoxLayout(this);

    // ===== 左侧面板：代码输入区 =====
    QVBoxLayout* leftLayout = new QVBoxLayout();

    // 工具栏
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    m_runButton = new QPushButton("▶ 运行", this);
    m_stopButton = new QPushButton("⏹ 停止", this);
    m_endDemoButton = new QPushButton("🟥 结束演示", this);
    m_clearButton = new QPushButton("清空", this);

    m_runButton->setStyleSheet("background-color: #2196F3; color: white; padding: 4px 12px;");
    m_stopButton->setStyleSheet("padding: 4px 12px;");
    m_endDemoButton->setStyleSheet("background-color: #F44336; color: white; padding: 4px 12px;");
    m_clearButton->setStyleSheet("padding: 4px 12px;");

    toolbarLayout->addWidget(m_runButton);
    toolbarLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(m_endDemoButton);
    toolbarLayout->addWidget(m_clearButton);
    toolbarLayout->addStretch();
    leftLayout->addLayout(toolbarLayout);

    // 代码编辑器
    m_codeEditor = new QPlainTextEdit(this);
    m_codeEditor->setPlaceholderText(
        "在此输入纯净 C++ 代码，程序会自动插入可视化调用！\n\n"
        "示例（冒泡排序，无需任何 viz:: 调用）：\n\n"
        "void bubbleSort(int arr[], int n) {\n"
        "    for (int i = 0; i < n - 1; i++) {\n"
        "        for (int j = 0; j < n - i - 1; j++) {\n"
        "            if (arr[j] > arr[j + 1]) {\n"
        "                int temp = arr[j];\n"
        "                arr[j] = arr[j + 1];\n"
        "                arr[j + 1] = temp;\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n\n"
        "提示：\n"
        "  • 自动检测 int arr[]={...} / vector<int> arr 并注册到可视化\n"
        "  • swap(arr[i], arr[j]) 自动转为 viz::swap\n"
        "  • arr[i] > arr[j] 等比较自动转为 viz::compare\n"
        "  • 若需精细控制可手动调用 viz:: API（见右侧参考）"
    );
    m_codeEditor->setFont(QFont("Consolas", 10));
    leftLayout->addWidget(m_codeEditor);

    // 错误提示区
    m_errorLabel = new QLabel("就绪", this);
    m_errorLabel->setStyleSheet("color: green;");
    m_errorLabel->setWordWrap(true);
    leftLayout->addWidget(m_errorLabel);

    mainLayout->addLayout(leftLayout, 1);

    // ===== 中间面板：演示区 =====
    QVBoxLayout* centerLayout = new QVBoxLayout();

    // 用 QScrollArea 包裹可视化区域，支持多数组时滚动
    m_vizScrollArea = new QScrollArea(this);
    m_vizScrollArea->setWidgetResizable(true);
    m_vizContainer = new QWidget();
    m_vizLayout = new QVBoxLayout(m_vizContainer);
    m_vizLayout->setContentsMargins(0, 0, 0, 0);
    m_vizScrollArea->setWidget(m_vizContainer);
    centerLayout->addWidget(m_vizScrollArea, 1);

    // 状态栏
    m_statusLabel = new QLabel("就绪", this);
    centerLayout->addWidget(m_statusLabel);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    centerLayout->addWidget(m_progressBar);

    // 复杂度分析
    QGroupBox* complexityGroup = new QGroupBox("复杂度分析", this);
    QVBoxLayout* complexityLayout = new QVBoxLayout(complexityGroup);
    m_complexityLabel = new QLabel("等待执行...", this);
    complexityLayout->addWidget(m_complexityLabel);
    centerLayout->addWidget(complexityGroup);

    mainLayout->addLayout(centerLayout, 2);

    // ===== 右侧面板：控制区 =====
    QVBoxLayout* rightLayout = new QVBoxLayout();

    // 动画控制
    QGroupBox* controlGroup = new QGroupBox("动画控制", this);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* playBtn = new QPushButton("▶", this);
    QPushButton* pauseBtn = new QPushButton("⏸", this);
    QPushButton* stepBackBtn = new QPushButton("⏮", this);
    QPushButton* stepFwdBtn = new QPushButton("⏭", this);

    btnLayout->addWidget(playBtn);
    btnLayout->addWidget(pauseBtn);
    btnLayout->addWidget(stepBackBtn);
    btnLayout->addWidget(stepFwdBtn);
    controlLayout->addLayout(btnLayout);

    // 速度控制
    controlLayout->addWidget(new QLabel("速度:", this));
    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider->setRange(1, 50);
    m_speedSlider->setValue(10);
    controlLayout->addWidget(m_speedSlider);

    rightLayout->addWidget(controlGroup);

    // API 参考
    QGroupBox* apiGroup = new QGroupBox("API 参考", this);
    QVBoxLayout* apiLayout = new QVBoxLayout(apiGroup);
    QLabel* apiLabel = new QLabel(
        "【自动插桩（纯净代码）】\n"
        "  swap(arr[i], arr[j])  → 自动\n"
        "  arr[i] > arr[j]       → 自动\n"
        "  int arr[]={...}       → 自动注册\n"
        "  vector<int> arr       → 自动注册\n\n"
        "【手动 viz:: API】\n"
        "• viz::array(arr, n, name)\n"
        "• viz::setValue(idx, val)\n"
        "• viz::swap(arr, i, j)\n"
        "• viz::compare(i, j)\n"
        "• viz::mark(idx, label)\n"
        "• viz::unmark(idx)\n"
        "• viz::unmarkAll()\n"
        "• viz::recurseEnter(l,r)\n"
        "• viz::recursePending(l,r)\n"
        "• viz::recurseLeave()\n"
        "• viz::highlight(idx,color)\n"
        "• viz::reset()\n"
        "• viz::comment(text)", this);
    apiLabel->setStyleSheet("font-size: 11px;");
    apiLayout->addWidget(apiLabel);
    rightLayout->addWidget(apiGroup);

    rightLayout->addStretch();
    mainLayout->addLayout(rightLayout, 0);

    // 连接按钮信号
    connect(m_runButton, &QPushButton::clicked, this, &DeveloperMode::onRunClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &DeveloperMode::onStopClicked);
    connect(m_endDemoButton, &QPushButton::clicked, this, &DeveloperMode::onEndDemoClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &DeveloperMode::onClearClicked);

    connect(playBtn, &QPushButton::clicked, m_animCtrl, &AnimationController::play);
    connect(pauseBtn, &QPushButton::clicked, m_animCtrl, &AnimationController::pause);
    connect(stepBackBtn, &QPushButton::clicked, m_animCtrl, &AnimationController::stepBackward);
    connect(stepFwdBtn, &QPushButton::clicked, m_animCtrl, &AnimationController::stepForward);

    connect(m_speedSlider, &QSlider::valueChanged, [this](int value) {
        m_animCtrl->setSpeed(value / 10.0);
    });
}

void DeveloperMode::onRunClicked() {
    QString code = m_codeEditor->toPlainText();
    if (code.trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请先输入代码");
        return;
    }

    m_errorLabel->setText("编译中...");
    m_errorLabel->setStyleSheet("color: blue;");

    QString error;
    if (!m_codeExecutor->execute(code, error)) {
        m_errorLabel->setText(error);
        m_errorLabel->setStyleSheet("color: red;");
        return;
    }

    m_errorLabel->setText("执行成功");
    m_errorLabel->setStyleSheet("color: green;");

    // 清理旧的面板（wrapper 含 label + ArrayVisualizer）
    for (auto it = m_arrayWrappers.begin(); it != m_arrayWrappers.end(); ++it) {
        m_vizLayout->removeWidget(it.value());
        delete it.value();  // 删除 wrapper 会自动删除其子控件 (label + viz)
    }
    m_arrayWrappers.clear();
    m_arrayVisualizers.clear();

    // 根据分析出的变量动态创建 ArrayVisualizer
    // 创建策略：
    // 1. 引用参数（isReferenceParam=true）：不创建面板，事件通过 pushAlias 别名路由到调用者面板
    // 2. 本地变量（isParameter=false）：总是创建面板
    // 3. 值传递参数（isParameter=true, !isReferenceParam）：
    //    - 若同名本地变量面板已存在：不创建新面板，复用已有面板（两者在运行时共享同一 ArrayVisualizer）
    //    - 若无同名面板：创建面板（初始隐藏，等 viz::array 注册时 show）
    //    - 注意：同一函数有多个调用点时，所有同名值参数复用同一面板
    //
    // 实现方式：先处理非参数变量，再处理值参数，确保本地变量优先占据 QMap 中的槽位
    const auto& vars = m_codeExecutor->getTrackedVariables();

    // 分两轮创建：第一轮本地变量，第二轮值参数
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& var : vars) {
            // 引用参数始终跳过
            if (var.isReferenceParam) continue;

            bool isValueParam = (var.isParameter && !var.isReferenceParam);

            // 第一轮只处理本地变量；第二轮只处理值参数
            if (pass == 0 && isValueParam) continue;
            if (pass == 1 && !isValueParam) continue;

            if (m_arrayVisualizers.contains(var.name)) {
                // 面板已存在（同名本地变量已在第一轮创建，或多个同名值参数）
                // 值参数复用已有面板：无需额外操作，事件通过 arrayName 自动路由
                // 本地变量重复出现（多个函数内同名局部变量）：也复用第一个面板
                continue;
            }

            // 创建包裹容器：label + ArrayVisualizer 作为一个整体可被 hide/show
            auto* wrapper = new QWidget(m_vizContainer);
            auto* wrapperLayout = new QVBoxLayout(wrapper);
            wrapperLayout->setContentsMargins(0, 0, 0, 0);
            wrapperLayout->setSpacing(2);

            QLabel* label = new QLabel(var.name + " (" + var.type + ")", wrapper);
            label->setStyleSheet("font-weight: bold; color: #333; padding: 4px 0;");

            auto* viz = new ArrayVisualizer(wrapper);
            viz->setMinimumHeight(100);

            wrapperLayout->addWidget(label);
            wrapperLayout->addWidget(viz);

            m_vizLayout->addWidget(wrapper);
            m_arrayWrappers.insert(var.name, wrapper);
            m_arrayVisualizers.insert(var.name, viz);

            // 值参数面板初始隐藏：等到函数被调用时 viz::array 触发 ArrayCreate 事件再 show
            // 本地变量面板始终可见（声明时立即注册）
            if (isValueParam) {
                wrapper->hide();
            }
        }
    }

    // 如果没分析到变量，至少创建一个默认视觉器
    if (m_arrayVisualizers.isEmpty()) {
        auto* viz = new ArrayVisualizer(m_vizContainer);
        m_vizLayout->addWidget(viz);
        m_arrayVisualizers.insert("", viz);
    }

    // 编译执行成功，转换为动画步骤
    // 构建"本地变量名集合"：面板中有非参数本地变量的名称
    // 当值参数与同名本地变量共用面板时，ArrayDestroy 应恢复本地变量数据而非 hide 面板
    QSet<QString> localVarNames;
    for (const auto& var : m_codeExecutor->getTrackedVariables()) {
        if (!var.isParameter) {
            localVarNames.insert(var.name);
        }
    }
    m_codeExecutor->convertEventsToSteps(m_arrayVisualizers, m_animCtrl, localVarNames);

    // 更新复杂度显示
    updateComplexityDisplay();

    // 开始播放
    m_animCtrl->play();

    // 更新状态
    m_statusLabel->setText("执行成功，开始播放动画");
    m_statusLabel->setStyleSheet("color: green;");
}

void DeveloperMode::onStopClicked() {
    m_animCtrl->stop();
}

void DeveloperMode::onEndDemoClicked() {
    // 停止动画
    m_animCtrl->stop();
    m_animCtrl->clearSteps();

    // 清理执行器
    m_codeExecutor->cleanup();

    // 清理可视化面板：移除并删除所有子 widget（标签 + ArrayVisualizer）
    QLayoutItem* item;
    while ((item = m_vizLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    m_arrayVisualizers.clear();
    m_arrayWrappers.clear();

    // 重置状态
    m_statusLabel->setText("就绪");
    m_statusLabel->setStyleSheet("color: black;");
    m_progressBar->setValue(0);
    m_complexityLabel->setText("等待执行...");
    m_errorLabel->setText("就绪");
    m_errorLabel->setStyleSheet("color: green;");
}

void DeveloperMode::onClearClicked() {
    m_codeEditor->clear();
}

void DeveloperMode::onStepExecuted(int step, int total, const QString& desc) {
    m_statusLabel->setText(desc);
    m_progressBar->setValue(total > 0 ? (step * 100 / total) : 0);
}

void DeveloperMode::onStateChanged(AnimationState state) {
    switch (state) {
    case AnimationState::Playing:
        m_runButton->setEnabled(false);
        m_stopButton->setEnabled(true);
        break;
    case AnimationState::Paused:
        m_runButton->setEnabled(true);
        m_stopButton->setEnabled(true);
        break;
    case AnimationState::Stopped:
        m_runButton->setEnabled(true);
        m_stopButton->setEnabled(false);
        break;
    }
}

void DeveloperMode::updateComplexityDisplay() {
    auto stats = VizRecorder::computeStats(m_codeExecutor->getEvents());

    QString text = QString(
        "比较次数: %1\n"
        "交换次数: %2\n"
        "数组访问: %3\n"
        "最大递归深度: %4\n"
        "估算时间复杂度: %5\n"
        "估算空间复杂度: %6"
    ).arg(stats.comparisons)
     .arg(stats.swaps)
     .arg(stats.arrayAccesses)
     .arg(stats.maxRecursionDepth)
     .arg(stats.estimateTimeRaw())
     .arg(stats.estimateSpaceRaw());

    m_complexityLabel->setText(text);
}
