#ifndef TOZEROWORKER_H
#define TOZEROWORKER_H

#include "common/ProcessWorker.h"
#include <QMetaType>
#include <QString>
#include <QStringList>

struct ToZeroParams {
    QString sourcePath;
    QString outputBasePath;
    QString inputDirectory;
    int beginId;
    int deltaNum;
    int lastFileId;
    int rangeOffset;
    int fileNum;
    int rangePoints;
    int kind;
    QString engineDirectory;
    int timeoutSec;
    bool requireEngine;

    ToZeroParams()
        : beginId(30)
        , deltaNum(120)
        , lastFileId(6000)
        , rangeOffset(100)
        , fileNum(16)
        , rangePoints(8192)
        , kind(0)
        , timeoutSec(0)
        , requireEngine(true)
    {
    }
};

struct ToZeroResultQt {
    int exitCode;
    quint64 restoredBytes;
    QStringList lines;
    QStringList warnings;
    QString recordPath;
    QString summary;
    ToZeroResultQt() : exitCode(-1), restoredBytes(0) {}
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
