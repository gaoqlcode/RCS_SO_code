#ifndef L0PREVIEWWORKER_H
#define L0PREVIEWWORKER_H

#include "common/ProcessWorker.h"
#include "rcs/types.hpp"
#include <QMetaType>
#include <QString>
#include <QVector>

struct L0PreviewParams {
    QString dataFolder;
    QString pol;
    double fsHz;
    int pulseNo;
    bool removeDC;
    int maxFiles;
};

Q_DECLARE_METATYPE(QVector<L0PulsePreviewItem>)

class L0PreviewWorker : public ProcessWorker {
    Q_OBJECT
public:
    explicit L0PreviewWorker(QObject *parent = 0);
    void setParams(const L0PreviewParams &p) { params_ = p; }

public slots:
    void startProcess();

signals:
    void previewReady(const QVector<L0PulsePreviewItem> &items);

private:
    L0PreviewParams params_;
};

#endif
