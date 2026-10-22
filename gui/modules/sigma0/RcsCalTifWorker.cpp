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
    req.l0HeaderPath = ss(params_.l0HeaderPath);
    req.mergeTxtPath = ss(params_.mergeTxtPath);
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

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return isCancelled(); };
    cb.onRectRoi = [this](const GrayImage &g, int fullNr, int fullNa, const std::string &stage,
                          RectRoi &out) -> bool {
        emit needRectRoi(grayToQImage(g), fullNr, fullNa, qs(stage));
        emit needUserInput(stage == "target" ? QStringLiteral("请框选目标")
                                             : QStringLiteral("请框选角反缓冲区"));
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
        if (!resp.files[i].pdatPath.empty())
            r.pdatPaths << qs(resp.files[i].pdatPath);
        if (!resp.files[i].positionPath.empty())
            r.positionPaths << qs(resp.files[i].positionPath);
        if (!resp.files[i].note.empty())
            emit message(qs(resp.files[i].rawName) + QStringLiteral(": ") + qs(resp.files[i].note));
    }
    r.summary = QStringLiteral("K=%1\nTIFF %2 个，三级 txt %3 个，二级 pdat %4 个，目标位置 %5 个")
                    .arg(resp.K, 0, 'e', 6)
                    .arg(r.tifPaths.size())
                    .arg(r.txtPaths.size())
                    .arg(r.pdatPaths.size())
                    .arg(r.positionPaths.size());
    for (int i = 0; i < r.txtPaths.size(); ++i)
        r.summary += QStringLiteral("\n三级: ") + r.txtPaths.at(i);
    for (int i = 0; i < r.pdatPaths.size(); ++i)
        r.summary += QStringLiteral("\n二级: ") + r.pdatPaths.at(i);
    for (int i = 0; i < r.positionPaths.size(); ++i)
        r.summary += QStringLiteral("\n目标位置: ") + r.positionPaths.at(i);
    emit calReady(r);
    emit finishedOk();
}
