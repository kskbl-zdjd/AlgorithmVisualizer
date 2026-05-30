#include "MainWindow.h"
#include "../modes/BeginnerMode.h"
#include "../modes/DeveloperMode.h"
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_modeStack(new QStackedWidget(this))
    , m_beginnerMode(new BeginnerMode(this))
    , m_developerMode(new DeveloperMode(this))
{
    setupUI();
    setupToolBar();

    setWindowTitle("算法及数据结构可视化");
    resize(1280, 800);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    m_modeStack->addWidget(m_beginnerMode);
    m_modeStack->addWidget(m_developerMode);
    setCentralWidget(m_modeStack);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("模式切换");
    toolBar->setMovable(false);

    m_beginnerAction = toolBar->addAction("📚 初学者模式", this, &MainWindow::switchToBeginnerMode);
    m_developerAction = toolBar->addAction("💻 开发者模式", this, &MainWindow::switchToDeveloperMode);

    m_beginnerAction->setCheckable(true);
    m_developerAction->setCheckable(true);
    m_beginnerAction->setChecked(true);

    // 互斥切换
    connect(m_beginnerAction, &QAction::triggered, this, [this]() {
        m_developerAction->setChecked(false);
    });
    connect(m_developerAction, &QAction::triggered, this, [this]() {
        m_beginnerAction->setChecked(false);
    });
}

void MainWindow::switchToBeginnerMode()
{
    m_modeStack->setCurrentIndex(0);
}

void MainWindow::switchToDeveloperMode()
{
    m_modeStack->setCurrentIndex(1);
}
