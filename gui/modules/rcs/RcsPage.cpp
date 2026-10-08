#include "RcsPage.h"
#include "common/SharedDataSession.h"
#include "ui/LogProgressPanel.h"
#include "ui/InteractivePlotWidget.h"
#include "ui/InteractiveImageWidget.h"
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QMetaObject>
#include <QRegExp>
#include <QDir>
#include <QApplication>
#include <QDesktopWidget>
#include <algorithm>

namespace {

void prepareModalDialog(QDialog &dlg, QWidget *anchor)
{
    dlg.setWindowModality(Qt::ApplicationModal);
    dlg.setWindowFlags((dlg.windowFlags() | Qt::Window) & ~Qt::WindowContextHelpButtonHint);
    const QRect ag = QApplication::desktop()->availableGeometry(anchor ? anchor : &dlg);
    const int w = qBound(960, 1280, ag.width() - 48);
    const int h = qBound(640, 800, ag.height() - 64);
    dlg.resize(w, h);
    dlg.move(ag.center() - QPoint(dlg.width() / 2, dlg.height() / 2));
}

int runRaisedDialog(QDialog &dlg)
{
    dlg.show();
    dlg.raise();
    dlg.activateWindow();
    QApplication::alert(&dlg, 3000);
    return dlg.exec();
}

} // namespace

RcsPage::RcsPage(SharedDataSession *session, QWidget *parent)
    : QWidget(parent)
    , session_(session)
    , outEdit_(0)
    , nameEdit_(0)
    , polCombo_(0)
    , azModeCombo_(0)
    , vSpin_(0)
    , sigmaSpin_(0)
    , cropSpin_(0)
    , startBtn_(0)
    , cancelBtn_(0)
    , log_(0)
    , plot_(0)
    , cursorLabel_(0)
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
    outEdit_->setPlaceholderText(QStringLiteral("数据文件夹/处理结果_RCS_<极化>"));
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
    azModeCombo_ = new QComboBox(paramBox);
    azModeCombo_->addItem(QStringLiteral("条带模式"), 1);
    azModeCombo_->addItem(QStringLiteral("聚束模式"), 2);
    azModeCombo_->setCurrentIndex(1);
    form->addRow(QStringLiteral("方位压缩"), azModeCombo_);
    vSpin_ = new QDoubleSpinBox(paramBox);
    vSpin_->setRange(0.1, 500);
    vSpin_->setValue(10);
    vSpin_->setSuffix(QStringLiteral(" m/s"));
    form->addRow(QStringLiteral("平台速度(条带)"), vSpin_);
    sigmaSpin_ = new QDoubleSpinBox(paramBox);
    sigmaSpin_->setRange(-50, 80);
    sigmaSpin_->setValue(18.25);
    form->addRow(QStringLiteral("定标体RCS(dBsm)"), sigmaSpin_);
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
    cursorLabel_ = new QLabel(QStringLiteral(""), right);//滚轮缩放(有界) · 拖拽平移 · 双击复位
    plot_ = new InteractivePlotWidget(right);
    plot_->setAxisTitles(QStringLiteral("距离 (m)"), QStringLiteral("RCS (dBsm)"));
    plot_->setPlotTitle(QStringLiteral("一维距离像 / RCS"));
    rightLay->addWidget(cursorLabel_);
    rightLay->addWidget(plot_, 1);
    connect(plot_, &InteractivePlotWidget::cursorInfo, cursorLabel_, &QLabel::setText);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes(QList<int>() << 340 << 1200);
    root->addWidget(split, 1);

    thread_ = new QThread(this);
    worker_ = new RcsWorker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &RcsWorker::progress, log_, &LogProgressPanel::setProgress);
    connect(worker_, &RcsWorker::message, log_, &LogProgressPanel::appendLog);
    connect(worker_, &RcsWorker::errorOccurred, this, [this](const QString &e) {
        log_->appendLog(QStringLiteral("[错误] ") + e);
        QMessageBox::warning(this, QStringLiteral("RCS"), e);
    });
    connect(worker_, &RcsWorker::needCornerPick, this, &RcsPage::onNeedCornerPick, Qt::QueuedConnection);
    connect(worker_, &RcsWorker::needTargetPick, this, &RcsPage::onNeedTargetPick, Qt::QueuedConnection);
    connect(worker_, &RcsWorker::rcsReady, this, &RcsPage::onReady);
    connect(worker_, &RcsWorker::finishedOk, this, [this]() { setBusy(false); });
    connect(worker_, &RcsWorker::finishedCancelled, this, [this]() {
        setBusy(false);
        log_->appendLog(QStringLiteral("任务已取消"));
    });
    thread_->start();

    connect(outBrowse, &QPushButton::clicked, this, &RcsPage::onBrowseOut);
    connect(polCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshDefaultOut();
    });
    connect(outEdit_, &QLineEdit::textEdited, this, [this](const QString &) {
        outDirUserEdited_ = true;
    });
    connect(startBtn_, &QPushButton::clicked, this, &RcsPage::onStart);
    connect(cancelBtn_, &QPushButton::clicked, this, &RcsPage::onCancel);
    if (session_) {
        connect(session_, &SharedDataSession::dataFolderChanged, this, &RcsPage::onDataFolderChanged);
        if (!session_->dataFolder().isEmpty())
            onDataFolderChanged(session_->dataFolder());
    }
}

