#include "Sigma0Page.h"
#include "common/OutPath.h"
#include "common/OutDirField.h"
#include "ui/LogProgressPanel.h"
#include "ui/RectPickDialog.h"
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QMetaType>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSplitter>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

Sigma0Page::Sigma0Page(QWidget *parent)
    : QWidget(parent)
    , rawEdit_(0)
    , outDir_(0)
    , nameEdit_(0)
    , timeEdit_(0)
    , nrSpin_(0)
    , naSpin_(0)
    , fcSpin_(0)
    , vsSpin_(0)
    , incSpin_(0)
    , hSpin_(0)
    , sigmaSpin_(0)
    , tgtTypeCombo_(0)
    , bgModeCombo_(0)
    , startBtn_(0)
    , cancelBtn_(0)
    , log_(0)
    , resultLabel_(0)
    , thread_(0)
    , worker_(0)
{
    qRegisterMetaType<Sigma0ResultQt>("Sigma0ResultQt");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // 旧版模板：顶栏路径全宽 + 左右分栏（窄左参数/日志，宽右结果）
    auto *pathBox = new QGroupBox(QStringLiteral("数据路径"), this);
    auto *pathLay = new QVBoxLayout(pathBox);
    pathLay->setSpacing(6);
    rawEdit_ = new QLineEdit(pathBox);
    rawEdit_->setMinimumHeight(28);
    rawEdit_->setPlaceholderText(QStringLiteral("已成像 uint16 LE .raw（如 Image_Nr*_Na*.raw）"));
    auto *rawBrowse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    rawBrowse->setFixedWidth(72);
    auto *rawRow = new QHBoxLayout;
    rawRow->addWidget(new QLabel(QStringLiteral("RAW 文件"), pathBox));
    rawRow->addWidget(rawEdit_, 1);
    rawRow->addWidget(rawBrowse);
    pathLay->addLayout(rawRow);
    outDir_ = new OutDirField(this);
    pathLay->addLayout(outDir_->createRow(pathBox));
    outDir_->setPlaceholderHint(QStringLiteral("RAW同目录"), sigma0ResultTag());
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
    form->addRow(QStringLiteral("目标名称"), nameEdit_);
    timeEdit_ = new QLineEdit(QStringLiteral("2026-07-17 15:27:00.000"), paramBox);
    form->addRow(QStringLiteral("测量时刻"), timeEdit_);
    nrSpin_ = new QSpinBox(paramBox);
    nrSpin_->setRange(1, 200000);
    nrSpin_->setValue(2048);
    form->addRow(QStringLiteral("Nr（距离行）"), nrSpin_);
    naSpin_ = new QSpinBox(paramBox);
    naSpin_->setRange(1, 200000);
    naSpin_->setValue(4602);
    form->addRow(QStringLiteral("Na（方位列）"), naSpin_);
    fcSpin_ = new QDoubleSpinBox(paramBox);
    fcSpin_->setRange(1, 200);
    fcSpin_->setDecimals(3);
    fcSpin_->setValue(35.0);
    fcSpin_->setSuffix(QStringLiteral(" GHz"));
    form->addRow(QStringLiteral("中心频率"), fcSpin_);
    vsSpin_ = new QDoubleSpinBox(paramBox);
    vsSpin_->setRange(0.1, 500);
    vsSpin_->setValue(10.0);
    vsSpin_->setSuffix(QStringLiteral(" m/s"));
    form->addRow(QStringLiteral("平台地速"), vsSpin_);
    incSpin_ = new QDoubleSpinBox(paramBox);
    incSpin_->setRange(1, 89);
    incSpin_->setValue(70.0);
    incSpin_->setSuffix(QStringLiteral(" °"));
    form->addRow(QStringLiteral("入射角"), incSpin_);
    hSpin_ = new QDoubleSpinBox(paramBox);
    hSpin_->setRange(1, 20000);
    hSpin_->setValue(500.0);
    hSpin_->setSuffix(QStringLiteral(" m"));
    form->addRow(QStringLiteral("飞行高度"), hSpin_);
    sigmaSpin_ = new QDoubleSpinBox(paramBox);
    sigmaSpin_->setRange(-50, 80);
    sigmaSpin_->setValue(18.25);
    sigmaSpin_->setSuffix(QStringLiteral(" dBsm"));
    form->addRow(QStringLiteral("角反理论 RCS"), sigmaSpin_);
    tgtTypeCombo_ = new QComboBox(paramBox);
    tgtTypeCombo_->addItem(QStringLiteral("区域目标（地物）"), 0);
    tgtTypeCombo_->addItem(QStringLiteral("点目标"), 1);
    form->addRow(QStringLiteral("目标类型"), tgtTypeCombo_);
    bgModeCombo_ = new QComboBox(paramBox);
    bgModeCombo_->addItem(QStringLiteral("不扣背景"), 0);
    bgModeCombo_->addItem(QStringLiteral("环形邻域"), 1);
    bgModeCombo_->addItem(QStringLiteral("单独框选背景"), 2);
    bgModeCombo_->setCurrentIndex(1);
    form->addRow(QStringLiteral("点目标背景"), bgModeCombo_);

    auto *paramScroll = new QScrollArea(left);
    paramScroll->setWidgetResizable(true);
    paramScroll->setFrameShape(QFrame::NoFrame);
    paramScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    paramScroll->setWidget(paramBox);
    leftLay->addWidget(paramScroll, 0);

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
    resultLabel_ = new QLabel(
        QStringLiteral(
            "结果将显示在这里。\n处理中会弹出框选窗口：噪声区（可选）→ 角反缓冲区 → 地物目标。"),
        right);
    resultLabel_->setWordWrap(true);
    resultLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    resultLabel_->setStyleSheet(QStringLiteral("color:#444; padding:4px;"));
    rightLay->addWidget(resultLabel_, 1);

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
    worker_ = new Sigma0Worker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &Sigma0Worker::progress, log_, &LogProgressPanel::setProgress);
    connect(worker_, &Sigma0Worker::message, log_, &LogProgressPanel::appendLog);
    connect(worker_, &Sigma0Worker::errorOccurred, this, [this](const QString &e) {
        log_->appendLog(QStringLiteral("[错误] ") + e);
        QMessageBox::warning(this, QStringLiteral("后向散射系数"), e);
    });
    connect(worker_, &Sigma0Worker::needRectRoi, this, &Sigma0Page::onNeedRectRoi, Qt::QueuedConnection);
    connect(worker_, &Sigma0Worker::needTargetRois, this, &Sigma0Page::onNeedTargetRois,
            Qt::QueuedConnection);
    connect(worker_, &Sigma0Worker::sigma0Ready, this, &Sigma0Page::onReady);
    connect(worker_, &Sigma0Worker::finishedOk, this, [this]() { setBusy(false); });
    connect(worker_, &Sigma0Worker::finishedCancelled, this, [this]() {
        setBusy(false);
        log_->appendLog(QStringLiteral("任务已取消"));
    });
    thread_->start();

    connect(rawBrowse, &QPushButton::clicked, this, &Sigma0Page::onBrowseRaw);
    connect(startBtn_, &QPushButton::clicked, this, &Sigma0Page::onStart);
    connect(cancelBtn_, &QPushButton::clicked, this, &Sigma0Page::onCancel);

    connect(rawEdit_, &QLineEdit::editingFinished, this, [this]() {
        const QString p = rawEdit_->text().trimmed();
        const QFileInfo fi(p);
        const QString base = fi.completeBaseName();
        // 尝试从文件名解析 Nr/Na：Image_Nr2048Na4602...
        const int iNr = base.indexOf(QStringLiteral("Nr"));
        const int iNa = base.indexOf(QStringLiteral("Na"));
        if (iNr >= 0 && iNa > iNr) {
            bool ok1 = false, ok2 = false;
            const int nr = base.mid(iNr + 2, iNa - iNr - 2).toInt(&ok1);
            int j = iNa + 2;
            while (j < base.size() && base[j].isDigit())
                ++j;
            const int na = base.mid(iNa + 2, j - iNa - 2).toInt(&ok2);
            if (ok1 && nr > 0)
                nrSpin_->setValue(nr);
            if (ok2 && na > 0)
                naSpin_->setValue(na);
        }
        outDir_->resetFromBase(baseDirOfInput(p), sigma0ResultTag());
    });
}

