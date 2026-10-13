#include "ToZeroPage.h"
#include "common/OutDirField.h"
#include "common/OutPath.h"
#include "ui/LogProgressPanel.h"

#include <QDir>
#include <QFileInfo>

#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaType>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QSpinBox *makeSpin(QWidget *parent, int value, int minValue, int maxValue)
{
    auto *box = new QSpinBox(parent);
    box->setRange(minValue, maxValue);
    box->setValue(value);
    box->setMinimumHeight(28);
    return box;
}

} // namespace

ToZeroPage::ToZeroPage(QWidget *parent)
    : QWidget(parent)
    , srcEdit_(0)
    , outDir_(0)
    , baseNameEdit_(0)
    , engineEdit_(0)
    , mcrEdit_(0)
    , beginSpin_(0)
    , deltaSpin_(0)
    , lastSpin_(0)
    , numSpin_(0)
    , offsetSpin_(0)
    , testSpin_(0)
    , aperSpin_(0)
    , tarYSpin_(0)
    , iq1Spin_(0)
    , iq2Spin_(0)
    , rangeSpin_(0)
    , errStartSpin_(0)
    , errLenSpin_(0)
    , timeoutSpin_(0)
    , engineCheck_(0)
    , startBtn_(0)
    , cancelBtn_(0)
    , log_(0)
    , resultLabel_(0)
    , thread_(0)
    , worker_(0)
{
    qRegisterMetaType<ToZeroResultQt>("ToZeroResultQt");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *pathBox = new QGroupBox(QStringLiteral("数据路径"), this);
    auto *pathLay = new QVBoxLayout(pathBox);
    pathLay->setSpacing(6);

    srcEdit_ = new QLineEdit(pathBox);
    srcEdit_->setMinimumHeight(28);
    srcEdit_->setPlaceholderText(QStringLiteral("源 DAT，先转为四极化 L0 再成像"));
    auto *srcBrowse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    srcBrowse->setFixedWidth(72);
    auto *srcRow = new QHBoxLayout;
    srcRow->addWidget(new QLabel(QStringLiteral("源 DAT"), pathBox));
    srcRow->addWidget(srcEdit_, 1);
    srcRow->addWidget(srcBrowse);
    pathLay->addLayout(srcRow);

    outDir_ = new OutDirField(this);
    pathLay->addLayout(outDir_->createRow(pathBox));
    outDir_->setPlaceholderHint(QStringLiteral("源文件目录"), toZeroResultTag());

    baseNameEdit_ = new QLineEdit(pathBox);
    baseNameEdit_->setMinimumHeight(28);
    baseNameEdit_->setPlaceholderText(QStringLiteral("L0 输出基名，如 source_L0.dat"));
    auto *baseRow = new QHBoxLayout;
    baseRow->addWidget(new QLabel(QStringLiteral("L0 基名"), pathBox));
    baseRow->addWidget(baseNameEdit_, 1);
    pathLay->addLayout(baseRow);

    engineEdit_ = new QLineEdit(pathBox);
    engineEdit_->setMinimumHeight(28);
    engineEdit_->setPlaceholderText(QStringLiteral("留空则使用随包 engine/for_testing"));
    auto *engBrowse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    engBrowse->setFixedWidth(72);
    auto *engRow = new QHBoxLayout;
    engRow->addWidget(new QLabel(QStringLiteral("引擎目录"), pathBox));
    engRow->addWidget(engineEdit_, 1);
    engRow->addWidget(engBrowse);
    pathLay->addLayout(engRow);

    mcrEdit_ = new QLineEdit(pathBox);
    mcrEdit_->setMinimumHeight(28);
    mcrEdit_->setText(QStringLiteral("/usr/local/MATLAB/MATLAB_Runtime/v93"));
    auto *mcrRow = new QHBoxLayout;
    mcrRow->addWidget(new QLabel(QStringLiteral("MCR 目录"), pathBox));
    mcrRow->addWidget(mcrEdit_, 1);
    pathLay->addLayout(mcrRow);
    root->addWidget(pathBox);

    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);

    auto *left = new QWidget(split);
    left->setMinimumWidth(480);
    left->setStyleSheet(QStringLiteral(
        "QComboBox,QSpinBox,QDoubleSpinBox,QLineEdit,QPushButton,QCheckBox{min-height:30px;}"
        "QGroupBox{font-weight:600; padding-top:8px;}"));
    left->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(10, 4, 10, 4);
    leftLay->setSpacing(10);

    auto *paramBox = new QGroupBox(QStringLiteral("成像参数（须与数据编号匹配，文件个数至少 2）"), left);
    auto *form = new QFormLayout(paramBox);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    form->setContentsMargins(8, 10, 8, 8);

    beginSpin_ = makeSpin(paramBox, 30, 0, 1000000);
    deltaSpin_ = makeSpin(paramBox, 120, 0, 1000000);
    lastSpin_ = makeSpin(paramBox, 6000, 0, 1000000);
    numSpin_ = makeSpin(paramBox, 16, 2, 1000000);
    offsetSpin_ = makeSpin(paramBox, 100, -1000000, 1000000);
    testSpin_ = makeSpin(paramBox, 0, 0, 100);
    aperSpin_ = makeSpin(paramBox, 256, 1, 1000000);
    tarYSpin_ = makeSpin(paramBox, 1711, 0, 10000000);
    iq1Spin_ = makeSpin(paramBox, 1, 0, 10);
    iq2Spin_ = makeSpin(paramBox, 1, 0, 10);
    rangeSpin_ = makeSpin(paramBox, 8192, 1, 10000000);
    errStartSpin_ = makeSpin(paramBox, 500, 0, 10000000);
    errLenSpin_ = makeSpin(paramBox, 500, 0, 10000000);
    timeoutSpin_ = makeSpin(paramBox, 0, 0, 86400);
    timeoutSpin_->setSpecialValueText(QStringLiteral("不限制"));

    form->addRow(QStringLiteral("起始文件编号"), beginSpin_);
    form->addRow(QStringLiteral("间隔文件数目"), deltaSpin_);
    form->addRow(QStringLiteral("最后文件编号"), lastSpin_);
    form->addRow(QStringLiteral("文件个数"), numSpin_);
    form->addRow(QStringLiteral("距离偏移"), offsetSpin_);
    form->addRow(QStringLiteral("测试模式"), testSpin_);
    form->addRow(QStringLiteral("误差估计点数"), aperSpin_);
    form->addRow(QStringLiteral("目标位置 y"), tarYSpin_);
    form->addRow(QStringLiteral("波段1 IQ 顺序"), iq1Spin_);
    form->addRow(QStringLiteral("波段2 IQ 顺序"), iq2Spin_);
    form->addRow(QStringLiteral("距离向点数"), rangeSpin_);
    form->addRow(QStringLiteral("误差估计开始点"), errStartSpin_);
    form->addRow(QStringLiteral("误差估计长度"), errLenSpin_);
    form->addRow(QStringLiteral("超时（秒）"), timeoutSpin_);
    engineCheck_ = new QCheckBox(QStringLiteral("调用后处理引擎（取消则只还原）"), paramBox);
    engineCheck_->setChecked(true);
    form->addRow(QStringLiteral("引擎"), engineCheck_);
    leftLay->addWidget(paramBox);

    auto *btns = new QHBoxLayout;
    startBtn_ = new QPushButton(QStringLiteral("转换并成像"), left);
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
        QStringLiteral("一次完成两步：源 DAT → HH/VV/HV/VH 与 .sarrec，再对该目录成像。\n"
                       "图像写回 L0 所在目录。目标机需安装 MATLAB Runtime v9.3。"),
        right);
    resultLabel_->setWordWrap(true);
    resultLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    resultLabel_->setStyleSheet(QStringLiteral("color:#444; padding:4px;"));
    resultLabel_->setMinimumHeight(120);
    rightLay->addWidget(resultLabel_, 1);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes(QList<int>() << 560 << 1000);
    QTimer::singleShot(0, split, [split]() {
        split->setSizes(QList<int>() << 560 << qMax(800, split->width() - 560));
    });
    root->addWidget(split, 1);

    thread_ = new QThread(this);
    worker_ = new ToZeroWorker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &ToZeroWorker::progress, log_, &LogProgressPanel::setProgress);
    connect(worker_, &ToZeroWorker::message, log_, &LogProgressPanel::appendLog);
    connect(worker_, &ToZeroWorker::errorOccurred, this, [this](const QString &e) {
        log_->appendLog(QStringLiteral("[错误] ") + e);
        QMessageBox::warning(this, QStringLiteral("零级成像"), e);
    });
    connect(worker_, &ToZeroWorker::toZeroReady, this, &ToZeroPage::onReady);
    connect(worker_, &ToZeroWorker::finishedOk, this, [this]() { setBusy(false); });
    connect(worker_, &ToZeroWorker::finishedCancelled, this, [this]() {
        setBusy(false);
        log_->appendLog(QStringLiteral("任务已取消"));
    });
    thread_->start();

    connect(srcBrowse, &QPushButton::clicked, this, &ToZeroPage::onBrowseSource);
    connect(srcEdit_, &QLineEdit::editingFinished, this, &ToZeroPage::refreshDefaultOut);
    connect(engBrowse, &QPushButton::clicked, this, &ToZeroPage::onBrowseEngine);
    connect(startBtn_, &QPushButton::clicked, this, &ToZeroPage::onStart);
    connect(cancelBtn_, &QPushButton::clicked, this, &ToZeroPage::onCancel);
}

