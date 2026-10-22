#include "ToZeroWorker.h"
#include "common/RcsApiQt.h"

ToZeroWorker::ToZeroWorker(QObject *parent) : ProcessWorker(parent) {}

void ToZeroWorker::startProcess()
{
    clearSelectionWait();
    emit message(params_.kind == 1 ? QStringLiteral("开始：源 DAT 转四极化 L0，然后 ISAR 成像…")
                                  : QStringLiteral("开始：源 DAT 转四极化 L0，然后 SAR 成像…"));

    ZeroImageRequest req;
    req.sourcePath = ss(params_.sourcePath);
    req.outputBasePath = ss(params_.outputBasePath);
    req.inputDirectory = ss(params_.inputDirectory);
    req.parameters.beginId = params_.beginId;
    req.parameters.deltaNum = params_.deltaNum;
    req.parameters.lastFileId = params_.lastFileId;
    req.parameters.rangeOffset = params_.rangeOffset;
    req.parameters.fileNum = params_.fileNum;
    req.parameters.rangePoints = params_.rangePoints;
    req.kind = params_.kind;
    req.engineDirectory = ss(params_.engineDirectory);
    req.timeoutMs = params_.timeoutSec > 0 ? params_.timeoutSec * 1000 : 0;
    req.requireEngine = params_.requireEngine;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return isCancelled(); };

    ZeroImageResponse resp;
    std::string err;
    if (!processZeroImage(req, resp, cb, &err)) {
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
    r.exitCode = resp.exitCode;
    r.restoredBytes = resp.restoredBytes;
    r.recordPath = qs(resp.recordPath);
    for (size_t i = 0; i < resp.l0Outputs.size(); ++i) {
        const ToZeroDataPolarizationOut &o = resp.l0Outputs[i];
        r.lines << QStringLiteral("L0 %1  %2  %3帧  %4字节")
                       .arg(qs(o.polarization))
                       .arg(qs(o.path))
                       .arg(o.frames)
                       .arg(o.bytes);
    }
    if (!resp.recoveryPath.empty())
        r.lines << QStringLiteral("还原 %1").arg(qs(resp.recoveryPath));
    for (size_t i = 0; i < resp.publishedFiles.size(); ++i) {
        const ZeroImagePublishedFile &f = resp.publishedFiles[i];
        r.lines << QStringLiteral("图像 %1  %2 字节").arg(qs(f.path)).arg(f.bytes);
    }
    for (size_t i = 0; i < resp.warnings.size(); ++i)
        r.warnings << qs(resp.warnings[i]);
    r.summary = QStringLiteral("L0 有效=%1  跳过=%2  成像退出码=%3  图像=%4")
                    .arg(resp.validRecords)
                    .arg(resp.skippedRecords)
                    .arg(r.exitCode)
                    .arg(resp.publishedFiles.size());
    emit toZeroReady(r);
    emit finishedOk();
}
