#include "MainWindow.h"
#include "CalMosaicWorker.h"
#include "RectPickDialog.h"

#include "rcs/rcs.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , calibEdit_(0)
    , rawDirEdit_(0)
    , calOutEdit_(0)
    , bandCombo_(0)
    , polCombo_(0)
    , sigmaSpin_(0)
    , calStartBtn_(0)
    , mosaicInEdit_(0)
    , mosaicOutEdit_(0)
    , medSpin_(0)
    , mosaicStartBtn_(0)
    , tzSrcEdit_(0)
    , tzOutEdit_(0)
    , tzRecCheck_(0)
    , tzStartBtn_(0)
    , cancelBtn_(0)
    , progress_(0)
    , log_(0)
    , workerThread_(0)
    , worker_(0)
    , busy_(false)
{
    setWindowTitle(QStringLiteral("SDK_207 示例 — 定标 / 拼接 / ToZero  (rcs %1)")
                       .arg(QString::fromUtf8(rcsVersion())));
    resize(900, 640);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);

    auto *tabs = new QTabWidget(this);

    // ---- 定标 ----
    auto *calPage = new QWidget;
    auto *calLay = new QVBoxLayout(calPage);
    auto *calForm = new QFormLayout;
    calibEdit_ = new QLineEdit;
    rawDirEdit_ = new QLineEdit;
    calOutEdit_ = new QLineEdit;
    bandCombo_ = new QComboBox;
    bandCombo_->addItems(QStringList() << QStringLiteral("X") << QStringLiteral("Ku")
                                       << QStringLiteral("Ka") << QStringLiteral("W"));
    polCombo_ = new QComboBox;
    polCombo_->addItems(QStringList() << QStringLiteral("HH") << QStringLiteral("HV")
                                      << QStringLiteral("VH") << QStringLiteral("VV")
                                      << QStringLiteral("(不筛)"));
    sigmaSpin_ = new QDoubleSpinBox;
    sigmaSpin_->setRange(-50.0, 80.0);
    sigmaSpin_->setDecimals(2);
    sigmaSpin_->setValue(18.25);
    sigmaSpin_->setSuffix(QStringLiteral(" dBsm"));

    auto *calibRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(calibRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(calibEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseCalib);
    }
    auto *rawRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(rawRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(rawDirEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseRawDir);
    }
    auto *outRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(outRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(calOutEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseCalOut);
    }

    calForm->addRow(QStringLiteral("定标 RAW（含角反）"), calibRow);
    calForm->addRow(QStringLiteral("批量 RAW 目录"), rawRow);
    calForm->addRow(QStringLiteral("输出目录（可空）"), outRow);
    calForm->addRow(QStringLiteral("波段 band"), bandCombo_);
    calForm->addRow(QStringLiteral("极化 pol"), polCombo_);
    calForm->addRow(QStringLiteral("角反理论 RCS"), sigmaSpin_);
    calLay->addLayout(calForm);
    calStartBtn_ = new QPushButton(QStringLiteral("开始定标（将弹出框选角反缓冲区）"));
    calLay->addWidget(calStartBtn_);
    calLay->addStretch(1);
    connect(calStartBtn_, &QPushButton::clicked, this, &MainWindow::onStartCal);
    tabs->addTab(calPage, QStringLiteral("辐射定标"));

    // ---- 拼接 ----
    auto *mosPage = new QWidget;
    auto *mosLay = new QVBoxLayout(mosPage);
    auto *mosForm = new QFormLayout;
    mosaicInEdit_ = new QLineEdit;
    mosaicOutEdit_ = new QLineEdit;
    medSpin_ = new QDoubleSpinBox;
    medSpin_->setRange(0.05, 1.0);
    medSpin_->setDecimals(2);
    medSpin_->setSingleStep(0.05);
    medSpin_->setValue(0.35);

    auto *minRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(minRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(mosaicInEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseMosaicIn);
    }
    auto *moutRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(moutRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(mosaicOutEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseMosaicOut);
    }
    mosForm->addRow(QStringLiteral("输入目录（≥2 张 TIFF）"), minRow);
    mosForm->addRow(QStringLiteral("输出目录"), moutRow);
    mosForm->addRow(QStringLiteral("匀光 medOut"), medSpin_);
    mosLay->addLayout(mosForm);
    mosaicStartBtn_ = new QPushButton(QStringLiteral("开始自动拼接"));
    mosLay->addWidget(mosaicStartBtn_);
    mosLay->addWidget(new QLabel(
        QStringLiteral("推荐输入定标产物 *_RCS_dBsm.tif；库内固定 DN↔dBsm 缩放 100。")));
    mosLay->addStretch(1);
    connect(mosaicStartBtn_, &QPushButton::clicked, this, &MainWindow::onStartMosaic);
    tabs->addTab(mosPage, QStringLiteral("大图拼接"));

    // ---- ToZeroData ----
    auto *tzPage = new QWidget;
    auto *tzLay = new QVBoxLayout(tzPage);
    auto *tzForm = new QFormLayout;
    tzSrcEdit_ = new QLineEdit;
    tzOutEdit_ = new QLineEdit;
    tzRecCheck_ = new QCheckBox(QStringLiteral("同时写 .sarrec 还原信息"));
    auto *tzSrcRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(tzSrcRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(tzSrcEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseToZeroSrc);
    }
    auto *tzOutRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(tzOutRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(tzOutEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseToZeroOut);
    }
    tzForm->addRow(QStringLiteral("源 DAT"), tzSrcRow);
    tzForm->addRow(QStringLiteral("输出基路径"), tzOutRow);
    tzForm->addRow(QString(), tzRecCheck_);
    tzLay->addLayout(tzForm);
    tzStartBtn_ = new QPushButton(QStringLiteral("开始 ToZeroData 转换"));
    tzLay->addWidget(tzStartBtn_);
    tzLay->addWidget(new QLabel(QStringLiteral(
        "输出基路径示例：/data/out/source_L0.dat → 派生 HH/VV/HV/VH 四个 DAT。\n"
        "API：processToZeroData（已静态合入 librcs_proc，客户只链 -lrcs_proc）。")));
    tzLay->addStretch(1);
    connect(tzStartBtn_, &QPushButton::clicked, this, &MainWindow::onStartToZero);
    tabs->addTab(tzPage, QStringLiteral("ToZeroData"));

    root->addWidget(tabs);

    cancelBtn_ = new QPushButton(QStringLiteral("取消"));
    cancelBtn_->setEnabled(false);
    connect(cancelBtn_, &QPushButton::clicked, this, &MainWindow::onCancel);
    root->addWidget(cancelBtn_);

    progress_ = new QProgressBar;
    progress_->setRange(0, 100);
    progress_->setValue(0);
    root->addWidget(progress_);

    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    root->addWidget(log_, 1);

    workerThread_ = new QThread(this);
    worker_ = new CalMosaicWorker;
    worker_->moveToThread(workerThread_);
    connect(worker_, &CalMosaicWorker::progress, this, &MainWindow::setProgress,
            Qt::QueuedConnection);
    connect(worker_, &CalMosaicWorker::message, this, &MainWindow::appendLog, Qt::QueuedConnection);
    connect(worker_, &CalMosaicWorker::finished, this, &MainWindow::onFinished, Qt::QueuedConnection);
    // QueuedConnection：对话框必须在主线程；Worker 侧 waitRectRoi 阻塞等待 provideRectRoi
    connect(worker_, &CalMosaicWorker::needRectRoi, this, &MainWindow::onNeedRectRoi,
            Qt::QueuedConnection);
    workerThread_->start();
}

