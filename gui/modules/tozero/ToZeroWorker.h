#ifndef TOZEROWORKER_H
#define TOZEROWORKER_H

#include "common/ProcessWorker.h"
#include <QMetaType>
#include <QString>
#include <QStringList>

struct ToZeroParams {
    QString sourcePath;
    QString outputBasePath;
    bool saveRecovery;
    ToZeroParams() : saveRecovery(false) {}
};

struct ToZeroResultQt {
    quint64 validRecords;
    quint64 skippedRecords;
    quint64 outputBytes;
    QStringList lines; ///< 展示用摘要行
    QString recoveryPath;
    QString sourceSha256;
    QString summary;
    ToZeroResultQt() : validRecords(0), skippedRecords(0), outputBytes(0) {}
};

Q_DECLARE_METATYPE(ToZeroResultQt)

class ToZeroWorker : public ProcessWorker {
    Q_OBJECT
public:
    explicit ToZeroWorker(QObject *parent = 0);
    void setParams(const ToZeroParams &p) { params_ = p; }

public slots:
    void startProcess();

signals:
    void toZeroReady(const ToZeroResultQt &r);

private:
    ToZeroParams params_;
};

#endif
