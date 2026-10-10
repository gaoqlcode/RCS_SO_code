#include "CwRcsPage.h"
#include "common/SharedDataSession.h"
#include "common/OutPath.h"
#include "common/OutDirField.h"
#include "ui/LogProgressPanel.h"
#include "ui/InteractivePlotWidget.h"
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
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMetaObject>
#include <QRegExp>
#include <QApplication>

CwRcsPage::CwRcsPage(SharedDataSession *session, QWidget *parent)
    : QWidget(parent)
    , session_(session)
    , folderEdit_(0)
    , outDir_(0)
    , nameEdit_(0)
    , polCombo_(0)
    , sigmaSpin_(0)
    , startBtn_(0)
    , cancelBtn_(0)
    , log_(0)
    , plot_(0)
    , cursorLabel_(0)
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
    outDir_->setPlaceholderHint(QStringLiteral("数据文件夹"), QStringLiteral("点频RCS_<极化>"));
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
    cursorLabel_ = new QLabel(QStringLiteral(""), right);
    plot_ = new InteractivePlotWidget(right);
    plot_->setAxisTitles(QStringLiteral("频率 (GHz)"), QStringLiteral("RCS (dBsm)"));
    plot_->setPlotTitle(QStringLiteral("点频 RCS–频率"));
    rightLay->addWidget(cursorLabel_);
    rightLay->addWidget(plot_, 1);
    connect(plot_, &InteractivePlotWidget::cursorInfo, cursorLabel_, &QLabel::setText);

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
    worker_ = new CwRcsWorker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &CwRcsWorker::progress, log_, &LogProgressPanel::setProgress);
    connect(worker_, &CwRcsWorker::message, log_, &LogProgressPanel::appendLog);
    connect(worker_, &CwRcsWorker::errorOccurred, this, [this](const QString &e) {
        log_->appendLog(QStringLiteral("[错误] ") + e);
        QMessageBox::warning(this, QStringLiteral("点频RCS"), e);
    });
    connect(worker_, &CwRcsWorker::needCornerConfirm, this, &CwRcsPage::onNeedCorner,
            Qt::QueuedConnection);
    connect(worker_, &CwRcsWorker::needTargetPick, this, &CwRcsPage::onNeedTargetPick,
            Qt::QueuedConnection);
    connect(worker_, &CwRcsWorker::cwRcsReady, this, &CwRcsPage::onReady);
    connect(worker_, &CwRcsWorker::finishedOk, this, [this]() { setBusy(false); });
    connect(worker_, &CwRcsWorker::finishedCancelled, this, [this]() {
        setBusy(false);
        log_->appendLog(QStringLiteral("任务已取消"));
    });
    thread_->start();

    connect(folderBrowse, &QPushButton::clicked, this, &CwRcsPage::onBrowseFolder);
    connect(folderEdit_, &QLineEdit::editingFinished, this, [this]() {
        if (session_)
            session_->setDataFolder(folderEdit_->text().trimmed());
        else
            outDir_->resetFromBase(folderEdit_->text().trimmed(),
                                   cwRcsResultTag(polCombo_->currentText()));
    });
    connect(polCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshDefaultOut();
    });
    connect(startBtn_, &QPushButton::clicked, this, &CwRcsPage::onStart);
    connect(cancelBtn_, &QPushButton::clicked, this, &CwRcsPage::onCancel);
    if (session_) {
        connect(session_, &SharedDataSession::dataFolderChanged, this, &CwRcsPage::onDataFolderChanged);
        if (!session_->dataFolder().isEmpty())
            onDataFolderChanged(session_->dataFolder());
    }
}

CwRcsPage::~CwRcsPage()
{
    if (thread_) {
        worker_->requestCancel();
        thread_->quit();
        thread_->wait(3000);
    }
}

void CwRcsPage::onDataFolderChanged(const QString &path)
{
    if (folderEdit_ && folderEdit_->text() != path)
        folderEdit_->setText(path);
    outDir_->resetFromBase(path, cwRcsResultTag(polCombo_->currentText()));
}

void CwRcsPage::refreshDefaultOut()
{
    outDir_->refresh(folderEdit_ ? folderEdit_->text().trimmed() : QString(),
                     cwRcsResultTag(polCombo_->currentText()));
}

void CwRcsPage::onBrowseFolder()
{
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择数据文件夹"),
                                                        folderEdit_->text());
    if (d.isEmpty())
        return;
    folderEdit_->setText(d);
    if (session_)
        session_->setDataFolder(d);
    else
        outDir_->resetFromBase(d, cwRcsResultTag(polCombo_->currentText()));
}

void CwRcsPage::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void CwRcsPage::onStart()
{
    if (folderEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("点频RCS"), QStringLiteral("请先选择数据文件夹"));
        return;
    }
    if (session_)
        session_->setDataFolder(folderEdit_->text().trimmed());
    refreshDefaultOut();
    CwRcsParams p;
    p.dataFolder = folderEdit_->text().trimmed();
    p.outDir = outDir_->text();
    p.tgtName = nameEdit_->text().trimmed().isEmpty() ? QStringLiteral("目标")
                                                      : nameEdit_->text().trimmed();
    p.pol = polCombo_->currentText();
    p.sigmaTheoryDb = sigmaSpin_->value();
    worker_->setParams(p);
    log_->reset();
    setBusy(true);
    QMetaObject::invokeMethod(worker_, "startProcess", Qt::QueuedConnection);
}