MainWindow::~MainWindow()
{
    if (worker_)
        worker_->requestCancel();
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(3000);
    }
    delete worker_;
    worker_ = 0;
}

void MainWindow::setBusy(bool busy)
{
    busy_ = busy;
    calStartBtn_->setEnabled(!busy);
    mosaicStartBtn_->setEnabled(!busy);
    tzStartBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void MainWindow::appendLog(const QString &s)
{
    log_->appendPlainText(s);
}

void MainWindow::setProgress(int p, const QString &s)
{
    progress_->setValue(p);
    if (!s.isEmpty())
        statusBar()->showMessage(s);
}

void MainWindow::onBrowseCalib()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择定标 RAW"),
                                                   calibEdit_->text(),
                                                   QStringLiteral("RAW (*.raw);;所有文件 (*)"));
    if (!f.isEmpty())
        calibEdit_->setText(f);
}

void MainWindow::onBrowseRawDir()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选择批量 RAW 目录"), rawDirEdit_->text());
    if (!d.isEmpty())
        rawDirEdit_->setText(d);
}

void MainWindow::onBrowseCalOut()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选择定标输出目录"), calOutEdit_->text());
    if (!d.isEmpty())
        calOutEdit_->setText(d);
}

void MainWindow::onBrowseMosaicIn()
{
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择拼接输入目录"),
                                                        mosaicInEdit_->text());
    if (!d.isEmpty())
        mosaicInEdit_->setText(d);
}

