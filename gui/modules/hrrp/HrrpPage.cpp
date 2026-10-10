#include "HrrpPage.h"
#include "common/SharedDataSession.h"
#include "ui/LogProgressPanel.h"
#include "ui/InteractivePlotWidget.h"
#include "common/OutPath.h"
#include "common/OutDirField.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QSizePolicy>
#include <QTimer>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QMetaObject>
#include <QLabel>
#include <QApplication>
#include <QDialog>

HrrpPage::HrrpPage(SharedDataSession *session, QWidget *parent)
    : QWidget(parent)
    , session_(session)
    , folderEdit_(0)
    , outDir_(0)
    , nameEdit_(0)
    , polCombo_(0)
    , sigmaSpin_(0)
    , cropSpin_(0)
    , startBtn_(0)
    , cancelBtn_(0)
    , log_(0)
    , plot_(0)
    , thread_(0)
    , worker_(0)
{

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *pathBox = new QGroupBox(QStringLiteral("数据路径"), this);
    auto *pathLay = new QVBoxLayout(pathBox);
    pathLay->setSpacing(6);
    folderEdit_ = new QLineEdit(pathBox);
    folderEdit_->setMinimumHeight(28);
    folderEdit_->setPlaceholderText(QStringLiteral("选择含 *_L0_*.dat 的目录"));
    auto *folderBrowse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    folderBrowse->setFixedWidth(72);
    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(new QLabel(QStringLiteral("数据文件夹"), pathBox));
    folderRow->addWidget(folderEdit_, 1);
    folderRow->addWidget(folderBrowse);
    pathLay->addLayout(folderRow);
    outDir_ = new OutDirField(this);
    pathLay->addLayout(outDir_->createRow(pathBox));
    outDir_->setPlaceholderHint(QStringLiteral("数据文件夹"), QStringLiteral("HRRP_<极化>"));
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
    nameEdit_ = new QLineEdit(QStringLiteral("目标"), paramBox);
    form->addRow(QStringLiteral("目标/背景名称"), nameEdit_);
    polCombo_ = new QComboBox(paramBox);
    polCombo_->addItems(QStringList() << "HH" << "HV" << "VV" << "VH");
    form->addRow(QStringLiteral("极化"), polCombo_);
    sigmaSpin_ = new QDoubleSpinBox(paramBox);
    sigmaSpin_->setRange(-50, 80);
    sigmaSpin_->setValue(18.25);
    sigmaSpin_->setSuffix(QStringLiteral(" dBsm"));
    form->addRow(QStringLiteral("定标体RCS"), sigmaSpin_);
    cropSpin_ = new QSpinBox(paramBox);
    cropSpin_->setRange(256, 65536);
    cropSpin_->setValue(8192);
    form->addRow(QStringLiteral("距离向点数"), cropSpin_);
    leftLay->addWidget(paramBox);

    auto *btns = new QHBoxLayout;
    startBtn_ = new QPushButton(QStringLiteral("开始处理"), left);
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
    auto *cursorLabel = new QLabel(QStringLiteral(""), right);//滚轮缩放(有界) · 拖拽平移 · 双击复位
    plot_ = new InteractivePlotWidget(right);
    plot_->setAxisTitles(QStringLiteral("距离 (m)"), QStringLiteral("RCS (dBsm)"));
    plot_->setPlotTitle(QStringLiteral("一维距离像 (HRRP)"));
    rightLay->addWidget(cursorLabel);
    rightLay->addWidget(plot_, 1);
    connect(plot_, &InteractivePlotWidget::cursorInfo, cursorLabel, &QLabel::setText);

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
    worker_ = new HrrpWorker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &HrrpWorker::progress, log_, &LogProgressPanel::setProgress);
    connect(worker_, &HrrpWorker::message, log_, &LogProgressPanel::appendLog);
    connect(worker_, &HrrpWorker::errorOccurred, this, [this](const QString &e) {
        log_->appendLog(QStringLiteral("[错误] ") + e);
        QMessageBox::warning(this, QStringLiteral("HRRP"), e);
    });
    connect(worker_, &HrrpWorker::needCornerConfirm, this, &HrrpPage::onNeedCorner, Qt::QueuedConnection);
    connect(worker_, &HrrpWorker::hrrpReady, this, &HrrpPage::onReady);
    connect(worker_, &HrrpWorker::finishedOk, this, [this]() { setBusy(false); });
    connect(worker_, &HrrpWorker::finishedCancelled, this, [this]() {
        setBusy(false);
        log_->appendLog(QStringLiteral("任务已取消"));
    });
    thread_->start();

    connect(folderBrowse, &QPushButton::clicked, this, &HrrpPage::onBrowseFolder);
    connect(folderEdit_, &QLineEdit::editingFinished, this, [this]() {
        if (session_)
            session_->setDataFolder(folderEdit_->text().trimmed());
        else
            outDir_->resetFromBase(folderEdit_->text().trimmed(),
                                   hrrpResultTag(polCombo_->currentText()));
    });
    connect(polCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshDefaultOut();
    });
    connect(startBtn_, &QPushButton::clicked, this, &HrrpPage::onStart);
    connect(cancelBtn_, &QPushButton::clicked, this, &HrrpPage::onCancel);
    if (session_) {
        connect(session_, &SharedDataSession::dataFolderChanged, this, &HrrpPage::onDataFolderChanged);
        if (!session_->dataFolder().isEmpty())
            onDataFolderChanged(session_->dataFolder());
    }
}

