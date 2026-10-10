#include "MainWindow.h"
#include <QApplication>
#include <QMetaType>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    qRegisterMetaType<PreviewBundle>("PreviewBundle");
    qRegisterMetaType<QVector<PreviewBundle> >("QVector<PreviewBundle>");

    MainWindow w;
    w.show();
    return app.exec();
}
