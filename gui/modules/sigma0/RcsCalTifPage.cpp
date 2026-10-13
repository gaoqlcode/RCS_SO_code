#include "RcsCalTifPage.h"
#include "common/OutPath.h"
#include "common/OutDirField.h"
#include "ui/LogProgressPanel.h"
#include "ui/RectPickDialog.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
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
#include <QVBoxLayout>

RcsCalTifPage::RcsCalTifPage(QWidget *parent)
    : QWidget(parent)
    , calibEdit_(0)
    , folderEdit_(0)
    , outDir_(0)
    , nameEdit_(0)
    , bandCombo_(0)
    , polCombo_(0)
    , sigmaSpin_(0)
    , fsSpin_(0)
    , incSpin_(0)
    , hSpin_(0)
    , maxFilesSpin_(0)
    , startBtn_(0)
    , cancelBtn_(0)
    , log_(0)
    , resultLabel_(0)
    , thread_(0)
    , worker_(0)
{
    qRegisterMetaType<RcsCalTifResultQt>("RcsCalTifResultQt");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *pathBox = new QGroupBox(QStringLiteral("数据路径"), this);
    auto *pathLay = new QVBoxLayout(pathBox);
    pathLay->setSpacing(6);

    calibEdit_ = new QLineEdit(pathBox);
    calibEdit_->setMinimumHeight(28);
    calibEdit_->setPlaceholderText(QStringLiteral("含角反的定标 RAW（Image_Nr*Na*.raw）"));
    auto *calibBrowse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    calibBrowse->setFixedWidth(72);
    auto *calibRow = new QHBoxLayout;
    calibRow->addWidget(new QLabel(QStringLiteral("定标 RAW"), pathBox));
    calibRow->addWidget(calibEdit_, 1);
    calibRow->addWidget(calibBrowse);
    pathLay->addLayout(calibRow);

    folderEdit_ = new QLineEdit(pathBox);
    folderEdit_->setMinimumHeight(28);
    folderEdit_->setPlaceholderText(QStringLiteral("待批量定标的 RAW 文件夹"));
    auto *folderBrowse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    folderBrowse->setFixedWidth(72);
    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(new QLabel(QStringLiteral("RAW 目录"), pathBox));
    folderRow->addWidget(folderEdit_, 1);
    folderRow->addWidget(folderBrowse);
    pathLay->addLayout(folderRow);

    outDir_ = new OutDirField(this);
    pathLay->addLayout(outDir_->createRow(pathBox));
    outDir_->setPlaceholderHint(QStringLiteral("RAW目录"), rcsCalTifResultTag());
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

    nameEdit_ = new QLineEdit(QStringLiteral("林地"), paramBox);
    form->addRow(QStringLiteral("目标名称"), nameEdit_);

    bandCombo_ = new QComboBox(paramBox);
    bandCombo_->addItems(QStringList() << QStringLiteral("X") << QStringLiteral("Ku")
                                       << QStringLiteral("Ka") << QStringLiteral("W"));
    form->addRow(QStringLiteral("波段"), bandCombo_);

    polCombo_ = new QComboBox(paramBox);
    polCombo_->addItems(QStringList() << QStringLiteral("HH") << QStringLiteral("HV")
                                      << QStringLiteral("VH") << QStringLiteral("VV")
                                      << QStringLiteral("(不筛)"));
    form->addRow(QStringLiteral("极化"), polCombo_);

    sigmaSpin_ = new QDoubleSpinBox(paramBox);
    sigmaSpin_->setRange(-50.0, 80.0);
    sigmaSpin_->setDecimals(3);
    sigmaSpin_->setValue(18.25);
    sigmaSpin_->setSuffix(QStringLiteral(" dBsm"));
    form->addRow(QStringLiteral("角反 RCS"), sigmaSpin_);

    fsSpin_ = new QDoubleSpinBox(paramBox);
    fsSpin_->setRange(1e6, 1e10);
    fsSpin_->setDecimals(0);
    fsSpin_->setValue(1.25e9);
    fsSpin_->setSuffix(QStringLiteral(" Hz"));
    form->addRow(QStringLiteral("采样率 Fs"), fsSpin_);

    incSpin_ = new QDoubleSpinBox(paramBox);
    incSpin_->setRange(1.0, 89.0);
    incSpin_->setDecimals(1);
    incSpin_->setValue(70.0);
    incSpin_->setSuffix(QStringLiteral(" °"));
    form->addRow(QStringLiteral("入射角"), incSpin_);

    hSpin_ = new QDoubleSpinBox(paramBox);
    hSpin_->setRange(1.0, 50000.0);
    hSpin_->setDecimals(1);
    hSpin_->setValue(500.0);
    hSpin_->setSuffix(QStringLiteral(" m"));
    form->addRow(QStringLiteral("飞行高度"), hSpin_);

    maxFilesSpin_ = new QSpinBox(paramBox);
    maxFilesSpin_->setRange(0, 100000);
    maxFilesSpin_->setSpecialValueText(QStringLiteral("全部"));
    maxFilesSpin_->setValue(0);
    form->addRow(QStringLiteral("最多文件数"), maxFilesSpin_);

    leftLay->addWidget(paramBox);

    auto *btnRow = new QHBoxLayout;
    startBtn_ = new QPushButton(QStringLiteral("开始定标"), left);
    cancelBtn_ = new QPushButton(QStringLiteral("取消"), left);
    cancelBtn_->setEnabled(false);
    btnRow->addWidget(startBtn_);
    btnRow->addWidget(cancelBtn_);
    btnRow->addStretch(1);
    leftLay->addLayout(btnRow);

    log_ = new LogProgressPanel(left);
    leftLay->addWidget(log_, 1);

    auto *right = new QWidget(split);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(8, 4, 8, 4);
    auto *resBox = new QGroupBox(QStringLiteral("结果"), right);
    auto *resLay = new QVBoxLayout(resBox);
    resultLabel_ = new QLabel(QStringLiteral("尚未运行"), resBox);
    resultLabel_->setWordWrap(true);
    resultLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resLay->addWidget(resultLabel_);
    resLay->addStretch(1);
    rightLay->addWidget(resBox, 1);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes(QList<int>() << 560 << 800);
    root->addWidget(split, 1);

    thread_ = new QThread(this);
    worker_ = new RcsCalTifWorker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &RcsCalTifWorker::progress, log_, &LogProgressPanel::setProgress);
    connect(worker_, &RcsCalTifWorker::message, log_, &LogProgressPanel::appendLog);
    connect(worker_, &RcsCalTifWorker::errorOccurred, this, [this](const QString &e) {
        log_->appendLog(QStringLiteral("[错误] ") + e);
        QMessageBox::warning(this, QStringLiteral("RCS像辐射定标"), e);
    });
    connect(worker_, &RcsCalTifWorker::needRectRoi, this, &RcsCalTifPage::onNeedRectRoi,
            Qt::QueuedConnection);
    connect(worker_, &RcsCalTifWorker::calReady, this, &RcsCalTifPage::onReady);
    connect(worker_, &RcsCalTifWorker::finishedOk, this, [this]() { setBusy(false); });
    connect(worker_, &RcsCalTifWorker::finishedCancelled, this, [this]() {
        setBusy(false);
        log_->appendLog(QStringLiteral("任务已取消"));
    });
    thread_->start();

    connect(calibBrowse, &QPushButton::clicked, this, &RcsCalTifPage::onBrowseCalib);
    connect(folderBrowse, &QPushButton::clicked, this, &RcsCalTifPage::onBrowseFolder);
    connect(startBtn_, &QPushButton::clicked, this, &RcsCalTifPage::onStart);
    connect(cancelBtn_, &QPushButton::clicked, this, &RcsCalTifPage::onCancel);

    auto refreshOut = [this]() {
        QString base = folderEdit_->text().trimmed();
        if (base.isEmpty())
            base = baseDirOfInput(calibEdit_->text());
        outDir_->resetFromBase(base, rcsCalTifResultTag());
    };
    connect(calibEdit_, &QLineEdit::editingFinished, this, refreshOut);
    connect(folderEdit_, &QLineEdit::editingFinished, this, refreshOut);
}

