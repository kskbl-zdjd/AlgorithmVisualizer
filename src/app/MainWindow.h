#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QToolBar>
#include <QAction>

class BeginnerMode;
class DeveloperMode;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void switchToBeginnerMode();
    void switchToDeveloperMode();

private:
    void setupUI();
    void setupToolBar();

    QStackedWidget *m_modeStack;
    BeginnerMode *m_beginnerMode;
    DeveloperMode *m_developerMode;

    QAction *m_beginnerAction;
    QAction *m_developerAction;
};

#endif // MAINWINDOW_H
