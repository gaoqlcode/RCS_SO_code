#include "MainWindow.h"
#include "ConfirmBridge.h"
#include "SimplePlotWidget.h"
#include "rcs/rcs_api.h"

#include <QByteArray>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

std::string toUtf8(const QString &s)
{
    const QByteArray u = s.toUtf8();
    return std::string(u.constData(), static_cast<size_t>(u.size()));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , confirmBridge_(0)
    , previewIndex_(0)
{
    setWindowTitle(QStringLiteral("RCS SDK gui_demo (%1)").arg(QString::fromUtf8(rcsVersion())));
    resize(1180, 780);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    // --- 路径 ---
    auto *pathBox = new QGroupBox(QStringLiteral("数据"), central);
    auto *form = new QFormLayout(pathBox);

    inEdit_ = new QLineEdit(pathBox);
    inEdit_->setPlaceholderText(QStringLiteral("放 *_L0_*.dat 的文件夹"));
    auto *inBrowse = new QPushButton(QStringLiteral("浏览"), pathBox);
    auto *inRow = new QHBoxLayout;
    inRow->addWidget(inEdit_, 1);
    inRow->addWidget(inBrowse);
    form->addRow(QStringLiteral("输入"), inRow);

    outEdit_ = new QLineEdit(pathBox);
    outEdit_->setPlaceholderText(QStringLiteral("HRRP 输出目录，可先空着"));
    auto *outBrowse = new QPushButton(QStringLiteral("浏览"), pathBox);
    auto *outRow = new QHBoxLayout;
    outRow->addWidget(outEdit_, 1);
    outRow->addWidget(outBrowse);
    form->addRow(QStringLiteral("输出"), outRow);

    polCombo_ = new QComboBox(pathBox);
    polCombo_->addItems(QStringList() << "HH" << "HV" << "VV" << "VH");
    auto *refreshPol = new QPushButton(QStringLiteral("扫一下极化"), pathBox);
    auto *polRow = new QHBoxLayout;
    polRow->addWidget(polCombo_, 1);
    polRow->addWidget(refreshPol);
    form->addRow(QStringLiteral("极化"), polRow);
    root->addWidget(pathBox);

    // --- 按钮 ---
    auto *btns = new QHBoxLayout;
    probeBtn_ = new QPushButton(QStringLiteral("探查"), central);
    previewBtn_ = new QPushButton(QStringLiteral("预览"), central);
    hrrpBtn_ = new QPushButton(QStringLiteral("跑 HRRP"), central);
    cancelBtn_ = new QPushButton(QStringLiteral("取消"), central);
    cancelBtn_->setEnabled(false);
    btns->addWidget(probeBtn_);
    btns->addWidget(previewBtn_);
    btns->addWidget(hrrpBtn_);
    btns->addWidget(cancelBtn_);
    btns->addStretch();
    root->addLayout(btns);

    auto *hint = new QLabel(
        QStringLiteral("库在后台线程跑。HRRP 会弹窗确认峰距。曲线自己画。"),
        central);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#666;"));
    root->addWidget(hint);

    auto *split = new QSplitter(Qt::Horizontal, central);

    // 左边：信息和日志
    auto *left = new QWidget(split);
    auto *leftLay = new QVBoxLayout(left);
    infoLabel_ = new QLabel(QStringLiteral("（探查结果）"), left);
    infoLabel_->setWordWrap(true);
    infoLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    leftLay->addWidget(infoLabel_, 1);
    log_ = new QTextEdit(left);
    log_->setReadOnly(true);
    log_->setMaximumHeight(200);
    leftLay->addWidget(log_);
    bar_ = new QProgressBar(left);
    bar_->setRange(0, 100);
    leftLay->addWidget(bar_);

    // 右边：图
    auto *right = new QWidget(split);
    auto *rightLay = new QVBoxLayout(right);
    fileLabel_ = new QLabel(QStringLiteral("文件名"), right);
    auto *nav = new QHBoxLayout;
    prevBtn_ = new QPushButton(QStringLiteral("上一个"), right);
    nextBtn_ = new QPushButton(QStringLiteral("下一个"), right);
    nav->addWidget(prevBtn_);
    nav->addWidget(nextBtn_);
    nav->addStretch();

    timePlot_ = new SimplePlotWidget(right);
    timePlot_->setAxisTitles(QStringLiteral("时间 (μs)"), QStringLiteral("幅度"));
    freqPlot_ = new SimplePlotWidget(right);
    freqPlot_->setAxisTitles(QStringLiteral("频率 (MHz)"), QStringLiteral("dB"));
    freqPlot_->setYRange(-60, 3);
    hrrpPlot_ = new SimplePlotWidget(right);
    hrrpPlot_->setAxisTitles(QStringLiteral("距离 (m)"), QStringLiteral("RCS (dBsm)"));
    hrrpPlot_->setTitle(QStringLiteral("HRRP"));

    rightLay->addWidget(fileLabel_);
    rightLay->addLayout(nav);
    rightLay->addWidget(timePlot_, 1);
    rightLay->addWidget(freqPlot_, 1);
    rightLay->addWidget(hrrpPlot_, 1);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes(QList<int>() << 320 << 860);
    root->addWidget(split, 1);
    setCentralWidget(central);

    confirmBridge_ = new ConfirmBridge(this); // 留在 UI 线程

    thread_ = new QThread(this);
    worker_ = new DemoWorker;
    worker_->setConfirmBridge(confirmBridge_);
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);

    connect(worker_, &DemoWorker::progress, this, [this](int p, const QString &m) {
        bar_->setValue(p);
        log_->append(m);
    });
    connect(worker_, &DemoWorker::logMessage, log_, &QTextEdit::append);
    connect(worker_, &DemoWorker::failed, this, [this](const QString &e) {
        log_->append(QStringLiteral("失败：") + e);
        QMessageBox::warning(this, QStringLiteral("出错了"), e);
    });
    connect(worker_, &DemoWorker::probeDone, infoLabel_, &QLabel::setText);
    connect(worker_, &DemoWorker::previewDone, this, [this](const QVector<PreviewBundle> &items) {
        previewItems_ = items;
        previewIndex_ = 0;
        showPreviewIndex(0);
        log_->append(QStringLiteral("预览好了，%1 个文件").arg(items.size()));
    });
    connect(worker_, &DemoWorker::hrrpDone, this,
            [this](const QString &path, const QVector<double> &R, const QVector<double> &rcs) {
                infoLabel_->setText(QStringLiteral("HRRP 写完了：\n") + path);
                hrrpPlot_->setData(R, rcs);
                hrrpPlot_->setTitle(QStringLiteral("HRRP"));
                log_->append(path);
            });
    connect(worker_, &DemoWorker::finished, this, [this]() { setBusy(false); });
    thread_->start();

    connect(inBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseIn);
    connect(outBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseOut);
    connect(refreshPol, &QPushButton::clicked, this, &MainWindow::onRefreshPol);
    connect(probeBtn_, &QPushButton::clicked, this, &MainWindow::onProbe);
    connect(previewBtn_, &QPushButton::clicked, this, &MainWindow::onPreview);
    connect(hrrpBtn_, &QPushButton::clicked, this, &MainWindow::onHrrp);
    connect(cancelBtn_, &QPushButton::clicked, this, &MainWindow::onCancel);
    connect(prevBtn_, &QPushButton::clicked, this, &MainWindow::onPrevFile);
    connect(nextBtn_, &QPushButton::clicked, this, &MainWindow::onNextFile);
}

