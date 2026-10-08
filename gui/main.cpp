#include <QApplication>
#include <QFont>
#include <QIcon>
#include "app/MainShellWindow.h"
#include "common/RcsApiQt.h"
#include "common/Types.h"
#include "modules/cw_rcs/CwRcsWorker.h"
#include <vector>
#include <complex>

Q_DECLARE_METATYPE(std::vector<std::complex<double> >)
Q_DECLARE_METATYPE(MosaicResult)

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("雷达数据处理软件"));
    QApplication::setApplicationDisplayName(QStringLiteral("雷达数据处理软件"));
    QApplication::setOrganizationName(QStringLiteral("HX"));
    app.setWindowIcon(QIcon(QStringLiteral(":/app.png")));

    registerRcsMetaTypes();
    qRegisterMetaType<std::vector<std::complex<double> >>("std::vector<std::complex<double> >");
    qRegisterMetaType<MosaicResult>("MosaicResult");
    qRegisterMetaType<CwRcsResultQt>("CwRcsResultQt");

    QFont font = app.font();
    font.setPointSize(10);
    app.setFont(font);

    MainShellWindow w;
    w.setWindowIcon(QIcon(QStringLiteral(":/app.png")));
    w.show();
    return app.exec();
}
