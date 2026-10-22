#include "CalMosaicWorker.h"

#include "rcs/rcs.hpp"

#include <cstring>
#include <vector>

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

void fillImaging(ZeroImageParameters &p, int beginId, int deltaNum, int lastFileId, int fileNum,
                 int rangeOffset, int rangePoints)
{
    p.beginId = beginId;
    p.deltaNum = deltaNum;
    p.lastFileId = lastFileId;
    p.fileNum = fileNum;
    p.rangeOffset = rangeOffset;
    p.rangePoints = rangePoints;
}

QString sidecarLines(const std::vector<ToZeroDataPolarizationOut> &outs)
{
    QString msg;
    for (size_t k = 0; k < outs.size(); ++k) {
        const ToZeroDataPolarizationOut &o = outs[k];
        msg += QStringLiteral("    L0 %1  %2  %3帧\n").arg(qs(o.polarization)).arg(qs(o.path)).arg(o.frames);
        if (!o.path.empty())
            msg += QStringLiteral("    zeroImageSidecarPath: %1\n").arg(qs(zeroImageSidecarPath(o.path)));
    }
    return msg;
}

} // namespace

CalMosaicWorker::CalMosaicWorker(QObject *parent)
    : QObject(parent)
    , cancel_(false)
    , haveRoi_(false)
    , roiOk_(false)
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
                               double sigmaTheoryDb, const QString &l0Header, const QString &mergeTxt)
{
    cancel_.store(false);
    clearWait();
    emit message(QStringLiteral("开始 RCS 像辐射定标…"));

    RcsCalTifRequest req;
    req.calibRawPath = ss(calibRaw);
    req.rawFolder = ss(rawFolder);
    req.outDir = ss(outDir);
    req.l0HeaderPath = ss(l0Header);
    req.mergeTxtPath = ss(mergeTxt);
    req.band = ss(band);
    req.pol = ss(pol);
    req.sigmaTheoryDb = sigmaTheoryDb;
    req.doCalib = true;
    req.autoPick = false; // GUI：走 onRectRoi
    req.tgtName = "目标";

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emit progress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return cancel_.load(); };
    cb.onRectRoi = [this](const GrayImage &g, int fullNr, int fullNa, const std::string &stage,
                          RectRoi &out) -> bool {
        clearWait();
        emit needRectRoi(grayToQImage(g), fullNr, fullNa, qs(stage));
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
        if (!f.pdatPath.empty())
            msg += QStringLiteral("    L2:   %1\n").arg(qs(f.pdatPath));
        if (!f.positionPath.empty())
            msg += QStringLiteral("    位置: %1\n").arg(qs(f.positionPath));
        if (!f.note.empty())
            msg += QStringLiteral("    说明: %1\n").arg(qs(f.note));
    }
    emit finished(true, msg);
}

void CalMosaicWorker::setMosaicOffsets(const QVector<int> &offR, const QVector<int> &offC)
{
    offR_ = offR;
    offC_ = offC;
}

void CalMosaicWorker::startMosaic(const QString &inFolder, const QString &outFolder, double medOut,
                                  int mode, int rawNr, int rawNa)
{
    cancel_.store(false);
    clearWait();
    emit message(mode == 1 ? QStringLiteral("调用 processMosaic（mode=1 手动偏移）…")
                           : QStringLiteral("调用 processMosaic（mode=0 自动配准）…"));

    MosaicRequest req;
    req.inFolder = ss(inFolder);
    req.outFolder = ss(outFolder);
    req.mode = mode == 1 ? 1 : 0;
    req.rawNrHint = rawNr;
    req.rawNaHint = rawNa;
    req.medOut = medOut > 0 ? medOut : 0.35;
    if (req.mode == 1) {
        for (int i = 0; i < offR_.size(); ++i)
            req.offR.push_back(offR_[i]);
        for (int i = 0; i < offC_.size(); ++i)
            req.offC.push_back(offC_[i]);
        offR_.clear();
        offC_.clear();
        if (req.offR.size() < 2 || req.offR.size() != req.offC.size()) {
            emit finished(false, QStringLiteral("手动偏移数量不足"));
            return;
        }
    }

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

void CalMosaicWorker::startFlatten(const QString &inTif, const QString &outTif, double medOut)
{
    cancel_.store(false);
    clearWait();
    emit message(QStringLiteral("调用 flattenMosaic…"));

    QString out = outTif.trimmed();
    if (out.isEmpty()) {
        const int dot = inTif.lastIndexOf(QLatin1Char('.'));
        out = (dot > 0 ? inTif.left(dot) : inTif) + QStringLiteral("_匀光.tif");
    }

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) {
        emit progress(p, qs(m));
        emit message(qs(m));
    };
    cb.isCancelled = [this]() { return cancel_.load(); };

    std::string err;
    if (!flattenMosaic(ss(inTif), ss(out), medOut > 0 ? medOut : 0.35, std::vector<int>(), cb, &err)) {
        if (cancel_.load())
            emit finished(false, QStringLiteral("已取消"));
        else
            emit finished(false, qs(err));
        return;
    }
    emit finished(true, QStringLiteral("匀光完成:\n  %1\n").arg(out));
}

void CalMosaicWorker::startZeroImage(const QString &sourceDirectory, const QString &outputDirectory,
                                     int beginId, int deltaNum, int lastFileId, int fileNum,
                                     int rangeOffset, int rangePoints, int kind)
{
    cancel_.store(false);
    clearWait();
    emit message(kind == 1 ? QStringLiteral("调用 processZeroImageFolder（目录转 L0 后 ISAR）…")
                           : QStringLiteral("调用 processZeroImageFolder（目录转 L0 后 SAR）…"));

    ZeroImageFolderRequest req;
    req.kind = kind == 1 ? 1 : 0;
    req.sourceDirectory = ss(sourceDirectory);
    req.outputDirectory = ss(outputDirectory);
    fillImaging(req.parameters, beginId, deltaNum, lastFileId, fileNum, rangeOffset, rangePoints);

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
        msg += sidecarLines(s.result.outputs);
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
