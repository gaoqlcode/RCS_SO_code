// =============================================================================
// ConfirmBridge.h — UI 线程上的「选点/确认」桥
//
// 背景：SDK 的 onCornerConfirm 在 Worker 线程里被库同步调用。
//       不能在那线程直接弹 QWidget，否则线程规则被破坏。
// 做法：本对象父对象挂在 MainWindow → 住在 UI 线程；
//       Worker 用 BlockingQueuedConnection 调 askCornerConfirm，阻塞等用户点完。
// =============================================================================

#ifndef CONFIRMBRIDGE_H
#define CONFIRMBRIDGE_H

#include <QObject> // QObject：信号槽、线程亲和性、元对象系统

// 住在 UI 线程。Worker 里 onCornerConfirm 用 BlockingQueuedConnection 调过来弹窗。
class ConfirmBridge : public QObject {
    Q_OBJECT // 启用 moc：槽 askCornerConfirm 才能被 invokeMethod 按名字调用
public:
    // parent 一般为 MainWindow*：对象树随窗口销毁，且线程亲和性 = UI 线程
    // lastRangeM_ 初值 -1：表示尚未确认过有效距离
    explicit ConfirmBridge(QObject *parent = 0) : QObject(parent), lastRangeM_(-1) {}

    // 供 Worker 在弹窗返回后读取：用户确认的角反峰距离（米）
    double lastRangeM() const { return lastRangeM_; }

public slots:
    // 必须是 slot，才能 QMetaObject::invokeMethod(..., "askCornerConfirm", ...)
    // suggestedM：库算出的自动峰距离，作为对话框默认值
    // 返回 true=用户点确定（距离写入 lastRangeM_）；false=取消
    bool askCornerConfirm(double suggestedM);

private:
    // 最近一次用户确认的距离（米）；取消时不更新或保持旧值，由调用方根据返回值判断
    double lastRangeM_;
};

#endif
