#include "MainWindow.h"
#include "CalMosaicWorker.h"
#include "ManualAlignDialog.h"
#include "RectPickDialog.h"

#include "rcs/rcs.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QImage>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
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
#include <cstring>

namespace {

QString qs(const std::string &s)
{
    return QString::fromUtf8(s.c_str());
}

std::string ss(const QString &s)
{
    return std::string(s.toUtf8().constData());
}

QImage grayToQImage(const GrayImage &g)
{
    if (g.width <= 0 || g.height <= 0 || g.pixels.empty())
        return QImage();
    QImage img(g.width, g.height, QImage::Format_Grayscale8);
    for (int y = 0; y < g.height; ++y) {
        std::memcpy(img.scanLine(y), &g.pixels[static_cast<size_t>(y) * g.width],
                    static_cast<size_t>(g.width));
    }
    return img;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , calibEdit_(0)
    , rawDirEdit_(0)
    , l0Edit_(0)
    , mergeEdit_(0)
    , calOutEdit_(0)
    , bandCombo_(0)
    , polCombo_(0)
    , sigmaSpin_(0)
    , calStartBtn_(0)
    , mosaicInEdit_(0)
    , mosaicOutEdit_(0)
    , mosaicModeCombo_(0)
    , mosaicRawNrSpin_(0)
    , mosaicRawNaSpin_(0)
    , medSpin_(0)
    , flattenTifEdit_(0)
    , mosaicListBtn_(0)
    , mosaicStartBtn_(0)
    , flattenBtn_(0)
    , ziSrcEdit_(0)
    , ziOutEdit_(0)
    , ziKindCombo_(0)
    , ziBeginSpin_(0)
    , ziDeltaSpin_(0)
    , ziLastSpin_(0)
    , ziNumSpin_(0)
    , ziOffsetSpin_(0)
    , ziRangeSpin_(0)
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
    l0Edit_ = new QLineEdit;
    l0Edit_->setPlaceholderText(QStringLiteral("可选，留空则按极化自动找 *_L0_HH.dat"));
    mergeEdit_ = new QLineEdit;
    mergeEdit_->setPlaceholderText(QStringLiteral("留空则按目标框从同名 .cplx 生成并写入三级"));
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
    auto *l0Row = new QWidget;
    {
        auto *hl = new QHBoxLayout(l0Row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(l0Edit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseL0);
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
    calForm->addRow(QStringLiteral("0 级头文件（可空）"), l0Row);
    auto *mergeRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(mergeRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(mergeEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseMerge);
    }
    calForm->addRow(QStringLiteral("距离文件（留空则自动生成）"), mergeRow);
    calForm->addRow(QStringLiteral("输出目录（可空）"), outRow);
    calForm->addRow(QStringLiteral("波段 band"), bandCombo_);
    calForm->addRow(QStringLiteral("极化 pol"), polCombo_);
    calForm->addRow(QStringLiteral("角反理论 RCS"), sigmaSpin_);
    calLay->addLayout(calForm);
    calStartBtn_ = new QPushButton(QStringLiteral("开始定标（先框角反，再框目标）"));
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
    mosaicModeCombo_ = new QComboBox;
    mosaicModeCombo_->addItem(QStringLiteral("自动拼接"), 0);
    mosaicModeCombo_->addItem(QStringLiteral("手动拼接"), 1);
    mosaicRawNrSpin_ = new QSpinBox;
    mosaicRawNaSpin_ = new QSpinBox;
    mosaicRawNrSpin_->setRange(0, 1000000);
    mosaicRawNaSpin_->setRange(0, 1000000);
    mosaicRawNrSpin_->setSpecialValueText(QStringLiteral("自动"));
    mosaicRawNaSpin_->setSpecialValueText(QStringLiteral("自动"));
    flattenTifEdit_ = new QLineEdit;
    flattenTifEdit_->setPlaceholderText(QStringLiteral("已有拼接 TIFF，单独匀光"));
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
    auto *flatRow = new QWidget;
    {
        auto *hl = new QHBoxLayout(flatRow);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(flattenTifEdit_, 1);
        auto *b = new QPushButton(QStringLiteral("浏览…"));
        hl->addWidget(b);
        connect(b, &QPushButton::clicked, this, &MainWindow::onBrowseFlattenTif);
    }
    mosForm->addRow(QStringLiteral("输入目录（≥2 张）"), minRow);
    mosForm->addRow(QStringLiteral("输出目录"), moutRow);
    mosForm->addRow(QStringLiteral("拼接方式"), mosaicModeCombo_);
    mosForm->addRow(QStringLiteral("RAW 行数"), mosaicRawNrSpin_);
    mosForm->addRow(QStringLiteral("RAW 列数"), mosaicRawNaSpin_);
    mosForm->addRow(QStringLiteral("匀光 medOut"), medSpin_);
    mosForm->addRow(QStringLiteral("单独匀光 TIFF"), flatRow);
    mosLay->addLayout(mosForm);
    mosaicListBtn_ = new QPushButton(QStringLiteral("列出 TIFF（listMosaicTiffs）"));
    mosaicStartBtn_ = new QPushButton(QStringLiteral("开始拼接（processMosaic）"));
    flattenBtn_ = new QPushButton(QStringLiteral("单独匀光（flattenMosaic）"));
    mosLay->addWidget(mosaicListBtn_);
    mosLay->addWidget(mosaicStartBtn_);
    mosLay->addWidget(flattenBtn_);
    mosLay->addStretch(1);
    connect(mosaicListBtn_, &QPushButton::clicked, this, &MainWindow::onListMosaicTiffs);
    connect(mosaicStartBtn_, &QPushButton::clicked, this, &MainWindow::onStartMosaic);
    connect(flattenBtn_, &QPushButton::clicked, this, &MainWindow::onStartFlatten);
    tabs->addTab(mosPage, QStringLiteral("大图拼接"));

    // ---- 零级成像 ----
    auto *ziPage = new QWidget;
    auto *ziLay = new QVBoxLayout(ziPage);
    auto *ziForm = new QFormLayout;
    ziSrcEdit_ = new QLineEdit;
    ziOutEdit_ = new QLineEdit;
    ziSrcEdit_->setPlaceholderText(QStringLiteral("含原始 DAT 的目录"));
    ziOutEdit_->setPlaceholderText(QStringLiteral("L0 与图像输出目录"));
    ziKindCombo_ = new QComboBox;
    ziKindCombo_->addItem(QStringLiteral("SAR 成像"), 0);
    ziKindCombo_->addItem(QStringLiteral("ISAR 成像"), 1);
    ziBeginSpin_ = new QSpinBox;
    ziDeltaSpin_ = new QSpinBox;
    ziLastSpin_ = new QSpinBox;
    ziNumSpin_ = new QSpinBox;
    ziOffsetSpin_ = new QSpinBox;
    ziRangeSpin_ = new QSpinBox;
    ziBeginSpin_->setRange(0, 1000000);
    ziDeltaSpin_->setRange(0, 1000000);
    ziLastSpin_->setRange(0, 1000000);
    ziNumSpin_->setRange(2, 1000000);
    ziOffsetSpin_->setRange(-1000000, 1000000);
    ziRangeSpin_->setRange(1, 10000000);
    ziBeginSpin_->setValue(3);
    ziDeltaSpin_->setValue(120);
    ziLastSpin_->setValue(6000);
    ziNumSpin_->setValue(50);
    ziOffsetSpin_->setValue(100);
    ziRangeSpin_->setValue(8192);
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
    ziForm->addRow(QStringLiteral("源 DAT 目录"), ziSrcRow);
    ziForm->addRow(QStringLiteral("L0 目录"), ziOutRow);
    ziForm->addRow(QStringLiteral("成像类型"), ziKindCombo_);
    ziForm->addRow(QStringLiteral("起始编号"), ziBeginSpin_);
    ziForm->addRow(QStringLiteral("间隔数目"), ziDeltaSpin_);
    ziForm->addRow(QStringLiteral("最后编号"), ziLastSpin_);
    ziForm->addRow(QStringLiteral("文件个数"), ziNumSpin_);
    ziForm->addRow(QStringLiteral("距离偏移"), ziOffsetSpin_);
    ziForm->addRow(QStringLiteral("距离向点数"), ziRangeSpin_);
    ziLay->addLayout(ziForm);
    ziStartBtn_ = new QPushButton(QStringLiteral("转换并成像"));
    ziLay->addWidget(ziStartBtn_);
    ziLay->addStretch(1);
    connect(ziKindCombo_, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onZeroKindChanged);
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
    mosaicListBtn_->setEnabled(!busy);
    mosaicStartBtn_->setEnabled(!busy);
    flattenBtn_->setEnabled(!busy);
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

void MainWindow::onBrowseL0()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择 0 级头文件"),
                                                   l0Edit_->text(),
                                                   QStringLiteral("DAT (*.dat);;所有文件 (*)"));
    if (!f.isEmpty())
        l0Edit_->setText(f);
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
        worker_, "startCal", Qt::QueuedConnection,         Q_ARG(QString, calibEdit_->text().trimmed()),
        Q_ARG(QString, rawDirEdit_->text().trimmed()), Q_ARG(QString, calOutEdit_->text().trimmed()),
        Q_ARG(QString, bandCombo_->currentText()), Q_ARG(QString, pol),
        Q_ARG(double, sigmaSpin_->value()), Q_ARG(QString, l0Edit_->text().trimmed()),
        Q_ARG(QString, mergeEdit_->text().trimmed()));
}

void MainWindow::onBrowseFlattenTif()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择要匀光的 TIFF"),
                                                   flattenTifEdit_->text(),
                                                   QStringLiteral("TIFF (*.tif *.tiff);;所有文件 (*)"));
    if (!f.isEmpty())
        flattenTifEdit_->setText(f);
}

