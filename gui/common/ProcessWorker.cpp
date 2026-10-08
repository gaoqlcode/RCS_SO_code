#include "ProcessWorker.h"
#include <QMutexLocker>

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
    emit progress(percent, status);
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
