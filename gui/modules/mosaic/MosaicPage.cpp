#include "MosaicPage.h"
#include "ManualAlignDialog.h"
#include "common/RcsApiQt.h"
#include "FlattenPanel.h"
#include "ui/LogProgressPanel.h"
#include "common/OutPath.h"
#include "common/OutDirField.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QSizePolicy>
#include <QTimer>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QLabel>
#include <QMetaObject>
#include <QFileInfo>
#include <QApplication>
#include <QDialog>

MosaicPage::MosaicPage(QWidget *parent) : QWidget(parent)
{
    // 与 207 / RCS·HRRP 页一致：顶栏路径 + 左右分栏（左参数/日志，右结果与匀光）
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *pathBox = new QGroupBox(QStringLiteral("数据路径"), this);
    auto *pathLay = new QVBoxLayout(pathBox);
    pathLay->setSpacing(6);
    inEdit_ = new QLineEdit(pathBox);
    inEdit_->setMinimumHeight(28);
    inEdit_->setPlaceholderText(QStringLiteral("含 TIFF 或 RAW 的目录"));
    auto *browse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    browse->setFixedWidth(72);
    auto *inRow = new QHBoxLayout;
    inRow->addWidget(new QLabel(QStringLiteral("输入文件夹"), pathBox));
    inRow->addWidget(inEdit_, 1);
    inRow->addWidget(browse);
    pathLay->addLayout(inRow);

    outDir_ = new OutDirField(this);
    pathLay->addLayout(outDir_->createRow(pathBox));
    outDir_->setPlaceholderHint(QStringLiteral("输入文件夹"), mosaicResultTag());
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
    modeCombo_ = new QComboBox(paramBox);
    modeCombo_->addItem(QStringLiteral("自动拼接"), 0);
    modeCombo_->addItem(QStringLiteral("手动拼接"), 1);
    form->addRow(QStringLiteral("拼接方式"), modeCombo_);
    flattenCheck_ = new QCheckBox(QStringLiteral("拼完后自动匀光"), paramBox);
    flattenCheck_->setChecked(true);
    form->addRow(QString(), flattenCheck_);
    medSpin_ = new QDoubleSpinBox(paramBox);
    medSpin_->setRange(0.05, 0.9);
    medSpin_->setSingleStep(0.01);
    medSpin_->setValue(0.35);
    form->addRow(QStringLiteral("匀光目标中位"), medSpin_);
    rawNrSpin_ = new QSpinBox(paramBox);
    rawNrSpin_->setRange(0, 1000000);
    rawNrSpin_->setValue(0);
    rawNrSpin_->setSpecialValueText(QStringLiteral("自动"));
    form->addRow(QStringLiteral("RAW行数(0=自动)"), rawNrSpin_);
    rawNaSpin_ = new QSpinBox(paramBox);
    rawNaSpin_->setRange(0, 1000000);
    rawNaSpin_->setValue(0);
    rawNaSpin_->setSpecialValueText(QStringLiteral("自动"));
    form->addRow(QStringLiteral("RAW列数(0=自动)"), rawNaSpin_);
    leftLay->addWidget(paramBox);

    auto *btns = new QHBoxLayout;
    startBtn_ = new QPushButton(QStringLiteral("开始拼接"), left);
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
    resultLabel_ = new QLabel(
        QStringLiteral("拼接完成后，右侧显示结果路径；可在下方对已有 MOSAIC.tif 单独匀光。"),
        right);
    resultLabel_->setWordWrap(true);
    resultLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    resultLabel_->setStyleSheet(QStringLiteral("color:#444; padding:4px;"));
    resultLabel_->setMinimumHeight(120);
    rightLay->addWidget(resultLabel_, 1);

    auto *box = new QGroupBox(QStringLiteral("单独匀光（可对已有 MOSAIC.tif）"), right);
    auto *bl = new QVBoxLayout(box);
    flattenPanel_ = new FlattenPanel(box);
    bl->addWidget(flattenPanel_);
    rightLay->addWidget(box);

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
    worker_ = new MosaicWorker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &MosaicWorker::progress, log_, &LogProgressPanel::setProgress);
    connect(worker_, &MosaicWorker::message, log_, &LogProgressPanel::appendLog);
    connect(worker_, &MosaicWorker::errorOccurred, this, [this](const QString &e) {
        log_->appendLog(QStringLiteral("[错误] ") + e);
        QMessageBox::warning(this, QStringLiteral("拼接"), e);
    });
    connect(worker_, &MosaicWorker::mosaicReady, this, &MosaicPage::onReady);
    connect(worker_, &MosaicWorker::finishedOk, this, [this]() { setBusy(false); });
    connect(worker_, &MosaicWorker::finishedCancelled, this, [this]() {
        setBusy(false);
        log_->appendLog(QStringLiteral("任务已取消"));
    });
    connect(flattenPanel_, &FlattenPanel::applyRequested, this, &MosaicPage::onFlatten);
    thread_->start();

    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输入文件夹(TIFF或RAW)"));
        if (!d.isEmpty()) {
            inEdit_->setText(d);
            outDir_->resetFromBase(d, mosaicResultTag());
        }
    });
    connect(inEdit_, &QLineEdit::editingFinished, this, [this]() {
        outDir_->resetFromBase(inEdit_->text().trimmed(), mosaicResultTag());
    });
    connect(startBtn_, &QPushButton::clicked, this, &MosaicPage::onStart);
    connect(cancelBtn_, &QPushButton::clicked, this, &MosaicPage::onCancel);
}