void CwRcsPage::onCancel()
{
    log_->appendLog(QStringLiteral("正在取消…"));
    worker_->requestCancel();
    if (auto *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
        dlg->reject();
}

void CwRcsPage::onNeedCorner(const QVector<double> &R, const QVector<double> &profileDb, int peakIdx)
{
    plot_->setAxisTitles(QStringLiteral("距离 (m)"), QStringLiteral("功率 (dB)"));
    plot_->setPlotTitle(QStringLiteral("点频：核对角反"));
    plot_->setData(R, profileDb);
    const double sug = (peakIdx >= 0 && peakIdx < R.size()) ? R[peakIdx] : 0;
    bool ok = false;
    const QString text = QInputDialog::getText(
        this, QStringLiteral("核对角反"),
        QStringLiteral("自动峰值约 %1 m。回车确认，或输入正确距离(m)：").arg(sug, 0, 'f', 1),
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
    worker_->provideSelection(sel);
}

UserSelection CwRcsPage::runTargetDialog(const QVector<double> &R, const QVector<double> &meanPowerDb)
{
    UserSelection sel;
    sel.targetMode = 1;
    bool ok = false;
    const QStringList items =
        QStringList() << QStringLiteral("1 = 点目标") << QStringLiteral("2 = 扩展目标");
    const QString item =
        QInputDialog::getItem(this, QStringLiteral("目标模式"), QStringLiteral("选择："), items, 0,
                              false, &ok);
    if (!ok)
        return sel;
    sel.targetMode = items.indexOf(item) + 1;

    if (sel.targetMode == 2) {
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("扩展目标：输入距离段"));
        dlg.resize(900, 560);
        auto *lay = new QVBoxLayout(&dlg);
        auto *pw = new InteractivePlotWidget(&dlg);
        pw->setAxisTitles(QStringLiteral("距离(m)"), QStringLiteral("功率dB"));
        pw->setData(R, meanPowerDb);
        lay->addWidget(pw, 1);
        auto *hint = new QLabel(QStringLiteral("输入起止距离(m)，可多次添加"), &dlg);
        lay->addWidget(hint);
        auto *edit = new QLineEdit(&dlg);
        lay->addWidget(edit);
        auto *addBtn = new QPushButton(QStringLiteral("添加距离段"), &dlg);
        lay->addWidget(addBtn);
        auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        lay->addWidget(box);
        connect(addBtn, &QPushButton::clicked, &dlg, [&]() {
            const QStringList parts =
                edit->text().trimmed().split(QRegExp("\\s+"), QString::SkipEmptyParts);
            if (parts.size() >= 2) {
                sel.extendedRanges.push_back(qMakePair(parts[0].toDouble(), parts[1].toDouble()));
                hint->setText(QStringLiteral("已添加 %1 段").arg(sel.extendedRanges.size()));
                edit->clear();
            }
        });
        connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted)
            sel.extendedRanges.clear();
        return sel;
    }

    // 点目标：输入距离(m)
    const QString text = QInputDialog::getText(
        this, QStringLiteral("点目标"), QStringLiteral("输入目标距离(m)，空则用功率峰："),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return sel;
    if (!text.trimmed().isEmpty()) {
        const double rm = text.trimmed().toDouble();
        int best = 0;
        double bestD = 1e99;
        for (int i = 0; i < R.size(); ++i) {
            const double d = qAbs(R[i] - rm);
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        PickPoint pt;
        pt.az = 0;
        pt.rg = best;
        sel.targets.push_back(pt);
    } else {
        // 用峰值
        int pk = 0;
        double best = -1e99;
        for (int i = 0; i < meanPowerDb.size(); ++i) {
            if (meanPowerDb[i] > best) {
                best = meanPowerDb[i];
                pk = i;
            }
        }
        PickPoint pt;
        pt.az = 0;
        pt.rg = pk;
        sel.targets.push_back(pt);
    }
    return sel;
}

void CwRcsPage::onNeedTargetPick(const QVector<double> &R, const QImage &, int, int,
                                 const QVector<double> &meanPowerDb, int)
{
    plot_->setAxisTitles(QStringLiteral("距离 (m)"), QStringLiteral("功率 (dB)"));
    plot_->setPlotTitle(QStringLiteral("点频：选目标"));
    plot_->setData(R, meanPowerDb);
    UserSelection sel = runTargetDialog(R, meanPowerDb);
    worker_->provideSelection(sel);
}

void CwRcsPage::onReady(const CwRcsResultQt &bundle)
{
    plot_->setAxisTitles(QStringLiteral("频率 (GHz)"), QStringLiteral("RCS (dBsm)"));
    plot_->setPlotTitle(QStringLiteral("点频 RCS–频率"));
    plot_->setData(bundle.freqGHz, bundle.rcsDbsm);
    log_->appendLog(QStringLiteral("已保存: %1").arg(bundle.l3Path));
}
