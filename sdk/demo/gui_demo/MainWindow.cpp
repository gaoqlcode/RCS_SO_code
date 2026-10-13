// =============================================================================
// MainWindow.cpp — 界面搭建 + 线程接线 + 用户操作入口
// =============================================================================

#include "MainWindow.h"      // 本类
#include "ConfirmBridge.h"   // UI 线程确认桥
#include "SimplePlotWidget.h"// 折线图
#include "rcs/rcs_api.h"     // listAvailablePolarizations、rcsVersion

#include <QByteArray>        // toUtf8 辅助
#include <QComboBox>         // 极化下拉
#include <QFileDialog>       // 选目录对话框
#include <QFormLayout>       // 标签+控件表单行
#include <QGroupBox>         // 「数据」分组框
#include <QHBoxLayout>       // 水平布局
#include <QLabel>            // 文本标签
#include <QLineEdit>         // 单行路径编辑
#include <QMessageBox>       // 信息/警告框
#include <QMetaObject>       // invokeMethod 派活给 Worker
#include <QProgressBar>      // 进度条
#include <QPushButton>       // 按钮
#include <QSplitter>         // 左右可拖分割
#include <QTextEdit>         // 多行日志
#include <QVBoxLayout>       // 垂直布局

namespace {

// 界面路径 → SDK UTF-8（与 DemoWorker 内同名函数职责相同）
std::string toUtf8(const QString &s)
{
    const QByteArray u = s.toUtf8(); // UTF-8 字节
    return std::string(u.constData(), static_cast<size_t>(u.size())); // 按长度构造
}

} // namespace

