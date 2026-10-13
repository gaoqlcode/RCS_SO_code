#include "MainWindow.h"

#include <QApplication>
#include <QMetaType>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qRegisterMetaType<QImage>("QImage");
    MainWindow w;
    w.show();
    return app.exec();
}
