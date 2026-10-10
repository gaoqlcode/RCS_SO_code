#include "Sigma0Worker.h"
#include "common/RcsApiQt.h"

Sigma0Worker::Sigma0Worker(QObject *parent) : ProcessWorker(parent) {}

void Sigma0Worker::startProcess()
{
    clearSelectionWait();
    emit message(QStringLiteral("开始后向散射系数处理…"));

    Sigma0Request req;
    req.rawPath = ss(params_.rawPath);
    req.outDir = ss(params_.outDir);
    req.tgtName = ss(params_.tgtName);
    req.nr = params_.nr;
    req.na = params_.na;
    req.fcGHz = params_.fcGHz;
    req.vs = params_.vs;
    req.incAngleDeg = params_.incAngleDeg;
    req.hFlight = params_.hFlight;
    req.sigmaTheoryDb = params_.sigmaTheoryDb;
    req.targetType = params_.targetType;
    req.bgMode = params_.bgMode;
    req.measureTime = ss(params_.measureTime);
    req.autoPick = false;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return isCancelled(); };

    cb.onRectRoi = [this](const GrayImage &g, int fullNr, int fullNa, const std::string &stage,
                          RectRoi &out) -> bool {
        emit needRectRoi(grayToQImage(g), fullNr, fullNa, qs(stage));
        emit needUserInput(QStringLiteral("请框选区域：") + qs(stage));
        UserSelection qsel;
        if (!waitForSelection(qsel) || qsel.rectRois.isEmpty())
            return false;
        const QRect &r = qsel.rectRois.first();
        out = RectRoi(r.x(), r.y(), r.width(), r.height());
        return out.valid();
    };

    cb.onTargetRois = [this](const GrayImage &g, int fullNr, int fullNa,
                             std::vector<RectRoi> &outs) -> bool {
        emit needTargetRois(grayToQImage(g), fullNr, fullNa);
        emit needUserInput(QStringLiteral("请框选地物目标（可多个）"));
        UserSelection qsel;
        if (!waitForSelection(qsel) || qsel.rectRois.isEmpty())
            return false;
        outs.clear();
        for (int i = 0; i < qsel.rectRois.size(); ++i) {
            const QRect &r = qsel.rectRois[i];
            outs.push_back(RectRoi(r.x(), r.y(), r.width(), r.height()));
        }
        return !outs.empty();
    };

    Sigma0Response resp;
    std::string err;
    if (!processSigma0(req, resp, cb, &err)) {
        if (isCancelled()) {
            emit message(QStringLiteral("已取消"));
            emit finishedCancelled();
        } else {
            emit errorOccurred(qs(err));
            emit finishedCancelled();
        }
        return;
    }

    Sigma0ResultQt r;
    r.csPath = qs(resp.csPath);
    r.K = resp.K;
    for (size_t i = 0; i < resp.targets.size(); ++i)
        r.sigma0Db.push_back(resp.targets[i].sigma0Db);
    emit sigma0Ready(r);
    emit finishedOk();
}
