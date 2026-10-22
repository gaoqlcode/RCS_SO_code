#ifndef CALMOSAICWORKER_H
#define CALMOSAICWORKER_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWaitCondition>
#include <atomic>

/** 工作线程：定标 / 拼接；框选时阻塞等待主线程 provideRectRoi */
class CalMosaicWorker : public QObject {
    Q_OBJECT
public:
    explicit CalMosaicWorker(QObject *parent = 0);

    void requestCancel();
    void provideRectRoi(bool ok, const QRect &fullResRoi);
    /** 主线程在排队 startMosaic 之前写入；mode=1 时使用 */
    void setMosaicOffsets(const QVector<int> &offR, const QVector<int> &offC);

public slots:
    void startCal(const QString &calibRaw, const QString &rawFolder, const QString &outDir,
                  const QString &band, const QString &pol, double sigmaTheoryDb,
                  const QString &l0Header, const QString &mergeTxt);
    void startMosaic(const QString &inFolder, const QString &outFolder, double medOut, int mode,
                     int rawNr, int rawNa);
    void startFlatten(const QString &inTif, const QString &outTif, double medOut);
    void startZeroImage(const QString &sourceDirectory, const QString &outputDirectory, int beginId,
                        int deltaNum, int lastFileId, int fileNum, int rangeOffset, int rangePoints,
                        int kind);

signals:
    void progress(int percent, const QString &status);
    void message(const QString &text);
    void needRectRoi(const QImage &preview, int fullNr, int fullNa, const QString &stage);
    void finished(bool ok, const QString &msg);

private:
    bool waitRectRoi(QRect &out);
    void clearWait();

    mutable QMutex mutex_;
    QWaitCondition cv_;
    std::atomic<bool> cancel_;
    bool haveRoi_;
    bool roiOk_;
    QRect roi_;
    QVector<int> offR_;
    QVector<int> offC_;
};

#endif