MainWindow::~MainWindow()
{
    if (worker_)
        worker_->requestCancel();
    if (thread_) {
        thread_->quit();
        thread_->wait(3000);
    }
}

void MainWindow::setBusy(bool busy)
{
    probeBtn_->setEnabled(!busy);
    previewBtn_->setEnabled(!busy);
    hrrpBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void MainWindow::onBrowseIn()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选数据目录"), inEdit_->text());
    if (d.isEmpty())
        return;
    inEdit_->setText(d);
    if (outEdit_->text().trimmed().isEmpty())
        outEdit_->setText(d + QStringLiteral("/处理结果_HRRP_HH"));
    onRefreshPol();
}

void MainWindow::onBrowseOut()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选输出目录"), outEdit_->text());
    if (!d.isEmpty())
        outEdit_->setText(d);
}

void MainWindow::onRefreshPol()
{
    const QString folder = inEdit_->text().trimmed();
    if (folder.isEmpty())
        return;

    std::vector<std::string> pols;
    std::string err;
    if (!listAvailablePolarizations(toUtf8(folder), pols, &err)) {
        log_->append(QStringLiteral("扫极化失败：") + QString::fromStdString(err));
        return;
    }

    polCombo_->clear();
    for (size_t i = 0; i < pols.size(); ++i)
        polCombo_->addItem(QString::fromUtf8(pols[i].c_str()));
    log_->append(QStringLiteral("找到 %1 种极化").arg(polCombo_->count()));
}

