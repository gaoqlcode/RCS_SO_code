#include "L0PreviewPage.h"
#include "common/SharedDataSession.h"
#include "ui/LogProgressPanel.h"
#include "ui/InteractivePlotWidget.h"
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
#include <QMetaObject>
#include <QMetaType>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

L0PreviewPage::L0PreviewPage(SharedDataSession *session, QWidget *parent)
    : QWidget(parent)
    , session_(session)
    , folderEdit_(0)
    , polCombo_(0)
    , pulseSpin_(0)
    , fsSpin_(0)
    , dcCheck_(0)
    , startBtn_(0)
    , cancelBtn_(0)
    , prevBtn_(0)
    , nextBtn_(0)
    , log_(0)
    , fileLabel_(0)
    , timePlot_(0)
    , freqPlot_(0)
    , index_(0)
    , thread_(0)
    , worker_(0)
{
    qRegisterMetaType<QVector<L0PulsePreviewItem> >("QVector<L0PulsePreviewItem>");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // 旧版模板：顶栏路径 + 左右分栏（左参数/日志，右时域/频域图）
    auto *pathBox = new QGroupBox(QStringLiteral("数据路径"), this);
    auto *pathLay = new QVBoxLayout(pathBox);
    pathLay->setSpacing(6);
    folderEdit_ = new QLineEdit(pathBox);
    folderEdit_->setMinimumHeight(28);
    folderEdit_->setPlaceholderText(QStringLiteral("含 *_L0_<极化>.dat 的目录"));
    auto *browse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    browse->setFixedWidth(72);
    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(new QLabel(QStringLiteral("数据文件夹"), pathBox));
    folderRow->addWidget(folderEdit_, 1);
    folderRow->addWidget(browse);
    pathLay->addLayout(folderRow);
    root->addWidget(pathBox);

    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);

    auto *left = new QWidget(split);
    left->setMinimumWidth(480);
    left->setStyleSheet(QStringLiteral(
        "QComboBox,QSpinBox,QDoubleSpinBox,QLineEdit,QPushButton{min-height:30px;}"
        "QGroupBox{font-weight:600; padding-top:8px;}"));
    left->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(10, 4, 10, 4);
    leftLay->setSpacing(10);

    auto *paramBox = new QGroupBox(QStringLiteral("处理参数"), left);
    auto *form = new QFormLayout(paramBox);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    form->setContentsMargins(8, 10, 8, 8);
    polCombo_ = new QComboBox(paramBox);
    polCombo_->addItems(QStringList() << "HH" << "HV" << "VV" << "VH");
    form->addRow(QStringLiteral("极化方式"), polCombo_);
    pulseSpin_ = new QSpinBox(paramBox);
    pulseSpin_->setRange(1, 10000);
    pulseSpin_->setValue(1);
    form->addRow(QStringLiteral("选用脉冲号"), pulseSpin_);
    fsSpin_ = new QDoubleSpinBox(paramBox);
    fsSpin_->setRange(1e6, 1e11);
    fsSpin_->setDecimals(0);
    fsSpin_->setValue(1.25e9);
    fsSpin_->setSuffix(QStringLiteral(" Hz"));
    form->addRow(QStringLiteral("采样率"), fsSpin_);
    dcCheck_ = new QCheckBox(QStringLiteral("去掉直流（建议勾选）"), paramBox);
    dcCheck_->setChecked(true);
    form->addRow(QStringLiteral(""), dcCheck_);
    leftLay->addWidget(paramBox);

    auto *btns = new QHBoxLayout;
    startBtn_ = new QPushButton(QStringLiteral("开始预览"), left);
    cancelBtn_ = new QPushButton(QStringLiteral("取消"), left);
    cancelBtn_->setEnabled(false);
    btns->addWidget(startBtn_);
    btns->addWidget(cancelBtn_);
    btns->addStretch();
    leftLay->addLayout(btns);

    log_ = new LogProgressPanel(left);
    leftLay->addWidget(log_, 1);

    auto *right = new QWidget(split);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(6, 0, 0, 0);
    fileLabel_ = new QLabel(QStringLiteral("尚未预览。开始后在此显示时域/频域曲线。"), right);
    fileLabel_->setStyleSheet(QStringLiteral("font-weight:600; padding:2px;"));
    fileLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileLabel_->setWordWrap(true);
    rightLay->addWidget(fileLabel_);

    timePlot_ = new InteractivePlotWidget(right);
    timePlot_->setAxisTitles(QStringLiteral("时间 (μs)"), QStringLiteral("信号幅度"));
    timePlot_->setPlotTitle(QStringLiteral("时域特性"));
    freqPlot_ = new InteractivePlotWidget(right);
    freqPlot_->setAxisTitles(QStringLiteral("频率 (MHz)"), QStringLiteral("频谱幅度 (dB)"));
    freqPlot_->setPlotTitle(QStringLiteral("频域特性"));
    freqPlot_->setFixedYRange(-60.0, 3.0);
    rightLay->addWidget(timePlot_, 1);
    rightLay->addWidget(freqPlot_, 1);

    auto *nav = new QHBoxLayout;
    prevBtn_ = new QPushButton(QStringLiteral("上一组"), right);
    nextBtn_ = new QPushButton(QStringLiteral("下一组"), right);
    prevBtn_->setEnabled(false);
    nextBtn_->setEnabled(false);
    nav->addWidget(prevBtn_);
    nav->addWidget(nextBtn_);
    nav->addStretch();
    rightLay->addLayout(nav);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes(QList<int>() << 560 << 1000);
    // 首帧再设一次，避免被右侧图最小宽度挤窄
    QTimer::singleShot(0, split, [split]() {
        split->setSizes(QList<int>() << 560 << qMax(800, split->width() - 560));
    });
    root->addWidget(split, 1);

    thread_ = new QThread(this);
    worker_ = new L0PreviewWorker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &L0PreviewWorker::progress, log_, &LogProgressPanel::setProgress);
    connect(worker_, &L0PreviewWorker::message, log_, &LogProgressPanel::appendLog);
    connect(worker_, &L0PreviewWorker::errorOccurred, this, [this](const QString &e) {
        log_->appendLog(QStringLiteral("[错误] ") + e);
        QMessageBox::warning(this, QStringLiteral("原始数据预览"), e);
    });
    connect(worker_, &L0PreviewWorker::previewReady, this, &L0PreviewPage::onReady);
    connect(worker_, &L0PreviewWorker::finishedOk, this, [this]() { setBusy(false); });
    connect(worker_, &L0PreviewWorker::finishedCancelled, this, [this]() {
        setBusy(false);
        log_->appendLog(QStringLiteral("任务已取消"));
    });
    thread_->start();

    connect(browse, &QPushButton::clicked, this, &L0PreviewPage::onBrowseFolder);
    connect(folderEdit_, &QLineEdit::editingFinished, this, [this]() {
        if (session_)
            session_->setDataFolder(folderEdit_->text().trimmed());
    });
    connect(startBtn_, &QPushButton::clicked, this, &L0PreviewPage::onStart);
    connect(cancelBtn_, &QPushButton::clicked, this, &L0PreviewPage::onCancel);
    connect(prevBtn_, &QPushButton::clicked, this, &L0PreviewPage::prevItem);
    connect(nextBtn_, &QPushButton::clicked, this, &L0PreviewPage::nextItem);
    connect(timePlot_, &InteractivePlotWidget::plotClicked, this, &L0PreviewPage::nextItem);
    connect(freqPlot_, &InteractivePlotWidget::plotClicked, this, &L0PreviewPage::nextItem);
    if (session_) {
        connect(session_, &SharedDataSession::dataFolderChanged, this,
                &L0PreviewPage::onDataFolderChanged);
        if (!session_->dataFolder().isEmpty())
            onDataFolderChanged(session_->dataFolder());
    }
}