ToZeroPage::~ToZeroPage()
{
    if (worker_)
        worker_->requestCancel();
    if (thread_) {
        thread_->quit();
        thread_->wait(3000);
    }
}

void ToZeroPage::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

int ToZeroPage::spinValue(QSpinBox *box) const
{
    return box ? box->value() : 0;
}

void ToZeroPage::onBrowseSource()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择源 DAT"), srcEdit_->text(),
                                                   QStringLiteral("DAT (*.dat);;所有文件 (*)"));
    if (f.isEmpty())
        return;
    srcEdit_->setText(f);
    refreshDefaultOut();
}

void ToZeroPage::refreshDefaultOut()
{
    const QString src = srcEdit_->text().trimmed();
    outDir_->resetFromBase(baseDirOfInput(src), toZeroResultTag());
    if (baseNameEdit_->text().trimmed().isEmpty() && !src.isEmpty()) {
        QFileInfo fi(src);
        QString stem = fi.completeBaseName();
        if (stem.isEmpty())
            stem = QStringLiteral("source");
        baseNameEdit_->setText(stem + QStringLiteral("_L0.dat"));
    }
}

QString ToZeroPage::buildOutputBasePath() const
{
    const QString dir = outDir_->text().trimmed();
    QString name = baseNameEdit_->text().trimmed();
    if (name.isEmpty())
        name = QStringLiteral("source_L0.dat");
    if (dir.isEmpty())
        return name;
    return QDir(dir).filePath(name);
}

