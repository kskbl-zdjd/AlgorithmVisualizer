#include <QApplication>
#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("算法及数据结构可视化");

    MainWindow window;
    window.show();

    return app.exec();
}