void MainWindow::onProbe()
{
    if (inEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("先选数据目录"));
        return;
    }
    setBusy(true);
    bar_->setValue(0);
    QMetaObject::invokeMethod(worker_, "runProbe", Qt::QueuedConnection,
                              Q_ARG(QString, inEdit_->text().trimmed()),
                              Q_ARG(QString, polCombo_->currentText()));
}

void MainWindow::onPreview()
{
    if (inEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("先选数据目录"));
        return;
    }
    setBusy(true);
    bar_->setValue(0);
    log_->append(QStringLiteral("开始预览…"));
    QMetaObject::invokeMethod(worker_, "runPreview", Qt::QueuedConnection,
                              Q_ARG(QString, inEdit_->text().trimmed()),
                              Q_ARG(QString, polCombo_->currentText()));
}

void MainWindow::onHrrp()
{
    if (inEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("先选数据目录"));
        return;
    }

    QString out = outEdit_->text().trimmed();
    if (out.isEmpty()) {
        out = inEdit_->text().trimmed() + QStringLiteral("/处理结果_HRRP_") +
              polCombo_->currentText();
        outEdit_->setText(out);
    }

    setBusy(true);
    bar_->setValue(0);
    log_->append(QStringLiteral("跑 HRRP…"));
    QMetaObject::invokeMethod(worker_, "runHrrp", Qt::QueuedConnection,
                              Q_ARG(QString, inEdit_->text().trimmed()), Q_ARG(QString, out),
                              Q_ARG(QString, polCombo_->currentText()));
}

void MainWindow::onCancel()
{
    if (worker_)
        worker_->requestCancel();
}

void MainWindow::onPrevFile()
{
    if (previewItems_.isEmpty())
        return;
    previewIndex_ = (previewIndex_ - 1 + previewItems_.size()) % previewItems_.size();
    showPreviewIndex(previewIndex_);
}

void MainWindow::onNextFile()
{
    if (previewItems_.isEmpty())
        return;
    previewIndex_ = (previewIndex_ + 1) % previewItems_.size();
    showPreviewIndex(previewIndex_);
}

void MainWindow::showPreviewIndex(int idx)
{
    if (idx < 0 || idx >= previewItems_.size())
        return;
    const PreviewBundle &b = previewItems_[idx];
    fileLabel_->setText(
        QStringLiteral("%1 / %2   %3").arg(idx + 1).arg(previewItems_.size()).arg(b.fileName));
    timePlot_->setData(b.tUs, b.amp);
    timePlot_->setTitle(QStringLiteral("时域  峰 %1 us").arg(b.ampPkUs, 0, 'f', 4));
    freqPlot_->setData(b.fMHz, b.spectrumDb);
    freqPlot_->setTitle(QStringLiteral("频域  峰 %1 MHz").arg(b.fPkMHz, 0, 'f', 3));
}
