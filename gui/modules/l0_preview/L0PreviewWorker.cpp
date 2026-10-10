#include "L0PreviewWorker.h"
#include "common/RcsApiQt.h"

L0PreviewWorker::L0PreviewWorker(QObject *parent) : ProcessWorker(parent) {}

void L0PreviewWorker::startProcess()
{
    clearSelectionWait();
    emit message(QStringLiteral("开始计算原始数据时域/频域预览…"));

    L0PreviewRequest req;
    req.dataFolder = ss(params_.dataFolder);
    req.pol = ss(params_.pol);
    req.fsHz = params_.fsHz;
    req.pulseNo = params_.pulseNo;
    req.nAvg = 1;
    req.removeDC = params_.removeDC;
    req.maxFiles = params_.maxFiles;

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emitProgress(p, qs(m)); };
    cb.onMessage = [this](const std::string &m) { emit message(qs(m)); };
    cb.isCancelled = [this]() { return isCancelled(); };

    L0PreviewResponse resp;
    std::string err;
    if (!previewL0Folder(req, resp, cb, &err)) {
        if (isCancelled()) {
            emit message(QStringLiteral("已取消"));
            emit finishedCancelled();
        } else {
            emit errorOccurred(qs(err));
            emit finishedCancelled();
        }
        return;
    }

    QVector<L0PulsePreviewItem> items;
    items.reserve(static_cast<int>(resp.items.size()));
    for (size_t i = 0; i < resp.items.size(); ++i)
        items.push_back(resp.items[i]);
    emit previewReady(items);
    emit finishedOk();
}
