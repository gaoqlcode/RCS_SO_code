#ifndef CONFIRMBRIDGE_H
#define CONFIRMBRIDGE_H

#include <QObject>

// 住在 UI 线程。Worker 里 onCornerConfirm 用 BlockingQueuedConnection 调过来弹窗。
class ConfirmBridge : public QObject {
    Q_OBJECT
public:
    explicit ConfirmBridge(QObject *parent = 0) : QObject(parent), lastRangeM_(-1) {}

    double lastRangeM() const { return lastRangeM_; }

public slots:
    // true=确认（距离在 lastRangeM_），false=取消
    bool askCornerConfirm(double suggestedM);

private:
    double lastRangeM_;
};

#endif