Sigma0Page::~Sigma0Page()
{
    if (worker_)
        worker_->requestCancel();
    if (thread_) {
        thread_->quit();
        thread_->wait(3000);
    }
}

void Sigma0Page::setBusy(bool busy)
{
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

void Sigma0Page::refreshDefaultOut()
{
    outDir_->refresh(baseDirOfInput(rawEdit_->text()), sigma0ResultTag());
}

void Sigma0Page::onBrowseRaw()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择成像 RAW"),
                                                   rawEdit_->text(),
                                                   QStringLiteral("RAW (*.raw);;所有文件 (*)"));
    if (f.isEmpty())
        return;
    rawEdit_->setText(f);
    emit rawEdit_->editingFinished();
}

void Sigma0Page::onStart()
{
    if (rawEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("后向散射系数"), QStringLiteral("请先选择 RAW 文件"));
        return;
    }
    refreshDefaultOut();
    Sigma0Params p;
    p.rawPath = rawEdit_->text().trimmed();
    p.outDir = outDir_->text();
    p.tgtName = nameEdit_->text().trimmed().isEmpty() ? QStringLiteral("目标") : nameEdit_->text().trimmed();
    p.nr = nrSpin_->value();
    p.na = naSpin_->value();
    p.fcGHz = fcSpin_->value();
    p.vs = vsSpin_->value();
    p.incAngleDeg = incSpin_->value();
    p.hFlight = hSpin_->value();
    p.sigmaTheoryDb = sigmaSpin_->value();
    p.targetType = tgtTypeCombo_->currentData().toInt();
    p.bgMode = bgModeCombo_->currentData().toInt();
    p.measureTime = timeEdit_->text().trimmed();
    worker_->setParams(p);
    setBusy(true);
    log_->appendLog(QStringLiteral("提交任务…"));
    resultLabel_->setText(QStringLiteral("处理中…"));
    QMetaObject::invokeMethod(worker_, "startProcess", Qt::QueuedConnection);
}

