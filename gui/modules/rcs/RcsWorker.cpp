#include "RcsWorker.h"
#include "common/RcsApiQt.h"
#include <exception>

RcsWorker::RcsWorker(QObject *parent) : ProcessWorker(parent) {}

void RcsWorker::startProcess()
{
    clearSelectionWait();
    emit message(QStringLiteral("开始 RCS 处理…"));

    try {
        RcsRequest req;
        req.dataFolder = ss(params_.dataFolder);
        req.pol = ss(params_.pol);
        req.tgtName = ss(params_.tgtName);
        req.outDir = ss(params_.outDir);
        req.sigmaTheoryDb = params_.sigmaTheoryDb;
        req.cropN = params_.cropN;
        req.phaseMode = params_.phaseMode;
        req.azMode = params_.azMode;
        req.vSar = params_.vSar;
        req.autoPick = false; // 界面里要人点角反/目标

        Callbacks cb;
        cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
        cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
        cb.isCancelled = [this]() { return isCancelled(); };

        cb.onCornerPick = [this](const std::vector<double> &R, const GrayImage &preview, int fullNr,
                                 int fullNp, InteractSelection &sel) {
            emit needCornerPick(toQV(R), grayToQImage(preview), fullNr, fullNp);
            emit needUserInput(QStringLiteral("请选择角反点"));
            UserSelection qsel;
            if (!waitForSelection(qsel) || qsel.corners.isEmpty())
                return false;
            sel = toLibSel(qsel);
            return true;
        };

        cb.onTargetPick = [this](const std::vector<double> &R, const GrayImage &preview, int fullNr,
                                 int fullNp, const std::vector<double> &meanPowerDb, int modeHint,
                                 InteractSelection &sel) {
            emit needTargetPick(toQV(R), grayToQImage(preview), fullNr, fullNp, toQV(meanPowerDb),
                                modeHint);
            emit needUserInput(QStringLiteral("请选择目标模式并选点/输入距离段"));
            UserSelection qsel;
            if (!waitForSelection(qsel))
                return false;
            sel = toLibSel(qsel);
            return true;
        };

        RcsResponse resp;
        std::string err;
        if (!processRcs(req, resp, cb, &err)) {
            if (isCancelled()) {
                emit message(QStringLiteral("已取消"));
                emit finishedCancelled();
            } else {
                emit errorOccurred(qs(err));
                emit finishedCancelled();
            }
            return;
        }

        RcsResultBundle bundle;
        bundle.pdatPath = qs(resp.pdatPath);
        bundle.rcsPath = qs(resp.rcsPath);
        bundle.rangeM = toQV(resp.rangeM);
        bundle.profileDbsm = toQV(resp.profileDbsm);
        for (size_t i = 0; i < resp.targets.size(); ++i) {
            const RcsTargetItem &t = resp.targets[i];
            RcsTargetResult qt;
            qt.rcsDbsm = t.rcsDbsm;
            qt.azViewDeg = t.azViewDeg;
            qt.rangeM = t.rangeM;
            qt.valid = t.valid;
            qt.label = qs(t.label);
            bundle.targets.push_back(qt);
        }
        emit rcsReady(bundle);
        emit finishedOk();
    } catch (const std::exception &ex) {
        emit errorOccurred(QStringLiteral("处理异常: %1").arg(QString::fromUtf8(ex.what())));
        emit finishedCancelled();
    } catch (...) {
        emit errorOccurred(QStringLiteral("处理异常（未知）"));
        emit finishedCancelled();
    }
}
