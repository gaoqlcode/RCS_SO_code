#ifndef HRRPWORKER_H
#define HRRPWORKER_H

#include "common/ProcessWorker.h"

struct HrrpParams {
    QString dataFolder;
    QString pol = QStringLiteral("HH");
    QString tgtName = QStringLiteral("目标");
    QString outDir;
    double sigmaTheoryDb = 18.25;
    int cropN = 8192;
    int phaseMode = 0;
};

class HrrpWorker : public ProcessWorker {
    Q_OBJECT
public:
    explicit HrrpWorker(QObject *parent = nullptr);
    void setParams(const HrrpParams &p) { params_ = p; }

public slots:
    void startProcess();

signals:
    void hrrpReady(const HrrpResult &result);
    void needCornerConfirm(const QVector<double> &R, const QVector<double> &profileDb, int peakIdx);

private:
    HrrpParams params_;
};

#endif