void MainWindow::onListMosaicTiffs()
{
    if (mosaicInEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("拼接"), QStringLiteral("请先选择输入目录"));
        return;
    }
    const std::vector<std::string> paths = listMosaicTiffs(ss(mosaicInEdit_->text().trimmed()));
    appendLog(QStringLiteral("---- listMosaicTiffs：%1 张 ----").arg(static_cast<int>(paths.size())));
    if (paths.empty())
        appendLog(QStringLiteral("目录里没有 TIFF。若只有 RAW，开始拼接时会先转换。"));
    for (size_t i = 0; i < paths.size(); ++i)
        appendLog(QStringLiteral("  %1").arg(qs(paths[i])));
}

bool MainWindow::collectManualOffsets(QVector<int> &offR, QVector<int> &offC)
{
    appendLog(QStringLiteral("调用 prepareMosaicInputs…"));
    QApplication::processEvents();
    std::vector<std::string> pathStd;
    Callbacks pcb;
    pcb.onMessage = [this](const std::string &s) { appendLog(qs(s)); };
    pcb.onProgress = [this](int p, const std::string &s) {
        setProgress(p, qs(s));
        appendLog(qs(s));
    };
    pcb.isCancelled = []() { return false; };
    std::string errStd;
    if (!prepareMosaicInputs(ss(mosaicInEdit_->text().trimmed()), ss(mosaicOutEdit_->text().trimmed()),
                             pathStd, pcb, &errStd, mosaicRawNrSpin_->value(),
                             mosaicRawNaSpin_->value())) {
        QMessageBox::warning(this, QStringLiteral("手动拼接"), qs(errStd));
        return false;
    }
    if (pathStd.size() < 2) {
        QMessageBox::warning(this, QStringLiteral("手动拼接"), QStringLiteral("至少需要 2 张图像"));
        return false;
    }

    const int step = 8;
    const int n = static_cast<int>(pathStd.size());
    QVector<QImage> previews(n);
    appendLog(QStringLiteral("调用 loadMosaicGrayPreview（抽稀×%1）…").arg(step));
    for (int i = 0; i < n; ++i) {
        int fw = 0, fh = 0;
        GrayImage gp;
        if (!loadMosaicGrayPreview(pathStd[static_cast<size_t>(i)], step, gp, fw, fh, &errStd)) {
            QMessageBox::warning(this, QStringLiteral("手动拼接"), qs(errStd));
            return false;
        }
        previews[i] = grayToQImage(gp);
        appendLog(QStringLiteral("  [%1] %2 (%3×%4)")
                      .arg(i + 1)
                      .arg(QFileInfo(qs(pathStd[static_cast<size_t>(i)])).fileName())
                      .arg(fw)
                      .arg(fh));
        QApplication::processEvents();
    }

    offR = QVector<int>(n, 0);
    offC = QVector<int>(n, 0);
    for (int i = 1; i < n; ++i) {
        ManualAlignDialog dlg(previews[i - 1], previews[i], step, i, n - 1, this);
        if (dlg.exec() != QDialog::Accepted) {
            appendLog(QStringLiteral("用户取消手动对齐"));
            return false;
        }
        offR[i] = offR[i - 1] + dlg.dRowFull();
        offC[i] = offC[i - 1] + dlg.dColFull();
        appendLog(QStringLiteral("第%1→%2张: Δ行=%3 Δ列=%4")
                      .arg(i)
                      .arg(i + 1)
                      .arg(dlg.dRowFull())
                      .arg(dlg.dColFull()));
        if (dlg.skipRestAuto()) {
            const int nom = qMax(1, previews[i].width() / 3) * step;
            for (int j = i + 1; j < n; ++j) {
                offR[j] = offR[j - 1];
                offC[j] = offC[j - 1] + nom;
            }
            appendLog(QStringLiteral("剩余对按名义步进自动摆位"));
            break;
        }
    }
    return true;
}