// ======================== 构造：搭 UI + 起线程 ========================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) // 顶级主窗
    , confirmBridge_(0)   // 稍后 new
    , previewIndex_(0)    // 预览从第 0 条开始
{
    // 标题带上库版本，确认链到的是哪份 so/dll
    setWindowTitle(QStringLiteral("RCS SDK gui_demo (%1)").arg(QString::fromUtf8(rcsVersion())));

    // 默认客户区大小（可被用户拖拽改变）
    resize(1180, 780);

    // 中央部件：QMainWindow 必须 setCentralWidget 才能显示主体内容
    auto *central = new QWidget(this);

    // 根布局：上路径、中按钮、下分割区
    auto *root = new QVBoxLayout(central);

    // ---------- 路径分组 ----------
    auto *pathBox = new QGroupBox(QStringLiteral("数据"), central); // 带标题边框
    auto *form = new QFormLayout(pathBox);                         // 左标签右控件

    inEdit_ = new QLineEdit(pathBox); // 输入目录编辑框
    inEdit_->setPlaceholderText(QStringLiteral("放 *_L0_*.dat 的文件夹")); // 灰字提示
    auto *inBrowse = new QPushButton(QStringLiteral("浏览"), pathBox);     // 打开目录对话框
    auto *inRow = new QHBoxLayout; // 编辑框 + 浏览按钮同一行
    inRow->addWidget(inEdit_, 1);  // stretch=1：编辑框吃掉多余宽度
    inRow->addWidget(inBrowse);    // 按钮固定宽度
    form->addRow(QStringLiteral("输入"), inRow); // 表单一行

    outEdit_ = new QLineEdit(pathBox); // 输出目录
    outEdit_->setPlaceholderText(QStringLiteral("HRRP 输出目录，可先空着"));
    auto *outBrowse = new QPushButton(QStringLiteral("浏览"), pathBox);
    auto *outRow = new QHBoxLayout;
    outRow->addWidget(outEdit_, 1);
    outRow->addWidget(outBrowse);
    form->addRow(QStringLiteral("输出"), outRow);

    polCombo_ = new QComboBox(pathBox); // 极化下拉
    // 先塞四种常见极化；选目录后可用「扫一下极化」按实际文件覆盖
    polCombo_->addItems(QStringList() << "HH" << "HV" << "VV" << "VH");
    auto *refreshPol = new QPushButton(QStringLiteral("扫一下极化"), pathBox);
    auto *polRow = new QHBoxLayout;
    polRow->addWidget(polCombo_, 1);
    polRow->addWidget(refreshPol);
    form->addRow(QStringLiteral("极化"), polRow);
    root->addWidget(pathBox); // 路径区加入根布局

    // ---------- 业务按钮行 ----------
    auto *btns = new QHBoxLayout;
    probeBtn_ = new QPushButton(QStringLiteral("探查"), central);
    previewBtn_ = new QPushButton(QStringLiteral("预览"), central);
    hrrpBtn_ = new QPushButton(QStringLiteral("跑 HRRP"), central);
    cancelBtn_ = new QPushButton(QStringLiteral("取消"), central);
    cancelBtn_->setEnabled(false); // 空闲时取消无意义，先禁用
    btns->addWidget(probeBtn_);
    btns->addWidget(previewBtn_);
    btns->addWidget(hrrpBtn_);
    btns->addWidget(cancelBtn_);
    btns->addStretch(); // 右侧留白，按钮靠左
    root->addLayout(btns);

    // 使用提示（灰色说明）
    auto *hint = new QLabel(
        QStringLiteral("库在后台线程跑。HRRP 会弹窗确认峰距。曲线自己画。"),
        central);
    hint->setWordWrap(true); // 窄窗时换行
    hint->setStyleSheet(QStringLiteral("color:#666;")); // 次要信息样式
    root->addWidget(hint);

    // ---------- 左右分割：信息 | 图 ----------
    auto *split = new QSplitter(Qt::Horizontal, central); // 水平分割条可拖

    // 左栏：探查文字 + 日志 + 进度条
    auto *left = new QWidget(split);
    auto *leftLay = new QVBoxLayout(left);
    infoLabel_ = new QLabel(QStringLiteral("（探查结果）"), left);
    infoLabel_->setWordWrap(true);
    infoLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft); // 多行从上往下排
    leftLay->addWidget(infoLabel_, 1); // stretch=1：占掉日志上方空间
    log_ = new QTextEdit(left);
    log_->setReadOnly(true);           // 只追加日志，不让用户改
    log_->setMaximumHeight(200);       // 别把左栏全占满
    leftLay->addWidget(log_);
    bar_ = new QProgressBar(left);
    bar_->setRange(0, 100);            // 与 Callbacks::onProgress 约定一致
    leftLay->addWidget(bar_);

    // 右栏：文件导航 + 三张图
    auto *right = new QWidget(split);
    auto *rightLay = new QVBoxLayout(right);
    fileLabel_ = new QLabel(QStringLiteral("文件名"), right); // 显示「i/n  文件名」
    auto *nav = new QHBoxLayout;
    prevBtn_ = new QPushButton(QStringLiteral("上一个"), right);
    nextBtn_ = new QPushButton(QStringLiteral("下一个"), right);
    nav->addWidget(prevBtn_);
    nav->addWidget(nextBtn_);
    nav->addStretch();

    timePlot_ = new SimplePlotWidget(right); // 时域图
    timePlot_->setAxisTitles(QStringLiteral("时间 (μs)"), QStringLiteral("幅度"));
    freqPlot_ = new SimplePlotWidget(right); // 频域图
    freqPlot_->setAxisTitles(QStringLiteral("频率 (MHz)"), QStringLiteral("dB"));
    freqPlot_->setYRange(-60, 3); // 频谱常用固定动态范围
    hrrpPlot_ = new SimplePlotWidget(right); // HRRP 图
    hrrpPlot_->setAxisTitles(QStringLiteral("距离 (m)"), QStringLiteral("RCS (dBsm)"));
    hrrpPlot_->setTitle(QStringLiteral("HRRP"));

    rightLay->addWidget(fileLabel_);
    rightLay->addLayout(nav);
    rightLay->addWidget(timePlot_, 1); // 三张图均分垂直空间
    rightLay->addWidget(freqPlot_, 1);
    rightLay->addWidget(hrrpPlot_, 1);

    split->addWidget(left);  // 分割器左
    split->addWidget(right); // 分割器右
    split->setStretchFactor(0, 0); // 拉大窗口时优先拉右边图区
    split->setStretchFactor(1, 1);
    split->setSizes(QList<int>() << 320 << 860); // 初始像素宽
    root->addWidget(split, 1); // stretch=1：占满窗口剩余高度
    setCentralWidget(central); // 装上中央部件

    // ---------- 线程与 Worker ----------
    confirmBridge_ = new ConfirmBridge(this); // 父=this → 与 MainWindow 同线程（UI）

    thread_ = new QThread(this); // 线程对象本身可挂在 UI；真正跑槽的是 move 过去的 Worker
    worker_ = new DemoWorker;    // 故意无 parent，以便安全 moveToThread
    worker_->setConfirmBridge(confirmBridge_); // 注入弹窗桥指针
    worker_->moveToThread(thread_); // 此后 worker_ 的槽在 thread_ 里执行
    // 线程结束时排队删除 Worker，避免泄漏
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);

    // 进度 → 进度条 + 日志（跨线程 Auto→Queued）
    connect(worker_, &DemoWorker::progress, this, [this](int p, const QString &m) {
        bar_->setValue(p); // 更新百分比
        log_->append(m);   // 追加一行说明
    });

    // 一般消息直接追加到日志控件
    connect(worker_, &DemoWorker::logMessage, log_, &QTextEdit::append);

    // 失败：写日志并弹警告框
    connect(worker_, &DemoWorker::failed, this, [this](const QString &e) {
        log_->append(QStringLiteral("失败：") + e);
        QMessageBox::warning(this, QStringLiteral("出错了"), e);
    });

    // 探查文字 → 左侧信息标签
    connect(worker_, &DemoWorker::probeDone, infoLabel_, &QLabel::setText);

    // 预览完成：缓存列表，显示第 0 个文件
    connect(worker_, &DemoWorker::previewDone, this, [this](const QVector<PreviewBundle> &items) {
        previewItems_ = items; // 整表拷贝到 UI 成员
        previewIndex_ = 0;     // 从头看
        showPreviewIndex(0);   // 刷新两张图
        log_->append(QStringLiteral("预览好了，%1 个文件").arg(items.size()));
    });

    // HRRP 完成：路径写信息区，曲线画到 hrrpPlot_
    connect(worker_, &DemoWorker::hrrpDone, this,
            [this](const QString &path, const QVector<double> &R, const QVector<double> &rcs) {
                infoLabel_->setText(QStringLiteral("HRRP 写完了：\n") + path);
                hrrpPlot_->setData(R, rcs); // 距离 vs RCS
                hrrpPlot_->setTitle(QStringLiteral("HRRP"));
                log_->append(path);
            });

    // 任务结束：恢复按钮可点
    connect(worker_, &DemoWorker::finished, this, [this]() { setBusy(false); });

    thread_->start(); // 启动线程事件循环；此后 Queued 槽才会被处理

    // ---------- 按钮 → 本类槽 ----------
    connect(inBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseIn);
    connect(outBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseOut);
    connect(refreshPol, &QPushButton::clicked, this, &MainWindow::onRefreshPol);
    connect(probeBtn_, &QPushButton::clicked, this, &MainWindow::onProbe);
    connect(previewBtn_, &QPushButton::clicked, this, &MainWindow::onPreview);
    connect(hrrpBtn_, &QPushButton::clicked, this, &MainWindow::onHrrp);
    connect(cancelBtn_, &QPushButton::clicked, this, &MainWindow::onCancel);
    connect(prevBtn_, &QPushButton::clicked, this, &MainWindow::onPrevFile);
    connect(nextBtn_, &QPushButton::clicked, this, &MainWindow::onNextFile);
}