HrrpPage::~HrrpPage()
{
    if (thread_) {
        worker_->requestCancel();
        thread_->quit();
        thread_->wait(3000);
    }
}

void HrrpPage::onDataFolderChanged(const QString &path)
{
    if (folderEdit_ && folderEdit_->text() != path)
        folderEdit_->setText(path);
    outDir_->resetFromBase(path, hrrpResultTag(polCombo_->currentText()));
}

void HrrpPage::refreshDefaultOut()
{
    outDir_->refresh(folderEdit_ ? folderEdit_->text().trimmed() : QString(),
                     hrrpResultTag(polCombo_->currentText()));
}

void HrrpPage::onBrowseFolder()
{
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择数据文件夹"),
                                                        folderEdit_->text());
    if (d.isEmpty())
        return;
    folderEdit_->setText(d);
    if (session_)
        session_->setDataFolder(d);
    else
        outDir_->resetFromBase(d, hrrpResultTag(polCombo_->currentText()));
}

void HrrpPage::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void HrrpPage::onStart()
{
    if (folderEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("HRRP"), QStringLiteral("请先选择数据文件夹"));
        return;
    }
    if (session_)
        session_->setDataFolder(folderEdit_->text().trimmed());
    refreshDefaultOut();
    HrrpParams p;
    p.dataFolder = folderEdit_->text().trimmed();
    p.outDir = outDir_->text();
    p.tgtName = nameEdit_->text().trimmed().isEmpty() ? QStringLiteral("目标") : nameEdit_->text().trimmed();
    p.pol = polCombo_->currentText();
    p.sigmaTheoryDb = sigmaSpin_->value();
    p.cropN = cropSpin_->value();
    worker_->setParams(p);
    log_->reset();
    setBusy(true);
    QMetaObject::invokeMethod(worker_, "startProcess", Qt::QueuedConnection);
}

void HrrpPage::onCancel()
{
    log_->appendLog(QStringLiteral("正在取消…"));
    worker_->requestCancel();
    if (auto *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
        dlg->reject();
}

void HrrpPage::onNeedCorner(const QVector<double> &R, const QVector<double> &profileDb, int peakIdx)
{
    plot_->setData(R, profileDb);
    const double sug = (peakIdx >= 0 && peakIdx < R.size()) ? R[peakIdx] : 0;
    log_->appendLog(QStringLiteral("请核对角反峰值（约 %1 m）…").arg(sug, 0, 'f', 1));
    raise();
    activateWindow();
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, QStringLiteral("核对角反"),
        QStringLiteral("自动峰值距离 %1 m。直接回车确认，或输入正确距离(m)：").arg(sug, 0, 'f', 1),
        QLineEdit::Normal, QString(), &ok);
    UserSelection sel;
    if (ok && !text.trimmed().isEmpty())
        sel.cornerRangeOverrideM = text.trimmed().toDouble();
    else if (ok)
        sel.cornerRangeOverrideM = -1;
    else {
        worker_->requestCancel();
        sel.cornerRangeOverrideM = -1;
    }
    // 必须直接调用：Worker 阻塞在 waitForSelection，QueuedConnection 无法执行
    worker_->provideSelection(sel);
}

void HrrpPage::onReady(const HrrpResult &result)
{
    plot_->setData(result.rangeM, result.rcsDbsm);
    log_->appendLog(QStringLiteral("已保存: %1").arg(result.hrrpPath));
    log_->appendLog(QStringLiteral("已保存: %1").arg(result.pdatPath));
}
