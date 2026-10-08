#ifndef RCSWORKER_H
#define RCSWORKER_H

#include "common/ProcessWorker.h"
#include "Types.h"
#include <QImage>

struct RcsParams {
    QString dataFolder;
    QString pol = QStringLiteral("HH");
    QString tgtName = QStringLiteral("目标");
    QString outDir;
    double sigmaTheoryDb = 18.25;
    int cropN = 8192;
    int phaseMode = 0;
    int azMode = 2; // 1 strip 2 spotlight
    double vSar = 10.0;
};

class RcsWorker : public ProcessWorker {
    Q_OBJECT
public:
    explicit RcsWorker(QObject *parent = nullptr);
    void setParams(const RcsParams &p) { params_ = p; }

public slots:
    void startProcess();

signals:
    /** 传预览图，避免跨线程拷贝整幅复数矩阵导致界面卡死 */
    void needCornerPick(const QVector<double> &R, const QImage &preview, int fullNr, int fullNp);
    void needTargetPick(const QVector<double> &R, const QImage &preview, int fullNr, int fullNp,
                        const QVector<double> &meanPowerDb, int modeHint);
    void rcsReady(const RcsResultBundle &bundle);

private:
    RcsParams params_;
};

#endif