// ======================== 析构：停库、停线程 ========================
MainWindow::~MainWindow()
{
    if (worker_)
        worker_->requestCancel(); // 尽快让库退出循环
    if (thread_) {
        thread_->quit();      // 结束线程事件循环
        thread_->wait(3000);  // 最多等 3 秒，避免关窗卡死过久
    }
}

// busy=true：正在跑库；禁用业务按钮，只留取消
void MainWindow::setBusy(bool busy)
{
    probeBtn_->setEnabled(!busy);
    previewBtn_->setEnabled(!busy);
    hrrpBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy);
}

// 选输入目录；若输出为空则给一个默认 HRRP 子目录名，并自动扫极化
void MainWindow::onBrowseIn()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选数据目录"), inEdit_->text());
    if (d.isEmpty()) // 用户取消对话框
        return;
    inEdit_->setText(d); // 填入路径
    if (outEdit_->text().trimmed().isEmpty()) // 输出还没填时给默认
        outEdit_->setText(d + QStringLiteral("/处理结果_HRRP_HH"));
    onRefreshPol(); // 按真实文件刷新极化列表
}

// 选输出目录
void MainWindow::onBrowseOut()
{
    const QString d =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选输出目录"), outEdit_->text());
    if (!d.isEmpty())
        outEdit_->setText(d);
}

