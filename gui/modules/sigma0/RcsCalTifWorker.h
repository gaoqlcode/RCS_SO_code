#ifndef RCSCALTIFWORKER_H
#define RCSCALTIFWORKER_H

#include "common/ProcessWorker.h"
#include <QImage>
#include <QMetaType>
#include <QString>
#include <QStringList>

struct RcsCalTifParams {
    QString calibRaw;
    QString rawFolder;
    QString outDir;
    QString l0HeaderPath;
    QString mergeTxtPath;
    QString tgtName;
    QString band;
    QString pol;
    double sigmaTheoryDb;
    double fsHz;
    double incAngleDeg;
    double hFlight;
    int maxFiles;
};

struct RcsCalTifResultQt {
    double K;
    QStringList tifPaths;
    QStringList txtPaths;
    QStringList pdatPaths;
    QStringList positionPaths;
    QString summary;
};

Q_DECLARE_METATYPE(RcsCalTifResultQt)

class RcsCalTifWorker : public ProcessWorker {
    Q_OBJECT
public:
    explicit RcsCalTifWorker(QObject *parent = 0);
    void setParams(const RcsCalTifParams &p) { params_ = p; }

public slots:
    void startProcess();

signals:
    void needRectRoi(const QImage &preview, int fullNr, int fullNa, const QString &stage);
    void calReady(const RcsCalTifResultQt &r);

private:
    RcsCalTifParams params_;
};

#endif
