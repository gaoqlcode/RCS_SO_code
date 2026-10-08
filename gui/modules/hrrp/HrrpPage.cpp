#include "HrrpPage.h"
#include "common/SharedDataSession.h"
#include "ui/LogProgressPanel.h"
#include "ui/InteractivePlotWidget.h"
#include "common/OutPath.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
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
#include <QDir>
#include <QApplication>
#include <QDialog>

HrrpPage::HrrpPage(SharedDataSession *session, QWidget *parent)
    : QWidget(parent)
    , session_(session)
    , outEdit_(0)
    , nameEdit_(0)
    , polCombo_(0)
    , sigmaSpin_(0)
    , cropSpin_(0)
    , startBtn_(0)
    , cancelBtn_(0)
    , log_(0)
    , plot_(0)
    , outDirUserEdited_(false)
    , thread_(0)
    , worker_(0)
{

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *pathBox = new QGroupBox(QStringLiteral("输出路径"), this);
    auto *pathLay = new QVBoxLayout(pathBox);
    pathLay->setSpacing(6);
    outEdit_ = new QLineEdit(pathBox);
    outEdit_->setMinimumHeight(28);
    outEdit_->setPlaceholderText(QStringLiteral("数据文件夹/处理结果_HRRP_<极化>"));
    auto *outBrowse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    outBrowse->setFixedWidth(72);
    auto *outRow = new QHBoxLayout;
    outRow->addWidget(new QLabel(QStringLiteral("输出目录"), pathBox));
    outRow->addWidget(outEdit_, 1);
    outRow->addWidget(outBrowse);
    pathLay->addLayout(outRow);
    root->addWidget(pathBox);

    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);

    auto *left = new QWidget(split);
    left->setMinimumWidth(300);
    left->setMaximumWidth(420);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 6, 0);

    auto *paramBox = new QGroupBox(QStringLiteral("处理参数"), left);
    auto *form = new QFormLayout(paramBox);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
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
    form->addRow(QStringLiteral("裁剪点数"), cropSpin_);
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
    split->setSizes(QList<int>() << 340 << 1200);
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

    connect(outBrowse, &QPushButton::clicked, this, &HrrpPage::onBrowseOut);
    connect(polCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshDefaultOut();
    });
    connect(outEdit_, &QLineEdit::textEdited, this, [this](const QString &) {
        outDirUserEdited_ = true;
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

void HrrpPage::onDataFolderChanged(const QString &)
{
    outDirUserEdited_ = false;
    refreshDefaultOut();
}

void HrrpPage::refreshDefaultOut()
{
    const QString data = session_ ? session_->dataFolder().trimmed() : QString();
    if (data.isEmpty())
        return;
    const QString autoPath = defaultResultDir(data, hrrpResultTag(polCombo_->currentText()));
    if (!outDirUserEdited_ || outEdit_->text().trimmed().isEmpty()
        || outEdit_->text().trimmed() == lastAutoOut_) {
        outEdit_->setText(QDir::toNativeSeparators(autoPath));
        lastAutoOut_ = outEdit_->text();
        outDirUserEdited_ = false;
    }
}


void HrrpPage::onBrowseOut()
{
    const QString start = outEdit_->text().trimmed().isEmpty()
                              ? (session_ ? session_->dataFolder() : QString())
                              : outEdit_->text().trimmed();
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"), start);
    if (!d.isEmpty()) {
        outEdit_->setText(d);
        outDirUserEdited_ = true;
    }
}

void HrrpPage::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void HrrpPage::onStart()
{
    if (!session_ || session_->dataFolder().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("HRRP"), QStringLiteral("请先在顶部选择数据文件夹"));
        return;
    }
    refreshDefaultOut();
    HrrpParams p;
    p.dataFolder = session_->dataFolder().trimmed();
    p.outDir = outEdit_->text().trimmed();
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
    log_->appendLog(QStringLiteral("请核对角反峰值（约 %.1f m）…").arg(sug));
    raise();
    activateWindow();
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, QStringLiteral("核对角反"),
        QStringLiteral("自动峰值距离 %.1f m。直接回车确认，或输入正确距离(m)：").arg(sug),
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
