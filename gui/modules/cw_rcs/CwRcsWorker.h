#ifndef CWRCSWORKER_H
#define CWRCSWORKER_H

#include "common/ProcessWorker.h"
#include "Types.h"
#include <QImage>

struct CwRcsParams {
    QString dataFolder;
    QString pol;
    QString tgtName;
    QString outDir;
    double sigmaTheoryDb;
    int targetMode; // 默认 1，交互时可改

    CwRcsParams()
        : pol(QStringLiteral("HH"))
        , tgtName(QStringLiteral("目标"))
        , sigmaTheoryDb(18.25)
        , targetMode(1)
    {
    }
};

struct CwRcsResultQt {
    QVector<double> freqGHz;
    QVector<double> rcsDbsm;
    QString l3Path;
};

Q_DECLARE_METATYPE(CwRcsResultQt)

class CwRcsWorker : public ProcessWorker {
    Q_OBJECT
public:
    explicit CwRcsWorker(QObject *parent = 0);
    void setParams(const CwRcsParams &p) { params_ = p; }

public slots:
    void startProcess();

signals:
    void needCornerConfirm(const QVector<double> &R, const QVector<double> &profileDb, int peakIdx);
    void needTargetPick(const QVector<double> &R, const QImage &preview, int fullNr, int fullNp,
                        const QVector<double> &meanPowerDb, int modeHint);
    void cwRcsReady(const CwRcsResultQt &bundle);

private:
    CwRcsParams params_;
};

#endif