void MainWindow::onBrowseMosaicOut()
{
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择拼接输出目录"),
                                                        mosaicOutEdit_->text());
    if (!d.isEmpty())
        mosaicOutEdit_->setText(d);
}

void MainWindow::onStartCal()
{
    if (calibEdit_->text().trimmed().isEmpty() || rawDirEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("定标"),
                             QStringLiteral("请先选择定标 RAW 与批量 RAW 目录"));
        return;
    }
    setBusy(true);
    progress_->setValue(0);
    appendLog(QStringLiteral("---- 定标开始 ----"));
    const QString pol =
        (polCombo_->currentIndex() == polCombo_->count() - 1) ? QString() : polCombo_->currentText();
    QMetaObject::invokeMethod(
        worker_, "startCal", Qt::QueuedConnection, Q_ARG(QString, calibEdit_->text().trimmed()),
        Q_ARG(QString, rawDirEdit_->text().trimmed()), Q_ARG(QString, calOutEdit_->text().trimmed()),
        Q_ARG(QString, bandCombo_->currentText()), Q_ARG(QString, pol),
        Q_ARG(double, sigmaSpin_->value()));
}

void MainWindow::onStartMosaic()
{
    if (mosaicInEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("拼接"), QStringLiteral("请先选择输入目录"));
        return;
    }
    setBusy(true);
    progress_->setValue(0);
    appendLog(QStringLiteral("---- 拼接开始 ----"));
    QMetaObject::invokeMethod(worker_, "startMosaic", Qt::QueuedConnection,
                              Q_ARG(QString, mosaicInEdit_->text().trimmed()),
                              Q_ARG(QString, mosaicOutEdit_->text().trimmed()),
                              Q_ARG(double, medSpin_->value()));
}

void MainWindow::onBrowseToZeroSrc()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择源 DAT"),
                                                   tzSrcEdit_->text(),
                                                   QStringLiteral("DAT (*.dat);;所有文件 (*)"));
    if (!f.isEmpty())
        tzSrcEdit_->setText(f);
}

void MainWindow::onBrowseToZeroOut()
{
    const QString f = QFileDialog::getSaveFileName(this, QStringLiteral("选择输出基路径"),
                                                   tzOutEdit_->text(),
                                                   QStringLiteral("DAT (*.dat);;所有文件 (*)"));
    if (!f.isEmpty())
        tzOutEdit_->setText(f);
}

void MainWindow::onStartToZero()
{
    if (tzSrcEdit_->text().trimmed().isEmpty() || tzOutEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ToZeroData"),
                             QStringLiteral("请指定源 DAT 与输出基路径"));
        return;
    }
    setBusy(true);
    progress_->setValue(0);
    appendLog(QStringLiteral("---- ToZeroData 开始 ----"));
    QMetaObject::invokeMethod(worker_, "startToZero", Qt::QueuedConnection,
                              Q_ARG(QString, tzSrcEdit_->text().trimmed()),
                              Q_ARG(QString, tzOutEdit_->text().trimmed()),
                              Q_ARG(bool, tzRecCheck_->isChecked()));
}

void MainWindow::onCancel()
{
    if (worker_)
        worker_->requestCancel();
    appendLog(QStringLiteral("正在取消…"));
}

void MainWindow::onFinished(bool ok, const QString &msg)
{
    setBusy(false);
    appendLog(msg);
    if (ok)
        progress_->setValue(100);
    else
        QMessageBox::warning(this, QStringLiteral("任务结束"), msg.isEmpty() ? QStringLiteral("失败")
                                                                              : msg);
}

void MainWindow::onNeedRectRoi(const QImage &preview, int fullNr, int fullNa)
{
    RectPickDialog dlg(preview, fullNr, fullNa, QStringLiteral("框选角反缓冲区"), this);
    const bool ok = (dlg.exec() == QDialog::Accepted) && dlg.acceptedOk();
    const RectRoi r = dlg.roi();
    worker_->provideRectRoi(ok, QRect(r.x, r.y, r.w, r.h));
}