void Sigma0Page::onCancel()
{
    if (worker_)
        worker_->requestCancel();
}

bool Sigma0Page::pickOneRoi(const QImage &preview, int fullNr, int fullNa, const QString &title,
                            QRect *out)
{
    RectPickDialog dlg(preview, fullNr, fullNa, title, this);
    if (dlg.exec() != QDialog::Accepted || !dlg.acceptedOk())
        return false;
    const RectRoi r = dlg.roi();
    *out = QRect(r.x, r.y, r.w, r.h);
    return true;
}

void Sigma0Page::onNeedRectRoi(const QImage &preview, int fullNr, int fullNa, const QString &stage)
{
    QString title = QStringLiteral("框选区域");
    if (stage == QLatin1String("noise"))
        title = QStringLiteral("框选噪声区（可取消跳过）");
    else if (stage == QLatin1String("cal"))
        title = QStringLiteral("框选角反缓冲区（必选）");
    else if (stage == QLatin1String("bg"))
        title = QStringLiteral("框选背景区");

    UserSelection sel;
    QRect r;
    if (pickOneRoi(preview, fullNr, fullNa, title, &r))
        sel.rectRois.push_back(r);
    else if (stage == QLatin1String("noise")) {
        // 噪声可跳过：空 ROI，库侧走直方图估噪
        worker_->provideSelection(sel);
        return;
    }
    worker_->provideSelection(sel);
}

void Sigma0Page::onNeedTargetRois(const QImage &preview, int fullNr, int fullNa)
{
    UserSelection sel;
    for (;;) {
        QRect r;
        if (!pickOneRoi(preview, fullNr, fullNa,
                        QStringLiteral("框选地物目标 #%1").arg(sel.rectRois.size() + 1), &r))
            break;
        sel.rectRois.push_back(r);
        const auto ans = QMessageBox::question(this, QStringLiteral("后向散射系数"),
                                               QStringLiteral("是否继续添加目标？"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            break;
    }
    worker_->provideSelection(sel);
}

void Sigma0Page::onReady(const Sigma0ResultQt &r)
{
    QStringList lines;
    lines << QStringLiteral("K = %1").arg(r.K, 0, 'g', 8);
    lines << QStringLiteral(".cs = %1").arg(r.csPath);
    for (int i = 0; i < r.sigma0Db.size(); ++i)
        lines << QStringLiteral("目标%1 σ⁰ = %2 dB").arg(i + 1).arg(r.sigma0Db[i], 0, 'f', 2);
    resultLabel_->setText(lines.join(QLatin1Char('\n')));
    log_->appendLog(QStringLiteral("完成：") + r.csPath);
}