L0PreviewPage::~L0PreviewPage()
{
    if (worker_)
        worker_->requestCancel();
    if (thread_) {
        thread_->quit();
        thread_->wait(3000);
    }
}

void L0PreviewPage::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void L0PreviewPage::onBrowseFolder()
{
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择 0 级数据文件夹"),
                                                        folderEdit_->text());
    if (d.isEmpty())
        return;
    folderEdit_->setText(d);
    if (session_)
        session_->setDataFolder(d);
}

void L0PreviewPage::onDataFolderChanged(const QString &path)
{
    if (folderEdit_->text() != path)
        folderEdit_->setText(path);
}

void L0PreviewPage::onStart()
{
    if (folderEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("原始数据预览"),
                             QStringLiteral("请先选择数据文件夹"));
        return;
    }
    if (session_)
        session_->setDataFolder(folderEdit_->text().trimmed());
    L0PreviewParams p;
    p.dataFolder = folderEdit_->text().trimmed();
    p.pol = polCombo_->currentText();
    p.fsHz = fsSpin_->value();
    p.pulseNo = pulseSpin_->value();
    p.removeDC = dcCheck_->isChecked();
    p.maxFiles = 0;
    worker_->setParams(p);
    setBusy(true);
    log_->appendLog(QStringLiteral("提交预览任务…"));
    QMetaObject::invokeMethod(worker_, "startProcess", Qt::QueuedConnection);
}

