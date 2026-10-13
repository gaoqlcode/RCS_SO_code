#include "CalMosaicWorker.h"

#include "rcs/rcs.hpp"

#include <cstring>

namespace {

QImage grayToQImage(const GrayImage &g)
{
    if (g.width <= 0 || g.height <= 0 || g.pixels.empty())
        return QImage();
    QImage img(g.width, g.height, QImage::Format_Grayscale8);
    for (int y = 0; y < g.height; ++y) {
        std::memcpy(img.scanLine(y), &g.pixels[static_cast<size_t>(y) * g.width],
                    static_cast<size_t>(g.width));
    }
    return img;
}

QString qs(const std::string &s)
{
    return QString::fromUtf8(s.c_str());
}

std::string ss(const QString &s)
{
    return std::string(s.toUtf8().constData());
}

} // namespace

CalMosaicWorker::CalMosaicWorker(QObject *parent)
    : QObject(parent), cancel_(false), haveRoi_(false), roiOk_(false)
{
}

void CalMosaicWorker::requestCancel()
{
    cancel_.store(true);
    QMutexLocker lock(&mutex_);
    haveRoi_ = true;
    roiOk_ = false;
    cv_.wakeAll();
}

void CalMosaicWorker::provideRectRoi(bool ok, const QRect &fullResRoi)
{
    QMutexLocker lock(&mutex_);
    haveRoi_ = true;
    roiOk_ = ok;
    roi_ = fullResRoi;
    cv_.wakeAll();
}

void CalMosaicWorker::clearWait()
{
    QMutexLocker lock(&mutex_);
    haveRoi_ = false;
    roiOk_ = false;
    roi_ = QRect();
}

bool CalMosaicWorker::waitRectRoi(QRect &out)
{
    QMutexLocker lock(&mutex_);
    while (!haveRoi_ && !cancel_.load())
        cv_.wait(&mutex_);
    if (cancel_.load() || !roiOk_)
        return false;
    out = roi_;
    haveRoi_ = false;
    return out.width() > 0 && out.height() > 0;
}

void CalMosaicWorker::startCal(const QString &calibRaw, const QString &rawFolder,
                               const QString &outDir, const QString &band, const QString &pol,
                               double sigmaTheoryDb)
{
    cancel_.store(false);
    clearWait();
    emit message(QStringLiteral("开始 RCS 像辐射定标…"));

    RcsCalTifRequest req;
    req.calibRawPath = ss(calibRaw);
    req.rawFolder = ss(rawFolder);
    req.outDir = ss(outDir);
    req.band = ss(band);
    req.pol = ss(pol);
    req.sigmaTheoryDb = sigmaTheoryDb;
    req.doCalib = true;
    req.autoPick = false; // GUI：走 onRectRoi
    req.l3Layout = rcs::RcsCalL3Point;
    req.tgtName = "目标";

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emit progress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return cancel_.load(); };
    cb.onRectRoi = [this](const GrayImage &g, int fullNr, int fullNa, const std::string & /*stage*/,
                          RectRoi &out) -> bool {
        clearWait();
        emit needRectRoi(grayToQImage(g), fullNr, fullNa);
        QRect r;
        if (!waitRectRoi(r))
            return false;
        out = RectRoi(r.x(), r.y(), r.width(), r.height());
        return out.valid();
    };

    RcsCalTifResponse resp;
    std::string err;
    if (!processRcsCalTif(req, resp, cb, &err)) {
        if (cancel_.load())
            emit finished(false, QStringLiteral("已取消"));
        else
            emit finished(false, qs(err));
        return;
    }

    QString msg = QStringLiteral("定标完成: K=%1  文件数=%2\n")
                      .arg(resp.K, 0, 'e', 6)
                      .arg(static_cast<int>(resp.files.size()));
    for (size_t i = 0; i < resp.files.size(); ++i) {
        const RcsCalTifFileResult &f = resp.files[i];
        msg += QStringLiteral("  %1\n").arg(qs(f.rawName));
        if (!f.tifPath.empty())
            msg += QStringLiteral("    TIFF: %1\n").arg(qs(f.tifPath));
        if (!f.txtPath.empty())
            msg += QStringLiteral("    L3:   %1\n").arg(qs(f.txtPath));
    }
    emit finished(true, msg);
}

void CalMosaicWorker::startMosaic(const QString &inFolder, const QString &outFolder, double medOut)
{
    cancel_.store(false);
    clearWait();
    emit message(QStringLiteral("开始自动拼接…"));

    MosaicRequest req;
    req.inFolder = ss(inFolder);
    req.outFolder = ss(outFolder);
    req.mode = 0;
    req.medOut = medOut > 0 ? medOut : 0.35;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) {
        emit progress(p, qs(m));
        emit message(qs(m));
    };
    cb.isCancelled = [this]() { return cancel_.load(); };

    MosaicResponse resp;
    std::string err;
    if (!processMosaic(req, resp, cb, &err)) {
        if (cancel_.load())
            emit finished(false, QStringLiteral("已取消"));
        else
            emit finished(false, qs(err));
        return;
    }

    QString msg = QStringLiteral("拼接完成:\n  MOSAIC:  %1\n  匀光:    %2\n  显示:    %3\n  L3:      %4\n")
                      .arg(qs(resp.mosaicTif))
                      .arg(qs(resp.flattenTif))
                      .arg(qs(resp.displayTif))
                      .arg(qs(resp.l3Path));
    emit finished(true, msg);
}

void CalMosaicWorker::startToZero(const QString &sourcePath, const QString &outputBasePath,
                                  bool saveRecovery)
{
    cancel_.store(false);
    clearWait();
    emit message(QStringLiteral("开始 ToZeroData 转换…"));

    ToZeroDataRequest req;
    req.sourcePath = ss(sourcePath);
    req.outputBasePath = ss(outputBasePath);
    req.saveRecovery = saveRecovery;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) {
        emit progress(p, qs(m));
        emit message(qs(m));
    };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return cancel_.load(); };

    ToZeroDataResponse resp;
    std::string err;
    if (!processToZeroData(req, resp, cb, &err)) {
        if (cancel_.load())
            emit finished(false, QStringLiteral("已取消"));
        else
            emit finished(false, qs(err));
        return;
    }

    QString msg = QStringLiteral("ToZeroData 完成: 有效记录=%1 跳过=%2 输出字节=%3\n")
                      .arg(resp.validRecords)
                      .arg(resp.skippedRecords)
                      .arg(resp.outputBytes);
    for (size_t i = 0; i < resp.outputs.size(); ++i) {
        const ToZeroDataPolarizationOut &o = resp.outputs[i];
        msg += QStringLiteral("  %1  %2  %3帧  %4字节\n")
                   .arg(qs(o.polarization))
                   .arg(qs(o.path))
                   .arg(o.frames)
                   .arg(o.bytes);
    }
    if (!resp.recoveryPath.empty()) {
        msg += QStringLiteral("  还原: %1 (%2 字节)\n  SHA-256: %3\n")
                   .arg(qs(resp.recoveryPath))
                   .arg(resp.recoveryBytes)
                   .arg(qs(resp.sourceSha256));
    }
    emit finished(true, msg);
}
