#include "ToZeroWorker.h"
#include "common/RcsApiQt.h"

ToZeroWorker::ToZeroWorker(QObject *parent) : ProcessWorker(parent) {}

void ToZeroWorker::startProcess()
{
    clearSelectionWait();
    emit message(QStringLiteral("开始 ToZeroData 转换…"));

    ToZeroDataRequest req;
    req.sourcePath = ss(params_.sourcePath);
    req.outputBasePath = ss(params_.outputBasePath);
    req.saveRecovery = params_.saveRecovery;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return isCancelled(); };

    ToZeroDataResponse resp;
    std::string err;
    if (!processToZeroData(req, resp, cb, &err)) {
        if (isCancelled()) {
            emit message(QStringLiteral("已取消"));
            emit finishedCancelled();
        } else {
            emit errorOccurred(qs(err));
            emit finishedCancelled();
        }
        return;
    }

    ToZeroResultQt r;
    r.validRecords = resp.validRecords;
    r.skippedRecords = resp.skippedRecords;
    r.outputBytes = resp.outputBytes;
    r.recoveryPath = qs(resp.recoveryPath);
    r.sourceSha256 = qs(resp.sourceSha256);
    for (size_t i = 0; i < resp.outputs.size(); ++i) {
        const ToZeroDataPolarizationOut &o = resp.outputs[i];
        r.lines << QStringLiteral("%1  %2  %3帧  %4字节")
                       .arg(qs(o.polarization))
                       .arg(qs(o.path))
                       .arg(o.frames)
                       .arg(o.bytes);
    }
    r.summary = QStringLiteral("有效记录=%1  跳过=%2  输出字节=%3")
                    .arg(r.validRecords)
                    .arg(r.skippedRecords)
                    .arg(r.outputBytes);
    emit toZeroReady(r);
    emit finishedOk();
}
