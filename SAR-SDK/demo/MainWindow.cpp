#include "MainWindow.h"
#include "CalMosaicWorker.h"
#include "RectPickDialog.h"

#include "rcs/rcs.hpp"

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
#include <QSpinBox>
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
    , ziSrcEdit_(0)
    , ziOutEdit_(0)
    , ziEngineEdit_(0)
    , ziMcrEdit_(0)
    , ziBeginSpin_(0)
    , ziDeltaSpin_(0)
    , ziLastSpin_(0)
    , ziNumSpin_(0)
    , ziTimeoutSpin_(0)
    , ziStartBtn_(0)
    , cancelBtn_(0)
    , progress_(0)
    , log_(0)
    , workerThread_(0)
    , worker_(0)
    , busy_(false)
{
    setWindowTitle(QStringLiteral("SAR-SDK 示例 — 定标 / 拼接 / 零级成像  (rcs %1)")
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
        QStringLiteral("推荐输入定标产物 *_RCS_dBsm.tif")));
    mosLay->addStretch(1);
    connect(mosaicStartBtn_, &QPushButton::clicked, this, &MainWindow::onStartMosaic);
    tabs->addTab(mosPage, QStringLiteral("大图拼接"));

    // ---- 零级成像 ----
    auto *ziPage = new QWidget;
    auto *ziLay = new QVBoxLayout(ziPage);
    auto *ziForm = new QFormLayout;
    ziSrcEdit_ = new QLineEdit;
    ziOutEdit_ = new QLineEdit;
    ziSrcEdit_->setPlaceholderText(QStringLiteral("含原始 DAT 的目录"));
    ziOutEdit_->setPlaceholderText(QStringLiteral("L0 与图像输出目录"));
    ziEngineEdit_ = new QLineEdit;
    ziEngineEdit_->setPlaceholderText(QStringLiteral("留空则使用 SDK/engine/for_testing"));
    ziMcrEdit_ = new QLineEdit(QStringLiteral("/usr/local/MATLAB/MATLAB_Runtime/v93"));
    ziBeginSpin_ = new QSpinBox;
    ziDeltaSpin_ = new QSpinBox;
    ziLastSpin_ = new QSpinBox;
    ziNumSpin_ = new QSpinBox;
    ziTimeoutSpin_ = new QSpinBox;
    ziBeginSpin_->setRange(0, 1000000);
    ziDeltaSpin_->setRange(0, 1000000);
    ziLastSpin_->setRange(0, 1000000);
    ziNumSpin_->setRange(2, 1000000);
    ziTimeoutSpin_->setRange(0, 86400);
    ziTimeoutSpin_->setSpecialValueText(QStringLiteral("不限制"));
    ziBeginSpin_->setValue(1);
    ziDeltaSpin_->setValue(20);
    ziLastSpin_->setValue(20);
    ziNumSpin_->setValue(20);
    auto *ziSrcRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(ziSrcRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(ziSrcEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseZeroSrc);
    }
    auto *ziOutRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(ziOutRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(ziOutEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseZeroOut);
    }
    auto *ziEngRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(ziEngRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(ziEngineEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseZeroEngine);
    }
    ziForm->addRow(QStringLiteral("源 DAT 目录"), ziSrcRow);
    ziForm->addRow(QStringLiteral("L0 目录"), ziOutRow);
    ziForm->addRow(QStringLiteral("起始编号"), ziBeginSpin_);
    ziForm->addRow(QStringLiteral("间隔数目"), ziDeltaSpin_);
    ziForm->addRow(QStringLiteral("最后编号"), ziLastSpin_);
    ziForm->addRow(QStringLiteral("文件个数"), ziNumSpin_);
    ziForm->addRow(QStringLiteral("引擎目录"), ziEngRow);
    ziForm->addRow(QStringLiteral("MCR 目录"), ziMcrEdit_);
    ziForm->addRow(QStringLiteral("超时（秒）"), ziTimeoutSpin_);
    ziLay->addLayout(ziForm);
    ziStartBtn_ = new QPushButton(QStringLiteral("转换并成像"));
    ziLay->addWidget(ziStartBtn_);
    ziLay->addWidget(new QLabel(QStringLiteral(
        "一次调用 processZeroImageFolder：把目录内这一层的源 DAT 全部转为 HH/VV/HV/VH 与 .sarrec，再对 L0 目录成像。\n"
        "图像写回 L0 目录。已是 _HH/_VV/_HV/_VH 的文件会跳过。\n"
        "需 MATLAB Runtime v9.3。\n"
        "引擎见 SDK/engine/for_testing。文件个数至少为 2。")));
    ziLay->addStretch(1);
    connect(ziStartBtn_, &QPushButton::clicked, this, &MainWindow::onStartZeroImage);
    tabs->addTab(ziPage, QStringLiteral("零级成像"));

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
    ziStartBtn_->setEnabled(!busy);
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

void MainWindow::onBrowseZeroSrc()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选择源 DAT 目录"), ziSrcEdit_->text());
    if (!d.isEmpty())
        ziSrcEdit_->setText(d);
}

void MainWindow::onBrowseZeroOut()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选择 L0 输出目录"), ziOutEdit_->text());
    if (!d.isEmpty())
        ziOutEdit_->setText(d);
}

void MainWindow::onBrowseZeroEngine()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选择引擎目录"), ziEngineEdit_->text());
    if (!d.isEmpty())
        ziEngineEdit_->setText(d);
}

void MainWindow::onStartZeroImage()
{
    if (ziSrcEdit_->text().trimmed().isEmpty() || ziOutEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("零级成像"),
                             QStringLiteral("请指定源 DAT 目录与 L0 输出目录"));
        return;
    }
    setBusy(true);
    progress_->setValue(0);
    appendLog(QStringLiteral("---- 目录内源 DAT 转 L0 并成像 ----"));
    QMetaObject::invokeMethod(
        worker_, "startZeroImage", Qt::QueuedConnection, Q_ARG(QString, ziSrcEdit_->text().trimmed()),
        Q_ARG(QString, ziOutEdit_->text().trimmed()), Q_ARG(int, ziBeginSpin_->value()),
        Q_ARG(int, ziDeltaSpin_->value()), Q_ARG(int, ziLastSpin_->value()),
        Q_ARG(int, ziNumSpin_->value()), Q_ARG(QString, ziEngineEdit_->text().trimmed()),
        Q_ARG(QString, ziMcrEdit_->text().trimmed()), Q_ARG(int, ziTimeoutSpin_->value()));
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
