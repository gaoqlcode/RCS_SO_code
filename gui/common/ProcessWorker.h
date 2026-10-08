#ifndef PROCESSWORKER_H
#define PROCESSWORKER_H

/**
 * @file ProcessWorker.h
 * @brief 处理线程 Worker 基类：进度/日志/取消/人机交互断点（UI 与算法线程分离）
 */

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <atomic>
#include "Types.h"

class ProcessWorker : public QObject {
    Q_OBJECT
public:
    explicit ProcessWorker(QObject *parent = nullptr);

public slots:
    /**
     * UI 请求取消。可从任意线程直接调用（勿用 QueuedConnection 投递到忙碌的 Worker，
     * 否则 startProcess 占着事件循环时标志设不上）。
     */
    void requestCancel();
    /** UI 回填交互选择后唤醒 Worker */
    void provideSelection(const UserSelection &sel);

signals:
    void progress(int percent, const QString &status);
    void message(const QString &text);
    void errorOccurred(const QString &text);
    void needUserInput(const QString &reason);
    void finishedOk();
    void finishedCancelled();

protected:
    bool isCancelled() const;
    void emitProgress(int percent, const QString &status);
    /** 阻塞等待 UI 选择；取消则返回 false */
    bool waitForSelection(UserSelection &out);
    void clearSelectionWait();

    mutable QMutex mutex_;
    std::atomic<bool> cancel_{false};
    bool haveSelection_ = false;
    UserSelection selection_;
    QWaitCondition selectionCv_;
};

#endif
