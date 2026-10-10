#ifndef SIGMA0WORKER_H
#define SIGMA0WORKER_H

#include "common/ProcessWorker.h"
#include <QImage>
#include <QMetaType>
#include <QString>
#include <QVector>

struct Sigma0Params {
    QString rawPath;
    QString outDir;
    QString tgtName;
    int nr;
    int na;
    double fcGHz;
    double vs;
    double incAngleDeg;
    double hFlight;
    double sigmaTheoryDb;
    int targetType; // 0 area 1 point
    int bgMode;
    QString measureTime;
};

struct Sigma0ResultQt {
    QString csPath;
    double K;
    QVector<double> sigma0Db;
};

Q_DECLARE_METATYPE(Sigma0ResultQt)

class Sigma0Worker : public ProcessWorker {
    Q_OBJECT
public:
    explicit Sigma0Worker(QObject *parent = 0);
    void setParams(const Sigma0Params &p) { params_ = p; }

public slots:
    void startProcess();

signals:
    void needRectRoi(const QImage &preview, int fullNr, int fullNa, const QString &stage);
    void needTargetRois(const QImage &preview, int fullNr, int fullNa);
    void sigma0Ready(const Sigma0ResultQt &r);

private:
    Sigma0Params params_;
};

#endif
