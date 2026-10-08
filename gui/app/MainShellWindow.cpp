#include "MainShellWindow.h"
#include "common/SharedDataSession.h"
#include "modules/rcs/RcsPage.h"
#include "modules/hrrp/HrrpPage.h"
#include "modules/cw_rcs/CwRcsPage.h"
#if defined(RCS_ENABLE_MOSAIC) && RCS_ENABLE_MOSAIC
#include "modules/mosaic/MosaicPage.h"
#endif
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QGroupBox>

MainShellWindow::MainShellWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("雷达数据处理软件"));
    setMinimumSize(1360, 860);
    resize(1680, 980);

    session_ = new SharedDataSession(this);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // 顶部：一次选文件夹，三个业务共用
    auto *pathBox = new QGroupBox(QStringLiteral("数据文件夹（RCS / HRRP / 点频RCS 共用）"), central);
    auto *pathLay = new QHBoxLayout(pathBox);
    folderEdit_ = new QLineEdit(pathBox);
    folderEdit_->setPlaceholderText(QStringLiteral("选择含 *_L0_*.dat 的目录"));
    auto *browse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    browse->setFixedWidth(72);
    pathLay->addWidget(folderEdit_, 1);
    pathLay->addWidget(browse);
    root->addWidget(pathBox);

    folderHint_ = new QLabel(QStringLiteral("请先选择数据文件夹，再进入各页开始处理。"), central);
    folderHint_->setStyleSheet(QStringLiteral("color:#666;"));
    root->addWidget(folderHint_);

    auto *tabs = new QTabWidget(central);
    tabs->addTab(new RcsPage(session_, tabs), QStringLiteral("RCS测量"));
    tabs->addTab(new HrrpPage(session_, tabs), QStringLiteral("HRRP一维距离像"));
    tabs->addTab(new CwRcsPage(session_, tabs), QStringLiteral("点频RCS"));
#if defined(RCS_ENABLE_MOSAIC) && RCS_ENABLE_MOSAIC
    tabs->addTab(new MosaicPage(tabs), QStringLiteral("大场景拼接"));
    folderHint_->setText(
        QStringLiteral("上方文件夹供 RCS/HRRP/点频 共用；大场景拼接请在本页选择 TIFF/RAW 图目录。"));
#endif
    root->addWidget(tabs, 1);

    setCentralWidget(central);
    setStatusBar(0);

    connect(browse, &QPushButton::clicked, this, &MainShellWindow::onBrowseDataFolder);
    connect(folderEdit_, &QLineEdit::editingFinished, this, &MainShellWindow::onFolderEdited);
    connect(session_, &SharedDataSession::dataFolderChanged, this, [this](const QString &p) {
        if (folderEdit_->text() != p)
            folderEdit_->setText(p);
    });
}

void MainShellWindow::onBrowseDataFolder()
{
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择数据文件夹"),
                                                        folderEdit_->text());
    if (d.isEmpty())
        return;
    folderEdit_->setText(d);
    session_->setDataFolder(d);
}

void MainShellWindow::onFolderEdited()
{
    session_->setDataFolder(folderEdit_->text());
}