RcsPage::~RcsPage()
{
    if (thread_) {
        worker_->requestCancel();
        thread_->quit();
        thread_->wait(3000);
    }
}

void RcsPage::onDataFolderChanged(const QString &)
{
    outDirUserEdited_ = false;
    refreshDefaultOut();
}

void RcsPage::refreshDefaultOut()
{
    const QString data = session_ ? session_->dataFolder().trimmed() : QString();
    if (data.isEmpty())
        return;
    const QString autoPath = defaultResultDir(data, rcsResultTag(polCombo_->currentText()));
    if (!outDirUserEdited_ || outEdit_->text().trimmed().isEmpty()
        || outEdit_->text().trimmed() == lastAutoOut_) {
        outEdit_->setText(QDir::toNativeSeparators(autoPath));
        lastAutoOut_ = outEdit_->text();
        outDirUserEdited_ = false;
    }
}


void RcsPage::onBrowseOut()
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

void RcsPage::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void RcsPage::onStart()
{
    if (!session_ || session_->dataFolder().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("RCS"), QStringLiteral("请先在顶部选择数据文件夹"));
        return;
    }
    refreshDefaultOut();
    RcsParams p;
    p.dataFolder = session_->dataFolder().trimmed();
    p.outDir = outEdit_->text().trimmed();
    p.tgtName = nameEdit_->text().trimmed().isEmpty() ? QStringLiteral("目标") : nameEdit_->text().trimmed();
    p.pol = polCombo_->currentText();
    p.azMode = azModeCombo_->currentData().toInt();
    p.vSar = vSpin_->value();
    p.sigmaTheoryDb = sigmaSpin_->value();
    p.cropN = cropSpin_->value();
    worker_->setParams(p);
    log_->reset();
    setBusy(true);
    QMetaObject::invokeMethod(worker_, "startProcess", Qt::QueuedConnection);
}

