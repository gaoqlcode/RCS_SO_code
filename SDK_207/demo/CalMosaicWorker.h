#ifndef CALMOSAICWORKER_H
#define CALMOSAICWORKER_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QRect>
#include <QString>
#include <QWaitCondition>
#include <atomic>

/** 工作线程：定标 / 拼接；框选时阻塞等待主线程 provideRectRoi */
class CalMosaicWorker : public QObject {
    Q_OBJECT
public:
    explicit CalMosaicWorker(QObject *parent = 0);

    void requestCancel();
    void provideRectRoi(bool ok, const QRect &fullResRoi);

public slots:
    void startCal(const QString &calibRaw, const QString &rawFolder, const QString &outDir,
                  const QString &band, const QString &pol, double sigmaTheoryDb);
    void startMosaic(const QString &inFolder, const QString &outFolder, double medOut);
    void startToZero(const QString &sourcePath, const QString &outputBasePath, bool saveRecovery);

signals:
    void progress(int percent, const QString &status);
    void message(const QString &text);
    void needRectRoi(const QImage &preview, int fullNr, int fullNa);
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
};

#endif
