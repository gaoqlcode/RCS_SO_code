// =============================================================================
// DemoWorker.h — 工作线程对象：只调 SDK，不直接碰任何 QWidget
//
// 进度/结果通过 signals 发回 UI 线程；取消用 atomic 标志供 Callbacks::isCancelled 读。
// 选点通过 ConfirmBridge + BlockingQueuedConnection 切回 UI 弹窗。
// =============================================================================

#ifndef DEMOWORKER_H
#define DEMOWORKER_H

#include <QObject>    // 信号槽基类；配合 moveToThread
#include <QString>    // 路径、极化、日志文字（跨线程信号参数）
#include <QVector>    // 曲线与 PreviewBundle 列表
#include <QMetaType>  // Q_DECLARE_METATYPE
#include <atomic>     // 无锁读写 cancel_，UI/Worker 都可访问

class ConfirmBridge; // 前置声明：只存指针，头文件不依赖 ConfirmBridge.h 的实现细节

// 把 SDK 的 L0PulsePreviewItem 里「界面要用的字段」收成可进信号的包
struct PreviewBundle {
    QString fileName;           // 源文件名（仅名，便于标签显示）
    QVector<double> tUs;        // 时域横轴：时间 μs
    QVector<double> amp;        // 时域纵轴：幅度
    QVector<double> fMHz;       // 频域横轴：频率 MHz
    QVector<double> spectrumDb; // 频域纵轴：归一化频谱 dB
    double ampPkUs;             // 时域峰值时刻 μs（标题用）
    double fPkMHz;              // 频域峰值频率 MHz（标题用）
};

// 告诉 Qt 元对象系统：这两种类型可以排队拷贝（还需 main 里 qRegisterMetaType）
Q_DECLARE_METATYPE(PreviewBundle)
Q_DECLARE_METATYPE(QVector<PreviewBundle>)

// 只调库，不碰控件；进度/结果用 signal 回主线程
class DemoWorker : public QObject {
    Q_OBJECT // 必须：slots/signals 要 moc 生成元对象代码
public:
    // parent 一般传 0：先无父对象再 moveToThread，避免「父子不在同一线程」
    explicit DemoWorker(QObject *parent = 0);

    // 注入 UI 线程上的桥；不取得所有权，生命周期由 MainWindow 保证
    void setConfirmBridge(ConfirmBridge *bridge) { bridge_ = bridge; }

    // UI 点「取消」时调用：置位原子变量；库在 isCancelled 里读到后尽快停
    void requestCancel() { cancel_ = true; }

public slots:
    // 以下槽由 MainWindow 用 QueuedConnection invokeMethod 调用 → 在 Worker 线程执行

    // 探查目录：文件数、首末名、文件头摘要
    void runProbe(const QString &folder, const QString &pol);

    // 0 级时域/频域预览（demo 限制 maxFiles=8）
    void runPreview(const QString &folder, const QString &pol);

    // HRRP：含 onCornerConfirm 交互确认峰距
    void runHrrp(const QString &folder, const QString &outDir, const QString &pol);

signals:
    // 进度 0~100 + 说明文字 → 进度条与日志
    void progress(int percent, const QString &msg);

    // 一般日志（对应 Callbacks::onMessage）
    void logMessage(const QString &msg);

    // 失败：err 为 SDK *err 转成的 QString；UI 弹警告
    void failed(const QString &err);

    // 探查结果拼好的多行文本 → 左侧信息标签
    void probeDone(const QString &text);

    // 预览列表 → 主窗口缓存并画时域/频域
    void previewDone(const QVector<PreviewBundle> &items);

    // HRRP 完成：落盘路径 + 曲线，供画图
    void hrrpDone(const QString &hrrpPath, const QVector<double> &rangeM,
                  const QVector<double> &rcsDbsm);

    // 任意任务结束（成功或失败）→ UI setBusy(false)
    void finished();

private:
    std::atomic<bool> cancel_; // true 表示用户请求取消
    ConfirmBridge *bridge_;    // UI 线程桥；可能为 nullptr（则 HRRP 确认直接失败）
};

#endif