MosaicPage::~MosaicPage()
{
    if (thread_) {
        worker_->requestCancel();
        thread_->quit();
        thread_->wait(3000);
    }
}

void MosaicPage::refreshDefaultOut()
{
    outDir_->refresh(inEdit_ ? inEdit_->text().trimmed() : QString(), mosaicResultTag());
}

void MosaicPage::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
    modeCombo_->setEnabled(!busy);
}

bool MosaicPage::collectManualOffsets(const QString &inFolder, const QString &outFolder,
                                      QVector<int> &offR, QVector<int> &offC,
                                      int rawNrHint, int rawNaHint)
{
    QStringList paths;
    log_->appendLog(QStringLiteral("准备输入（TIFF 直接用 / 仅 RAW 则转 TIFF）…"));
    QApplication::processEvents();
    std::vector<std::string> pathStd;
    Callbacks pcb;
    pcb.onMessage = [this](const std::string &s) { log_->appendLog(qs(s)); };
    pcb.isCancelled = []() { return false; };
    std::string errStd;
    if (!prepareMosaicInputs(ss(inFolder), ss(outFolder), pathStd, pcb, &errStd, rawNrHint,
                                  rawNaHint)) {
        QMessageBox::warning(this, QStringLiteral("手动拼接"), qs(errStd));
        return false;
    }
    for (const auto &p : pathStd)
        paths << qs(p);
    if (paths.size() < 2) {
        QMessageBox::warning(this, QStringLiteral("手动拼接"), QStringLiteral("至少需要2张图像"));
        return false;
    }

    const int step = 8;
    const int n = paths.size();
    offR = QVector<int>(n, 0);
    offC = QVector<int>(n, 0);

    QVector<QImage> previews(n);
    log_->appendLog(QStringLiteral("加载预览（抽稀×%1）…").arg(step));
    QApplication::processEvents();
    for (int i = 0; i < n; ++i) {
        int fw = 0, fh = 0;
        GrayImage gp;
        if (!loadMosaicGrayPreview(ss(paths[i]), step, gp, fw, fh, &errStd)) {
            QMessageBox::warning(this, QStringLiteral("手动拼接"), qs(errStd));
            return false;
        }
        previews[i] = grayToQImage(gp);
        log_->appendLog(QStringLiteral("  [%1] %2 (%3×%4)")
                            .arg(i + 1)
                            .arg(QFileInfo(paths[i]).fileName())
                            .arg(fw)
                            .arg(fh));
        QApplication::processEvents();
    }

    for (int i = 1; i < n; ++i) {
        ManualAlignDialog dlg(previews[i - 1], previews[i], step, i, n - 1, this);
        if (dlg.exec() != QDialog::Accepted) {
            log_->appendLog(QStringLiteral("用户取消手动对齐"));
            return false;
        }
        offR[i] = offR[i - 1] + dlg.dRowFull();
        offC[i] = offC[i - 1] + dlg.dColFull();
        log_->appendLog(QStringLiteral("第%1→%2张: Δ行=%3 Δ列=%4")
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
            log_->appendLog(QStringLiteral("剩余对按名义步进自动摆位"));
            break;
        }
    }
    return true;
}

