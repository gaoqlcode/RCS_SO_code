#include "MainShellWindow.h"
#include "common/SharedDataSession.h"
#include "modules/rcs/RcsPage.h"
#include "modules/hrrp/HrrpPage.h"
#include "modules/cw_rcs/CwRcsPage.h"
#include "modules/sigma0/Sigma0Page.h"
#include "modules/l0_preview/L0PreviewPage.h"
#if defined(RCS_ENABLE_MOSAIC) && RCS_ENABLE_MOSAIC
#include "modules/mosaic/MosaicPage.h"
#endif
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainShellWindow::MainShellWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("雷达数据处理软件"));
    setMinimumSize(1360, 860);
    resize(1680, 980);

    // 仅在后台共享：某一页选的数据文件夹会同步到 RCS/HRRP/点频/L0 预览，界面不再单独放顶栏
    session_ = new SharedDataSession(this);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto *tabs = new QTabWidget(central);
    tabs->addTab(new L0PreviewPage(session_, tabs), QStringLiteral("原始数据预览"));
    tabs->addTab(new RcsPage(session_, tabs), QStringLiteral("RCS测量"));
    tabs->addTab(new HrrpPage(session_, tabs), QStringLiteral("HRRP一维距离像"));
    tabs->addTab(new CwRcsPage(session_, tabs), QStringLiteral("点频RCS"));
    tabs->addTab(new Sigma0Page(tabs), QStringLiteral("后向散射系数"));
#if defined(RCS_ENABLE_MOSAIC) && RCS_ENABLE_MOSAIC
    tabs->addTab(new MosaicPage(tabs), QStringLiteral("大场景拼接"));
#endif
    root->addWidget(tabs, 1);

    setCentralWidget(central);
    setStatusBar(0);
}
