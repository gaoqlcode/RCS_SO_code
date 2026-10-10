#ifndef DEMOWORKER_H
#define DEMOWORKER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMetaType>
#include <atomic>

class ConfirmBridge;

struct PreviewBundle {
    QString fileName;
    QVector<double> tUs, amp, fMHz, spectrumDb;
    double ampPkUs;
    double fPkMHz;
};

Q_DECLARE_METATYPE(PreviewBundle)
Q_DECLARE_METATYPE(QVector<PreviewBundle>)

// 只调库，不碰控件；进度/结果用 signal 回主线程
class DemoWorker : public QObject {
    Q_OBJECT
public:
    explicit DemoWorker(QObject *parent = 0);
    void setConfirmBridge(ConfirmBridge *bridge) { bridge_ = bridge; }
    void requestCancel() { cancel_ = true; }

public slots:
    void runProbe(const QString &folder, const QString &pol);
    void runPreview(const QString &folder, const QString &pol);
    void runHrrp(const QString &folder, const QString &outDir, const QString &pol);

signals:
    void progress(int percent, const QString &msg);
    void logMessage(const QString &msg);
    void failed(const QString &err);
    void probeDone(const QString &text);
    void previewDone(const QVector<PreviewBundle> &items);
    void hrrpDone(const QString &hrrpPath, const QVector<double> &rangeM,
                  const QVector<double> &rcsDbsm);
    void finished();

private:
    std::atomic<bool> cancel_;
    ConfirmBridge *bridge_;
};

#endif