void MosaicPage::onStart()
{
    if (inEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("拼接"), QStringLiteral("请选择输入文件夹"));
        return;
    }
    refreshDefaultOut();

    MosaicParams p;
    p.inFolder = inEdit_->text().trimmed();
    p.outFolder = outDir_->text();
    p.doFlatten = flattenCheck_->isChecked();
    p.medOut = medSpin_->value();
    p.mode = modeCombo_->currentData().toInt();
    p.rawNrHint = rawNrSpin_->value();
    p.rawNaHint = rawNaSpin_->value();

    log_->reset();

    if (p.mode == 1) {
        setBusy(true);
        cancelBtn_->setEnabled(false); // 对话框阶段不可后台取消
        if (!collectManualOffsets(p.inFolder, p.outFolder, p.offR, p.offC, p.rawNrHint, p.rawNaHint)) {
            setBusy(false);
            return;
        }
        cancelBtn_->setEnabled(true);
        log_->appendLog(QStringLiteral("手动摆位完成，开始融合写出…"));
    }

    worker_->setParams(p);
    setBusy(true);
    QMetaObject::invokeMethod(worker_, "startProcess", Qt::QueuedConnection);
}

void MosaicPage::onCancel()
{
    log_->appendLog(QStringLiteral("正在取消…"));
    worker_->requestCancel();
    if (auto *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
        dlg->reject();
}

void MosaicPage::onFlatten(const QString &path, double medOut)
{
    if (path.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("匀光"), QStringLiteral("请选择TIFF"));
        return;
    }
    setBusy(true);
    QMetaObject::invokeMethod(worker_, "startFlattenOnly", Qt::QueuedConnection,
                              Q_ARG(QString, path), Q_ARG(double, medOut));
}

void MosaicPage::onReady(const MosaicResult &result)
{
    log_->appendLog(QStringLiteral("MOSAIC: %1").arg(result.mosaicTif));
    if (!result.displayTif.isEmpty())
        log_->appendLog(QStringLiteral("显示版: %1").arg(result.displayTif));
    if (!result.flattenTif.isEmpty()) {
        log_->appendLog(QStringLiteral("匀光: %1").arg(result.flattenTif));
        flattenPanel_->setTifPath(result.mosaicTif);
    }
    if (!result.offsetsCsv.isEmpty())
        log_->appendLog(QStringLiteral("偏移表: %1").arg(result.offsetsCsv));

    QStringList lines;
    lines << QStringLiteral("拼接完成");
    if (!result.mosaicTif.isEmpty())
        lines << QStringLiteral("MOSAIC: %1").arg(QDir::toNativeSeparators(result.mosaicTif));
    if (!result.displayTif.isEmpty())
        lines << QStringLiteral("显示版: %1").arg(QDir::toNativeSeparators(result.displayTif));
    if (!result.flattenTif.isEmpty())
        lines << QStringLiteral("匀光: %1").arg(QDir::toNativeSeparators(result.flattenTif));
    if (!result.offsetsCsv.isEmpty())
        lines << QStringLiteral("偏移表: %1").arg(QDir::toNativeSeparators(result.offsetsCsv));
    resultLabel_->setText(lines.join(QStringLiteral("\n")));
}
