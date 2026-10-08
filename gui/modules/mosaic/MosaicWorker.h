#ifndef MOSAICWORKER_H
#define MOSAICWORKER_H

#include "common/ProcessWorker.h"
#include "common/RcsApiQt.h"
#include <QVector>

struct MosaicParams {
    QString inFolder;
    QString outFolder;
    bool doFlatten = true;
    double medOut = 0.35;
    int mode = 0;
    QVector<int> offR;
    QVector<int> offC;
    int rawNrHint = 0;
    int rawNaHint = 0;
};

class MosaicWorker : public ProcessWorker {
    Q_OBJECT
public:
    explicit MosaicWorker(QObject *parent = nullptr);
    void setParams(const MosaicParams &p) { params_ = p; }

public slots:
    void startProcess();
    void startFlattenOnly(const QString &tifPath, double medOut);

signals:
    void mosaicReady(const MosaicResult &result);

private:
    MosaicParams params_;
};

#endif
