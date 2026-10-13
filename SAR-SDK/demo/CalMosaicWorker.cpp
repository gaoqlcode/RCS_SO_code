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

void CalMosaicWorker::startZeroImage(const QString &sourceDirectory, const QString &outputDirectory,
                                     int beginId, int deltaNum, int lastFileId, int fileNum,
                                     const QString &engineDir, const QString &mcrRoot, int timeoutSec)
{
    cancel_.store(false);
    clearWait();
    emit message(QStringLiteral("开始：目录内源 DAT 全部转为四极化 L0，然后成像…"));

    ZeroImageFolderRequest req;
    req.sourceDirectory = ss(sourceDirectory);
    req.outputDirectory = ss(outputDirectory);
    req.parameters.beginId = beginId;
    req.parameters.deltaNum = deltaNum;
    req.parameters.lastFileId = lastFileId;
    req.parameters.fileNum = fileNum;
    req.engineDirectory = ss(engineDir);
    req.mcrRoot = ss(mcrRoot);
    req.timeoutMs = timeoutSec > 0 ? timeoutSec * 1000 : 0;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) {
        emit progress(p, qs(m));
        emit message(qs(m));
    };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return cancel_.load(); };

    ZeroImageFolderResponse resp;
    std::string err;
    if (!processZeroImageFolder(req, resp, cb, &err)) {
        if (cancel_.load())
            emit finished(false, QStringLiteral("已取消"));
        else
            emit finished(false, qs(err));
        return;
    }

    QString msg = QStringLiteral("完成: 源文件=%1 成像退出码=%2 图像=%3\n")
                      .arg(static_cast<qulonglong>(resp.sources.size()))
                      .arg(resp.exitCode)
                      .arg(static_cast<qulonglong>(resp.publishedFiles.size()));
    for (size_t i = 0; i < resp.sources.size(); ++i) {
        const ZeroImageFolderSource &s = resp.sources[i];
        msg += QStringLiteral("  源 %1\n").arg(qs(s.sourcePath));
        for (size_t k = 0; k < s.result.outputs.size(); ++k) {
            const ToZeroDataPolarizationOut &o = s.result.outputs[k];
            msg += QStringLiteral("    L0 %1  %2  %3帧\n").arg(qs(o.polarization)).arg(qs(o.path)).arg(o.frames);
        }
        if (!s.result.recoveryPath.empty())
            msg += QStringLiteral("    还原 %1\n").arg(qs(s.result.recoveryPath));
    }
    for (size_t i = 0; i < resp.publishedFiles.size(); ++i) {
        const ZeroImagePublishedFile &f = resp.publishedFiles[i];
        msg += QStringLiteral("  %1  %2 字节\n").arg(qs(f.path)).arg(f.bytes);
    }
    if (!resp.recordPath.empty())
        msg += QStringLiteral("  记录: %1\n").arg(qs(resp.recordPath));
    for (size_t i = 0; i < resp.warnings.size(); ++i)
        msg += QStringLiteral("  提示: %1\n").arg(qs(resp.warnings[i]));
    emit finished(true, msg);
}