// 扫目录里实际存在的极化（快，demo 直接在 UI 线程调 SDK）
void MainWindow::onRefreshPol()
{
    const QString folder = inEdit_->text().trimmed(); // 去掉首尾空白
    if (folder.isEmpty())
        return;

    std::vector<std::string> pols; // SDK 输出
    std::string err;
    if (!listAvailablePolarizations(toUtf8(folder), pols, &err)) {
        log_->append(QStringLiteral("扫极化失败：") + QString::fromStdString(err));
        return;
    }

    polCombo_->clear(); // 清掉默认 HH/HV/...
    for (size_t i = 0; i < pols.size(); ++i)
        polCombo_->addItem(QString::fromUtf8(pols[i].c_str())); // 按实际有的加
    log_->append(QStringLiteral("找到 %1 种极化").arg(polCombo_->count()));
}

// 派探查任务到 Worker（Queued：不在 UI 线程跑库）
void MainWindow::onProbe()
{
    if (inEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("先选数据目录"));
        return;
    }
    setBusy(true);     // 锁按钮
    bar_->setValue(0); // 进度归零
    // 槽名字符串必须与 DemoWorker 的 public slots 一致；参数用 Q_ARG 装箱
    QMetaObject::invokeMethod(worker_, "runProbe", Qt::QueuedConnection,
                              Q_ARG(QString, inEdit_->text().trimmed()),
                              Q_ARG(QString, polCombo_->currentText()));
}

// 派预览任务
void MainWindow::onPreview()
{
    if (inEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("先选数据目录"));
        return;
    }
    setBusy(true);
    bar_->setValue(0);
    log_->append(QStringLiteral("开始预览…"));
    QMetaObject::invokeMethod(worker_, "runPreview", Qt::QueuedConnection,
                              Q_ARG(QString, inEdit_->text().trimmed()),
                              Q_ARG(QString, polCombo_->currentText()));
}

// 派 HRRP；输出空则按输入目录拼默认路径
void MainWindow::onHrrp()
{
    if (inEdit_->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("先选数据目录"));
        return;
    }

    QString out = outEdit_->text().trimmed();
    if (out.isEmpty()) {
        // 与库 defaultOutDir / 界面习惯一致的目录名
        out = inEdit_->text().trimmed() + QStringLiteral("/处理结果_HRRP_") +
              polCombo_->currentText();
        outEdit_->setText(out); // 回填，方便用户看见将要写到哪
    }

    setBusy(true);
    bar_->setValue(0);
    log_->append(QStringLiteral("跑 HRRP…"));
    QMetaObject::invokeMethod(worker_, "runHrrp", Qt::QueuedConnection,
                              Q_ARG(QString, inEdit_->text().trimmed()), Q_ARG(QString, out),
                              Q_ARG(QString, polCombo_->currentText()));
}

// 只置取消标志；不立刻 setBusy(false)，等 Worker emit finished
void MainWindow::onCancel()
{
    if (worker_)
        worker_->requestCancel();
}

// 预览列表循环向前
void MainWindow::onPrevFile()
{
    if (previewItems_.isEmpty())
        return;
    // +size 再取模，避免 (0-1)%n 在 C++ 里得到负数
    previewIndex_ = (previewIndex_ - 1 + previewItems_.size()) % previewItems_.size();
    showPreviewIndex(previewIndex_);
}

// 预览列表循环向后
void MainWindow::onNextFile()
{
    if (previewItems_.isEmpty())
        return;
    previewIndex_ = (previewIndex_ + 1) % previewItems_.size();
    showPreviewIndex(previewIndex_);
}

// 按索引把缓存的 PreviewBundle 画到时域/频域图
void MainWindow::showPreviewIndex(int idx)
{
    if (idx < 0 || idx >= previewItems_.size()) // 防御非法下标
        return;
    const PreviewBundle &b = previewItems_[idx]; // 当前文件数据
    // 标签：序号 / 总数 + 文件名
    fileLabel_->setText(
        QStringLiteral("%1 / %2   %3").arg(idx + 1).arg(previewItems_.size()).arg(b.fileName));
    timePlot_->setData(b.tUs, b.amp); // 时域曲线
    timePlot_->setTitle(QStringLiteral("时域  峰 %1 us").arg(b.ampPkUs, 0, 'f', 4));
    freqPlot_->setData(b.fMHz, b.spectrumDb); // 频域曲线
    freqPlot_->setTitle(QStringLiteral("频域  峰 %1 MHz").arg(b.fPkMHz, 0, 'f', 3));
}