void MainWindow::onStartMosaic()
{
    if (mosaicInEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("拼接"), QStringLiteral("请先选择输入目录"));
        return;
    }
    const int mode = mosaicModeCombo_->currentData().toInt();
    QVector<int> offR, offC;
    if (mode == 1) {
        progress_->setValue(0);
        appendLog(QStringLiteral("---- 手动拼接 ----"));
        if (!collectManualOffsets(offR, offC))
            return;
        worker_->setMosaicOffsets(offR, offC);
    }
    setBusy(true);
    progress_->setValue(0);
    appendLog(mode == 1 ? QStringLiteral("手动摆位完成，开始融合写出…")
                        : QStringLiteral("---- 自动拼接 ----"));
    QMetaObject::invokeMethod(worker_, "startMosaic", Qt::QueuedConnection,
                              Q_ARG(QString, mosaicInEdit_->text().trimmed()),
                              Q_ARG(QString, mosaicOutEdit_->text().trimmed()),
                              Q_ARG(double, medSpin_->value()), Q_ARG(int, mode),
                              Q_ARG(int, mosaicRawNrSpin_->value()),
                              Q_ARG(int, mosaicRawNaSpin_->value()));
}

void MainWindow::onStartFlatten()
{
    if (flattenTifEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("匀光"), QStringLiteral("请先选择 TIFF"));
        return;
    }
    setBusy(true);
    progress_->setValue(0);
    appendLog(QStringLiteral("---- 单独匀光 ----"));
    QMetaObject::invokeMethod(worker_, "startFlatten", Qt::QueuedConnection,
                              Q_ARG(QString, flattenTifEdit_->text().trimmed()),
                              Q_ARG(QString, QString()), Q_ARG(double, medSpin_->value()));
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