RcsCalTifPage::~RcsCalTifPage()
{
    if (worker_)
        worker_->requestCancel();
    if (thread_) {
        thread_->quit();
        thread_->wait(3000);
    }
}

void RcsCalTifPage::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void RcsCalTifPage::onBrowseCalib()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择定标 RAW"),
                                                   calibEdit_->text(),
                                                   QStringLiteral("RAW (*.raw);;所有文件 (*)"));
    if (f.isEmpty())
        return;
    calibEdit_->setText(f);
    emit calibEdit_->editingFinished();
}

void RcsCalTifPage::onBrowseFolder()
{
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择 RAW 目录"),
                                                        folderEdit_->text());
    if (d.isEmpty())
        return;
    folderEdit_->setText(d);
    emit folderEdit_->editingFinished();
}

void RcsCalTifPage::onStart()
{
    if (calibEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("RCS像辐射定标"),
                             QStringLiteral("请先选择定标 RAW"));
        return;
    }
    if (folderEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("RCS像辐射定标"),
                             QStringLiteral("请先选择待定标 RAW 目录"));
        return;
    }

    QString base = folderEdit_->text().trimmed();
    outDir_->refresh(base, rcsCalTifResultTag());

    RcsCalTifParams p;
    p.calibRaw = calibEdit_->text().trimmed();
    p.rawFolder = folderEdit_->text().trimmed();
    p.outDir = outDir_->text();
    p.tgtName = nameEdit_->text().trimmed().isEmpty() ? QStringLiteral("目标")
                                                      : nameEdit_->text().trimmed();
    p.band = bandCombo_->currentText();
    p.pol = (polCombo_->currentIndex() == polCombo_->count() - 1) ? QString()
                                                                  : polCombo_->currentText();
    p.sigmaTheoryDb = sigmaSpin_->value();
    p.fsHz = fsSpin_->value();
    p.incAngleDeg = incSpin_->value();
    p.hFlight = hSpin_->value();
    p.maxFiles = maxFilesSpin_->value();

    worker_->setParams(p);
    setBusy(true);
    log_->appendLog(QStringLiteral("提交任务…"));
    resultLabel_->setText(QStringLiteral("处理中…"));
    QMetaObject::invokeMethod(worker_, "startProcess", Qt::QueuedConnection);
}

void RcsCalTifPage::onCancel()
{
    if (worker_)
        worker_->requestCancel();
}

void RcsCalTifPage::onNeedRectRoi(const QImage &preview, int fullNr, int fullNa,
                                  const QString &stage)
{
    Q_UNUSED(stage);
    RectPickDialog dlg(preview, fullNr, fullNa, QStringLiteral("框选角反缓冲区（必选）"), this);
    UserSelection sel;
    if (dlg.exec() == QDialog::Accepted && dlg.acceptedOk()) {
        const RectRoi r = dlg.roi();
        sel.rectRois.push_back(QRect(r.x, r.y, r.w, r.h));
    }
    worker_->provideSelection(sel);
}

void RcsCalTifPage::onReady(const RcsCalTifResultQt &r)
{
    resultLabel_->setText(r.summary);
    log_->appendLog(QStringLiteral("完成：") + r.summary.split(QLatin1Char('\n')).first());
}
