#include "MosaicWorker.h"

MosaicWorker::MosaicWorker(QObject *parent) : ProcessWorker(parent) {}

void MosaicWorker::startProcess()
{
    clearSelectionWait();
    emit message(params_.mode == 1 ? QStringLiteral("按手动摆位融合…")
                                   : QStringLiteral("开始自动拼接…"));

    MosaicRequest req;
    req.inFolder = ss(params_.inFolder);
    req.outFolder = ss(params_.outFolder);
    req.doFlatten = params_.doFlatten;
    req.medOut = params_.medOut;
    req.mode = params_.mode;
    req.rawNrHint = params_.rawNrHint;
    req.rawNaHint = params_.rawNaHint;
    for (int i = 0; i < params_.offR.size(); ++i)
        req.offR.push_back(params_.offR[i]);
    for (int i = 0; i < params_.offC.size(); ++i)
        req.offC.push_back(params_.offC[i]);

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &s) {
        emitProgress(p, qs(s));
        emit message(qs(s));
    };
    cb.isCancelled = [this]() { return isCancelled(); };

    MosaicResponse resp;
    std::string err;
    if (!processMosaic(req, resp, cb, &err)) {
        if (isCancelled())
            emit finishedCancelled();
        else {
            emit errorOccurred(qs(err));
            emit finishedCancelled();
        }
        return;
    }
    emit mosaicReady(toQtMosaic(resp));
    emit finishedOk();
}

void MosaicWorker::startFlattenOnly(const QString &tifPath, double medOut)
{
    clearSelectionWait();
    const QString out = tifPath.left(tifPath.lastIndexOf('.')) + QStringLiteral("_匀光.tif");
    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &s) { emitProgress(p, qs(s)); };
    cb.isCancelled = [this]() { return isCancelled(); };
    std::string err;
    if (!flattenMosaic(ss(tifPath), ss(out), medOut, std::vector<int>(), cb, &err)) {
        emit errorOccurred(qs(err));
        emit finishedCancelled();
        return;
    }
    MosaicResult result;
    result.mosaicTif = tifPath;
    result.flattenTif = out;
    emit mosaicReady(result);
    emit finishedOk();
}
