#include "HrrpWorker.h"
#include "common/RcsApiQt.h"

HrrpWorker::HrrpWorker(QObject *parent) : ProcessWorker(parent) {}

void HrrpWorker::startProcess()
{
    clearSelectionWait();
    emit message(QStringLiteral("开始 HRRP 处理…"));

    HrrpRequest req;
    req.dataFolder = ss(params_.dataFolder);
    req.pol = ss(params_.pol);
    req.tgtName = ss(params_.tgtName);
    req.outDir = ss(params_.outDir);
    req.sigmaTheoryDb = params_.sigmaTheoryDb;
    req.cropN = params_.cropN;
    req.phaseMode = params_.phaseMode;
    req.autoPick = false;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return isCancelled(); };
    cb.onCornerConfirm = [this](const std::vector<double> &R, const std::vector<double> &profileDb,
                                int peakIdx, InteractSelection &sel) {
        emit needCornerConfirm(toQV(R), toQV(profileDb), peakIdx);
        emit needUserInput(QStringLiteral("请核对角反峰值距离"));
        UserSelection qsel;
        if (!waitForSelection(qsel))
            return false;
        sel = toLibSel(qsel);
        return true;
    };

    HrrpResponse resp;
    std::string err;
    if (!processHrrp(req, resp, cb, &err)) {
        if (isCancelled()) {
            emit message(QStringLiteral("已取消"));
            emit finishedCancelled();
        } else {
            emit errorOccurred(qs(err));
            emit finishedCancelled();
        }
        return;
    }

    HrrpResult result;
    result.rangeM = toQV(resp.rangeM);
    result.rcsDbsm = toQV(resp.rcsDbsm);
    result.phaseDeg = toQV(resp.phaseDeg);
    result.hrrpPath = qs(resp.hrrpPath);
    result.pdatPath = qs(resp.pdatPath);
    emit hrrpReady(result);
    emit finishedOk();
}