void MainWindow::onZeroKindChanged(int index)
{
    const bool isar = index == 1;
    ziBeginSpin_->setValue(isar ? 12 : 3);
    ziDeltaSpin_->setValue(isar ? 100 : 120);
    ziLastSpin_->setValue(6000);
    ziNumSpin_->setValue(isar ? 8 : 50);
    ziOffsetSpin_->setValue(isar ? 1140 : 100);
    ziRangeSpin_->setValue(isar ? 512 : 8192);
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
        Q_ARG(int, ziNumSpin_->value()), Q_ARG(int, ziOffsetSpin_->value()),
        Q_ARG(int, ziRangeSpin_->value()), Q_ARG(int, ziKindCombo_->currentData().toInt()));
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

void MainWindow::onBrowseMerge()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择距离 txt"),
                                                   mergeEdit_->text(),
                                                   QStringLiteral("文本 (*.txt);;所有文件 (*)"));
    if (!f.isEmpty())
        mergeEdit_->setText(f);
}

void MainWindow::onNeedRectRoi(const QImage &preview, int fullNr, int fullNa, const QString &stage)
{
    const QString title = (stage == QStringLiteral("target"))
                              ? QStringLiteral("框选目标（交给成像方的范围）")
                              : QStringLiteral("框选角反缓冲区");
    RectPickDialog dlg(preview, fullNr, fullNa, title, this);
    const bool ok = (dlg.exec() == QDialog::Accepted) && dlg.acceptedOk();
    const RectRoi r = dlg.roi();
    worker_->provideRectRoi(ok, QRect(r.x, r.y, r.w, r.h));
}