void RcsPage::onCancel()
{
    log_->appendLog(QStringLiteral("正在取消…"));
    // 必须直接调用：QueuedConnection 在 Worker 忙于计算时进不了事件循环
    worker_->requestCancel();
    if (auto *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
        dlg->reject();
}

UserSelection RcsPage::runPickDialog(const QString &title, const QVector<double> &R, const QImage &preview,
                                     int fullNr, int fullNp, const QVector<double> &meanPowerDb, bool askMode)
{
    UserSelection sel;
    sel.targetMode = 1;
    if (askMode) {
        bool ok = false;
        const QStringList items = QStringList() << QStringLiteral("1 = 点目标") << QStringLiteral("2 = 扩展目标");
        const QString item = QInputDialog::getItem(this, QStringLiteral("目标模式"), QStringLiteral("选择："), items, 0, false, &ok);
        if (!ok) return sel;
        sel.targetMode = items.indexOf(item) + 1;
    }

    if (askMode && sel.targetMode == 2) {
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("扩展目标：输入距离段"));
        prepareModalDialog(dlg, this);
        auto *lay = new QVBoxLayout(&dlg);
        auto *pw = new InteractivePlotWidget(&dlg);
        pw->setAxisTitles(QStringLiteral("距离(m)"), QStringLiteral("功率dB"));
        pw->setPlotTitle(QStringLiteral("平均功率剖面"));
        pw->setData(R, meanPowerDb);
        lay->addWidget(pw, 1);
        auto *hint = new QLabel(QStringLiteral("输入起止距离(m)，可多次添加；点错可「撤销上一段」"), &dlg);
        lay->addWidget(hint);
        auto *edit = new QLineEdit(&dlg);
        lay->addWidget(edit);
        auto *editRow = new QHBoxLayout;
        auto *addBtn = new QPushButton(QStringLiteral("添加距离段"), &dlg);
        auto *undoSegBtn = new QPushButton(QStringLiteral("撤销上一段"), &dlg);
        auto *clearSegBtn = new QPushButton(QStringLiteral("清空距离段"), &dlg);
        editRow->addWidget(addBtn);
        editRow->addWidget(undoSegBtn);
        editRow->addWidget(clearSegBtn);
        lay->addLayout(editRow);
        auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        lay->addWidget(box);
        connect(addBtn, &QPushButton::clicked, &dlg, [&]() {
            const QStringList parts = edit->text().trimmed().split(QRegExp("\\s+"), QString::SkipEmptyParts);
            if (parts.size() >= 2) {
                sel.extendedRanges.push_back(qMakePair(parts[0].toDouble(), parts[1].toDouble()));
                hint->setText(QStringLiteral("已添加 %1 段").arg(sel.extendedRanges.size()));
                edit->clear();
            }
        });
        connect(undoSegBtn, &QPushButton::clicked, &dlg, [&]() {
            if (!sel.extendedRanges.isEmpty()) {
                sel.extendedRanges.removeLast();
                hint->setText(QStringLiteral("已撤销，剩余 %1 段").arg(sel.extendedRanges.size()));
            }
        });
        connect(clearSegBtn, &QPushButton::clicked, &dlg, [&]() {
            sel.extendedRanges.clear();
            hint->setText(QStringLiteral("已清空距离段"));
        });
        connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        runRaisedDialog(dlg);
        return sel;
    }

    if (preview.isNull() || fullNr <= 0 || fullNp <= 0) {
        QMessageBox::warning(this, title, QStringLiteral("选点预览图为空，无法选点。请取消后重试。"));
        return sel;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(title + QStringLiteral("  （请在本窗口选点后点确定）"));
    prepareModalDialog(dlg, this);
    auto *lay = new QVBoxLayout(&dlg);
    auto *hint = new QLabel(
        QStringLiteral("左键选点；Shift+左键删除附近点；Backspace/Ctrl+Z 撤销；滚轮缩放(有界)。选完后点「确定」继续。"),
        &dlg);
    hint->setWordWrap(true);
    lay->addWidget(hint);
    auto *status = new QLabel(QStringLiteral("已选 0 点"), &dlg);
    lay->addWidget(status);
    auto *img = new InteractiveImageWidget(&dlg);
    img->setPreview(preview, R, fullNr, fullNp);
    lay->addWidget(img, 1);
    auto *editRow = new QHBoxLayout;
    auto *undoBtn = new QPushButton(QStringLiteral("撤销上一点"), &dlg);
    auto *clearBtn = new QPushButton(QStringLiteral("清空全部"), &dlg);
    editRow->addWidget(undoBtn);
    editRow->addWidget(clearBtn);
    editRow->addStretch();
    lay->addLayout(editRow);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    lay->addWidget(box);
    auto refreshStatus = [status, img]() {
        status->setText(QStringLiteral("已选 %1 点").arg(img->picks().size()));
    };
    connect(img, &InteractiveImageWidget::pickChanged, &dlg, [refreshStatus](const QVector<PickPoint> &) {
        refreshStatus();
    });
    connect(img, &InteractiveImageWidget::statusText, status, &QLabel::setText);
    connect(undoBtn, &QPushButton::clicked, &dlg, [img, refreshStatus]() {
        img->undoLastPick();
        refreshStatus();
        img->setFocus();
    });
    connect(clearBtn, &QPushButton::clicked, &dlg, [img, refreshStatus]() {
        img->clearPicks();
        refreshStatus();
        img->setFocus();
    });
    connect(box, &QDialogButtonBox::accepted, &dlg, [&]() {
        const int n = img->picks().size();
        if (n <= 0) {
            QMessageBox::information(&dlg, title, QStringLiteral("请先至少选 1 个点，再点确定。"));
            return;
        }
        dlg.accept();
    });
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    img->setFocus();
    if (runRaisedDialog(dlg) == QDialog::Accepted) {
        if (askMode)
            sel.targets = img->picks();
        else
            sel.corners = img->picks();
    }
    return sel;
}

void RcsPage::onNeedCornerPick(const QVector<double> &R, const QImage &preview, int fullNr, int fullNp)
{
    log_->appendLog(QStringLiteral("已弹出「角反选点」窗口，请在该窗口选点后点确定…"));
    raise();
    activateWindow();
    UserSelection sel = runPickDialog(QStringLiteral("第一步·角反选点"), R, preview, fullNr, fullNp, {}, false);
    // 必须直接调用：Worker 正阻塞在 waitForSelection，QueuedConnection 进不了事件循环
    worker_->provideSelection(sel);
}

void RcsPage::onNeedTargetPick(const QVector<double> &R, const QImage &preview, int fullNr, int fullNp,
                               const QVector<double> &meanPowerDb, int)
{
    log_->appendLog(QStringLiteral("已弹出「目标选点」窗口，请完成选择…"));
    raise();
    activateWindow();
    UserSelection sel = runPickDialog(QStringLiteral("第二步·目标"), R, preview, fullNr, fullNp, meanPowerDb, true);
    worker_->provideSelection(sel);
}

void RcsPage::onReady(const RcsResultBundle &bundle)
{
    plot_->setData(bundle.rangeM, bundle.profileDbsm);
    log_->appendLog(QStringLiteral("pdat: %1").arg(bundle.pdatPath));
    log_->appendLog(QStringLiteral("rcs: %1").arg(bundle.rcsPath));
    for (const auto &t : bundle.targets)
        log_->appendLog(QStringLiteral("%1: %.2f dBsm @ %.1f m").arg(t.label).arg(t.rcsDbsm).arg(t.rangeM));
}
