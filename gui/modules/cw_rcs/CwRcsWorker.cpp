#include "CwRcsWorker.h"
#include "common/RcsApiQt.h"

CwRcsWorker::CwRcsWorker(QObject *parent) : ProcessWorker(parent) {}

void CwRcsWorker::startProcess()
{
    clearSelectionWait();
    emit message(QStringLiteral("开始点频 RCS 处理…"));

    CwRcsRequest req;
    req.dataFolder = ss(params_.dataFolder);
    req.pol = ss(params_.pol);
    req.tgtName = ss(params_.tgtName);
    req.outDir = ss(params_.outDir);
    req.sigmaTheoryDb = params_.sigmaTheoryDb;
    req.targetMode = params_.targetMode;
    req.autoPick = false;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return isCancelled(); };

    cb.onCornerConfirm = [this](const std::vector<double> &R, const std::vector<double> &profileDb,
                                int peakIdx, InteractSelection &sel) {
        emit needCornerConfirm(toQV(R), toQV(profileDb), peakIdx);
        emit needUserInput(QStringLiteral("请核对角反峰值"));
        UserSelection qsel;
        if (!waitForSelection(qsel))
            return false;
        sel = toLibSel(qsel);
        return true;
    };

    cb.onTargetPick = [this](const std::vector<double> &R, const GrayImage &preview, int fullNr,
                             int fullNp, const std::vector<double> &meanPowerDb, int modeHint,
                             InteractSelection &sel) {
        emit needTargetPick(toQV(R), grayToQImage(preview), fullNr, fullNp, toQV(meanPowerDb), modeHint);
        emit needUserInput(QStringLiteral("请选择目标模式并选点/输入距离段"));
        UserSelection qsel;
        if (!waitForSelection(qsel))
            return false;
        sel = toLibSel(qsel);
        return true;
    };

    CwRcsResponse resp;
    std::string err;
    if (!processCwRcs(req, resp, cb, &err)) {
        if (isCancelled()) {
            emit message(QStringLiteral("已取消"));
            emit finishedCancelled();
        } else {
            emit errorOccurred(qs(err));
            emit finishedCancelled();
        }
        return;
    }

    CwRcsResultQt out;
    out.freqGHz = toQV(resp.freqGHz);
    out.rcsDbsm = toQV(resp.rcsDbsm);
    out.l3Path = qs(resp.l3Path);
    emit cwRcsReady(out);
    emit finishedOk();
}
