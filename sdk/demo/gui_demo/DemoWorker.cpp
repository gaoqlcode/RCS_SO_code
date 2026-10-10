#include "DemoWorker.h"
#include "ConfirmBridge.h"
#include "rcs/rcs.hpp"

#include <QByteArray>
#include <QMetaObject>
#include <QVector>

namespace {

std::string toUtf8(const QString &s)
{
    const QByteArray u = s.toUtf8();
    return std::string(u.constData(), static_cast<size_t>(u.size()));
}

QString fromUtf8(const std::string &s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

QVector<double> copyVec(const std::vector<double> &v)
{
    QVector<double> o;
    o.reserve(static_cast<int>(v.size()));
    for (size_t i = 0; i < v.size(); ++i)
        o.push_back(v[i]);
    return o;
}

} // namespace

DemoWorker::DemoWorker(QObject *parent) : QObject(parent), cancel_(false), bridge_(0) {}

void DemoWorker::runProbe(const QString &folder, const QString &pol)
{
    cancel_ = false;
    L0FolderProbe p;
    std::string err;
    if (!probeDataFolder(toUtf8(folder), toUtf8(pol), p, &err)) {
        emit failed(fromUtf8(err));
        emit finished();
        return;
    }

    QString text;
    text += QStringLiteral("极化：%1\n").arg(fromUtf8(p.pol));
    text += QStringLiteral("文件数：%1\n").arg(p.fileCount);
    text += QStringLiteral("第一个：%1\n").arg(fromUtf8(p.firstFile));
    text += QStringLiteral("最后一个：%1\n").arg(fromUtf8(p.lastFile));
    if (p.hasHeader) {
        text += QStringLiteral("中心频率：%1 GHz\n").arg(p.firstHeader.fcGHz, 0, 'f', 4);
        text += QStringLiteral("带宽：%1 MHz\n").arg(p.firstHeader.bwMHz);
    }
    emit probeDone(text);
    emit finished();
}

void DemoWorker::runPreview(const QString &folder, const QString &pol)
{
    cancel_ = false;

    L0PreviewRequest req;
    req.dataFolder = toUtf8(folder);
    req.pol = toUtf8(pol);
    req.maxFiles = 8;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emit progress(p, fromUtf8(m)); };
    cb.onMessage = [this](const std::string &m) { emit logMessage(fromUtf8(m)); };
    cb.isCancelled = [this]() { return cancel_.load(); };

    L0PreviewResponse resp;
    std::string err;
    if (!previewL0Folder(req, resp, cb, &err)) {
        emit failed(fromUtf8(err));
        emit finished();
        return;
    }

    QVector<PreviewBundle> items;
    for (size_t i = 0; i < resp.items.size(); ++i) {
        const L0PulsePreviewItem &src = resp.items[i];
        PreviewBundle one;
        one.fileName = fromUtf8(src.fileName);
        one.tUs = copyVec(src.tUs);
        one.amp = copyVec(src.amp);
        one.fMHz = copyVec(src.fMHz);
        one.spectrumDb = copyVec(src.spectrumDb);
        one.ampPkUs = src.ampPkUs;
        one.fPkMHz = src.fPkMHz;
        items.push_back(one);
    }
    emit previewDone(items);
    emit finished();
}

void DemoWorker::runHrrp(const QString &folder, const QString &outDir, const QString &pol)
{
    cancel_ = false;

    HrrpRequest req;
    req.dataFolder = toUtf8(folder);
    req.outDir = toUtf8(outDir);
    req.pol = toUtf8(pol);

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emit progress(p, fromUtf8(m)); };
    cb.onMessage = [this](const std::string &m) { emit logMessage(fromUtf8(m)); };
    cb.isCancelled = [this]() { return cancel_.load(); };

    cb.onCornerConfirm = [this](const std::vector<double> &R, const std::vector<double> & /*db*/,
                                int peakIdx, InteractSelection &sel) -> bool {
        if (!bridge_)
            return false;
        const double suggest =
            (peakIdx >= 0 && peakIdx < static_cast<int>(R.size())) ? R[static_cast<size_t>(peakIdx)]
                                                                   : 0.0;
        bool accepted = false;
        const bool invoked = QMetaObject::invokeMethod(
            bridge_, "askCornerConfirm", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, accepted),
            Q_ARG(double, suggest));
        if (!invoked || !accepted)
            return false;
        sel.cornerRangeOverrideM = bridge_->lastRangeM();
        return true;
    };

    HrrpResponse resp;
    std::string err;
    if (!processHrrp(req, resp, cb, &err)) {
        emit failed(fromUtf8(err));
        emit finished();
        return;
    }
    emit hrrpDone(fromUtf8(resp.hrrpPath), copyVec(resp.rangeM), copyVec(resp.rcsDbsm));
    emit finished();
}