void L0PreviewPage::onCancel()
{
    if (worker_)
        worker_->requestCancel();
}

void L0PreviewPage::onReady(const QVector<L0PulsePreviewItem> &items)
{
    items_ = items;
    index_ = 0;
    const bool ok = !items_.isEmpty();
    prevBtn_->setEnabled(ok);
    nextBtn_->setEnabled(ok);
    if (!ok) {
        fileLabel_->setText(QStringLiteral("没有可显示的预览"));
        QMessageBox::information(this, QStringLiteral("原始数据预览"),
                                 QStringLiteral("没有可显示的预览"));
        return;
    }
    log_->appendLog(QStringLiteral("预览完成，共 %1 个文件（右侧切换）").arg(items_.size()));
    showIndex(0);
}

void L0PreviewPage::showIndex(int idx)
{
    if (items_.isEmpty())
        return;
    if (idx < 0 || idx >= items_.size())
        idx = 0;
    index_ = idx;
    const L0PulsePreviewItem &it = items_[index_];

    fileLabel_->setText(QStringLiteral("[%1/%2]  文件名：%3")
                            .arg(index_ + 1)
                            .arg(items_.size())
                            .arg(QString::fromUtf8(it.fileName.data(),
                                                   static_cast<int>(it.fileName.size()))));

    QVector<double> t, amp, f, sdB;
    t.reserve(static_cast<int>(it.tUs.size()));
    amp.reserve(static_cast<int>(it.amp.size()));
    for (size_t i = 0; i < it.tUs.size() && i < it.amp.size(); ++i) {
        t.push_back(it.tUs[i]);
        amp.push_back(it.amp[i]);
    }
    f.reserve(static_cast<int>(it.fMHz.size()));
    sdB.reserve(static_cast<int>(it.spectrumDb.size()));
    for (size_t i = 0; i < it.fMHz.size() && i < it.spectrumDb.size(); ++i) {
        f.push_back(it.fMHz[i]);
        sdB.push_back(it.spectrumDb[i]);
    }

    // 批量刷新，避免 setData/setPlotTitle 各触发一次整图重绘
    timePlot_->setUpdatesEnabled(false);
    freqPlot_->setUpdatesEnabled(false);
    timePlot_->setData(t, amp);
    timePlot_->setPlotTitle(QStringLiteral("时域特性 | 第%1脉冲 | 峰值 %2 μs | 信号区 %3~%4 μs")
                                .arg(it.pulseSel)
                                .arg(it.ampPkUs, 0, 'f', 4)
                                .arg(it.sig1Us, 0, 'f', 4)
                                .arg(it.sig2Us, 0, 'f', 4));
    freqPlot_->setData(f, sdB);
    // 不要写 90%%：链式 .arg() 后常残留两个 %
    freqPlot_->setPlotTitle(
        QStringLiteral("频域特性 | 谱峰 %1 MHz | 90%占用带宽 %2 MHz")
            .arg(QString::number(it.fPkMHz, 'f', 3), QString::number(it.occBwMHz, 'f', 3)));
    timePlot_->setUpdatesEnabled(true);
    freqPlot_->setUpdatesEnabled(true);
    timePlot_->update();
    freqPlot_->update();
}

void L0PreviewPage::nextItem()
{
    if (items_.isEmpty())
        return;
    index_ = (index_ + 1) % items_.size();
    showIndex(index_);
}

void L0PreviewPage::prevItem()
{
    if (items_.isEmpty())
        return;
    index_ = (index_ - 1 + items_.size()) % items_.size();
    showIndex(index_);
}
