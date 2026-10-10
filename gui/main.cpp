#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include "app/MainShellWindow.h"
#include "common/RcsApiQt.h"
#include "common/Types.h"
#include "modules/cw_rcs/CwRcsWorker.h"
#include <vector>
#include <complex>

// WSL/精简 Linux 常无中文字体，优先选已安装的 CJK 族
static QFont pickUiFont()
{
    const char *cands[] = {
        "Noto Sans CJK SC",
        "Noto Sans CJK",
        "WenQuanYi Micro Hei",
        "WenQuanYi Zen Hei",
        "Source Han Sans SC",
        "Droid Sans Fallback",
        0
    };
    const QStringList families = QFontDatabase().families();
    for (int i = 0; cands[i]; ++i) {
        const QString name = QString::fromUtf8(cands[i]);
        if (families.contains(name, Qt::CaseInsensitive)) {
            QFont f(name);
            f.setPointSize(11);
            return f;
        }
        for (int j = 0; j < families.size(); ++j) {
            if (families.at(j).contains(QString::fromUtf8("Noto Sans CJK"), Qt::CaseInsensitive) ||
                families.at(j).contains(QString::fromUtf8("WenQuanYi"), Qt::CaseInsensitive)) {
                QFont f(families.at(j));
                f.setPointSize(11);
                return f;
            }
        }
    }
    QFont f;
    f.setPointSize(11);
    return f;
}

Q_DECLARE_METATYPE(std::vector<std::complex<double> >)
#if defined(RCS_ENABLE_MOSAIC) && RCS_ENABLE_MOSAIC
Q_DECLARE_METATYPE(MosaicResult)
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("雷达数据处理软件"));
    QApplication::setApplicationDisplayName(QStringLiteral("雷达数据处理软件"));
    QApplication::setOrganizationName(QStringLiteral("HX"));
    // app.ico 优先；app.png 须为真 PNG（曾误把 JPEG 命名为 .png，Qt 加载失败）
    QIcon appIcon(QStringLiteral(":/app.ico"));
    if (appIcon.isNull())
        appIcon = QIcon(QStringLiteral(":/app.png"));
    app.setWindowIcon(appIcon);

    registerRcsMetaTypes();
    qRegisterMetaType<std::vector<std::complex<double> >>("std::vector<std::complex<double> >");
#if defined(RCS_ENABLE_MOSAIC) && RCS_ENABLE_MOSAIC
    qRegisterMetaType<MosaicResult>("MosaicResult");
#endif
    qRegisterMetaType<CwRcsResultQt>("CwRcsResultQt");

    app.setFont(pickUiFont());

    MainShellWindow w;
    w.setWindowIcon(appIcon);
    w.showMaximized();
    return app.exec();
}
