#include "ProcessWorker.h"
#include <QMutexLocker>
#include <QMetaObject>
#include <QThread>

ProcessWorker::ProcessWorker(QObject *parent) : QObject(parent) {}

void ProcessWorker::requestCancel()
{
    cancel_.store(true);
    QMutexLocker lk(&mutex_);
    selectionCv_.wakeAll();
}

void ProcessWorker::provideSelection(const UserSelection &sel)
{
    QMutexLocker lk(&mutex_);
    selection_ = sel;
    haveSelection_ = true;
    selectionCv_.wakeAll();
}

bool ProcessWorker::isCancelled() const
{
    return cancel_.load();
}

void ProcessWorker::emitProgress(int percent, const QString &status)
{
    // 库内线程池不得直接 emit；若误从其它线程回调，投递回 Worker 线程
    if (QThread::currentThread() == thread()) {
        emit progress(percent, status);
        return;
    }
    const QString s = status;
    QMetaObject::invokeMethod(
        this,
        [this, percent, s]() { emit progress(percent, s); },
        Qt::QueuedConnection);
}

bool ProcessWorker::waitForSelection(UserSelection &out)
{
    QMutexLocker lk(&mutex_);
    haveSelection_ = false;
    while (!haveSelection_ && !cancel_.load())
        selectionCv_.wait(&mutex_);
    if (cancel_.load())
        return false;
    out = selection_;
    haveSelection_ = false;
    return true;
}

void ProcessWorker::clearSelectionWait()
{
    QMutexLocker lk(&mutex_);
    haveSelection_ = false;
    cancel_.store(false);
}