void ToZeroPage::onBrowseEngine()
{
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择引擎目录"),
                                                        engineEdit_->text());
    if (!d.isEmpty())
        engineEdit_->setText(d);
}

void ToZeroPage::onStart()
{
    if (srcEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("零级成像"), QStringLiteral("请先选择源 DAT"));
        return;
    }
    refreshDefaultOut();
    const QString outBase = buildOutputBasePath();
    if (outBase.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("零级成像"), QStringLiteral("请指定 L0 输出基路径"));
        return;
    }

    ToZeroParams p;
    p.sourcePath = srcEdit_->text().trimmed();
    p.outputBasePath = outBase;
    p.beginId = spinValue(beginSpin_);
    p.deltaNum = spinValue(deltaSpin_);
    p.lastFileId = spinValue(lastSpin_);
    p.fileNum = spinValue(numSpin_);
    p.rangeOffset = spinValue(offsetSpin_);
    p.testMode = spinValue(testSpin_);
    p.aperLen = spinValue(aperSpin_);
    p.tarPosY = spinValue(tarYSpin_);
    p.iqOrderBand1 = spinValue(iq1Spin_);
    p.iqOrderBand2 = spinValue(iq2Spin_);
    p.rangePoints = spinValue(rangeSpin_);
    p.errEstStart = spinValue(errStartSpin_);
    p.errEstLength = spinValue(errLenSpin_);
    p.engineDirectory = engineEdit_->text().trimmed();
    p.mcrRoot = mcrEdit_->text().trimmed();
    p.timeoutSec = spinValue(timeoutSpin_);
    p.requireEngine = engineCheck_->isChecked();

    setBusy(true);
    log_->reset();
    log_->appendLog(QStringLiteral("源 DAT: ") + p.sourcePath);
    log_->appendLog(QStringLiteral("L0 基路径: ") + p.outputBasePath);
    log_->appendLog(QStringLiteral("起始=%1 间隔=%2 最后=%3 个数=%4")
                        .arg(p.beginId)
                        .arg(p.deltaNum)
                        .arg(p.lastFileId)
                        .arg(p.fileNum));
    worker_->setParams(p);
    QMetaObject::invokeMethod(worker_, "startProcess", Qt::QueuedConnection);
}

void ToZeroPage::onCancel()
{
    if (worker_)
        worker_->requestCancel();
}

void ToZeroPage::onReady(const ToZeroResultQt &r)
{
    QString text = r.summary + QStringLiteral("\n\n");
    for (int i = 0; i < r.lines.size(); ++i)
        text += r.lines[i] + QStringLiteral("\n");
    if (!r.recordPath.isEmpty())
        text += QStringLiteral("\n记录: %1\n").arg(r.recordPath);
    for (int i = 0; i < r.warnings.size(); ++i)
        text += QStringLiteral("提示: %1\n").arg(r.warnings[i]);
    resultLabel_->setText(text);
    log_->appendLog(QStringLiteral("成像完成"));
}
