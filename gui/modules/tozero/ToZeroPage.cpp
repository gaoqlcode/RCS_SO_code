#include "ToZeroPage.h"
#include "common/OutDirField.h"
#include "common/OutPath.h"
#include "ui/LogProgressPanel.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaType>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

ToZeroPage::ToZeroPage(QWidget *parent)
    : QWidget(parent)
    , srcEdit_(0)
    , outDir_(0)
    , baseNameEdit_(0)
    , saveRecCheck_(0)
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
    srcEdit_->setPlaceholderText(QStringLiteral("源 DAT 文件"));
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
    baseNameEdit_->setPlaceholderText(QStringLiteral("输出基名，如 source_L0.dat"));
    auto *baseRow = new QHBoxLayout;
    baseRow->addWidget(new QLabel(QStringLiteral("输出基名"), pathBox));
    baseRow->addWidget(baseNameEdit_, 1);
    pathLay->addLayout(baseRow);

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

    auto *paramBox = new QGroupBox(QStringLiteral("处理参数"), left);
    auto *form = new QFormLayout(paramBox);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    form->setContentsMargins(8, 10, 8, 8);
    saveRecCheck_ = new QCheckBox(QStringLiteral("同时写 .sarrec 还原信息"), paramBox);
    form->addRow(QStringLiteral("还原信息"), saveRecCheck_);
    leftLay->addWidget(paramBox);

    auto *btns = new QHBoxLayout;
    startBtn_ = new QPushButton(QStringLiteral("开始转换"), left);
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
        QStringLiteral("将源 DAT 转为 HH/VV/HV/VH 四个 L0 DAT；可选写 .sarrec。\n"
                       "输出基路径 = 输出目录 / 输出基名。"),
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
        QMessageBox::warning(this, QStringLiteral("ToZeroData"), e);
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

void ToZeroPage::refreshDefaultOut()
{
    const QString src = srcEdit_->text().trimmed();
    const QString base = baseDirOfInput(src);
    outDir_->resetFromBase(base, toZeroResultTag());
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

void ToZeroPage::onBrowseSource()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择源 DAT"),
                                                   srcEdit_->text(),
                                                   QStringLiteral("DAT (*.dat);;所有文件 (*)"));
    if (f.isEmpty())
        return;
    srcEdit_->setText(f);
    refreshDefaultOut();
}

void ToZeroPage::onStart()
{
    if (srcEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ToZeroData"), QStringLiteral("请先选择源 DAT"));
        return;
    }
    refreshDefaultOut();
    const QString outBase = buildOutputBasePath();
    if (outBase.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ToZeroData"), QStringLiteral("请指定输出基路径"));
        return;
    }

    ToZeroParams p;
    p.sourcePath = srcEdit_->text().trimmed();
    p.outputBasePath = outBase;
    p.saveRecovery = saveRecCheck_->isChecked();

    setBusy(true);
    log_->reset();
    log_->appendLog(QStringLiteral("源: ") + p.sourcePath);
    log_->appendLog(QStringLiteral("输出基路径: ") + p.outputBasePath);
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
    if (!r.recoveryPath.isEmpty()) {
        text += QStringLiteral("\n还原: %1\nSHA-256: %2\n").arg(r.recoveryPath, r.sourceSha256);
    }
    resultLabel_->setText(text);
    log_->appendLog(QStringLiteral("转换完成"));
}
