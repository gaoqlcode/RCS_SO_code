#include "RcsCalTifWorker.h"
#include "common/RcsApiQt.h"

RcsCalTifWorker::RcsCalTifWorker(QObject *parent) : ProcessWorker(parent) {}

void RcsCalTifWorker::startProcess()
{
    clearSelectionWait();
    emit message(QStringLiteral("开始 RCS 像辐射定标…"));

    RcsCalTifRequest req;
    req.calibRawPath = ss(params_.calibRaw);
    req.rawFolder = ss(params_.rawFolder);
    req.outDir = ss(params_.outDir);
    req.tgtName = ss(params_.tgtName);
    req.band = ss(params_.band);
    req.pol = ss(params_.pol);
    req.sigmaTheoryDb = params_.sigmaTheoryDb;
    req.fsHz = params_.fsHz;
    req.incAngleDeg = params_.incAngleDeg;
    req.hFlight = params_.hFlight;
    req.maxFiles = params_.maxFiles;
    req.doCalib = true;
    req.autoPick = false;
    req.l3Layout = rcs::RcsCalL3Point;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return isCancelled(); };
    cb.onRectRoi = [this](const GrayImage &g, int fullNr, int fullNa, const std::string &stage,
                          RectRoi &out) -> bool {
        emit needRectRoi(grayToQImage(g), fullNr, fullNa, qs(stage));
        emit needUserInput(QStringLiteral("请框选角反缓冲区"));
        UserSelection qsel;
        if (!waitForSelection(qsel) || qsel.rectRois.isEmpty())
            return false;
        const QRect &r = qsel.rectRois.first();
        out = RectRoi(r.x(), r.y(), r.width(), r.height());
        return out.valid();
    };

    RcsCalTifResponse resp;
    std::string err;
    if (!processRcsCalTif(req, resp, cb, &err)) {
        if (isCancelled()) {
            emit message(QStringLiteral("已取消"));
            emit finishedCancelled();
        } else {
            emit errorOccurred(qs(err));
            emit finishedCancelled();
        }
        return;
    }

    RcsCalTifResultQt r;
    r.K = resp.K;
    for (size_t i = 0; i < resp.files.size(); ++i) {
        if (!resp.files[i].tifPath.empty())
            r.tifPaths << qs(resp.files[i].tifPath);
        if (!resp.files[i].txtPath.empty())
            r.txtPaths << qs(resp.files[i].txtPath);
    }
    r.summary = QStringLiteral("K=%1\nTIFF %2 个，三级 txt %3 个")
                    .arg(resp.K, 0, 'e', 6)
                    .arg(r.tifPaths.size())
                    .arg(r.txtPaths.size());
    emit calReady(r);
    emit finishedOk();
}
