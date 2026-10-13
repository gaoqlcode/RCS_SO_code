// =============================================================================
// DemoWorker.cpp — 在工作线程里组装 Request/Callbacks 并调用 SDK
// =============================================================================

#include "DemoWorker.h"    // 本类 + PreviewBundle
#include "ConfirmBridge.h" // askCornerConfirm / lastRangeM
#include "rcs/rcs.hpp"     // 一次引入 types / callbacks / rcs_api

#include <QByteArray>   // QString::toUtf8 的返回类型
#include <QMetaObject>  // invokeMethod：跨线程调 Bridge 槽
#include <QVector>      // copyVec 输出类型

namespace {

// QString（Qt UTF-16）→ std::string（SDK 约定 UTF-8 字节）
std::string toUtf8(const QString &s)
{
    // toUtf8() 得到 UTF-8 字节数组（含 '\0' 结尾，但 size 不含也可）
    const QByteArray u = s.toUtf8();
    // 用指针+长度构造，避免中间经过本地 8bit 编码
    return std::string(u.constData(), static_cast<size_t>(u.size()));
}

// SDK 的 UTF-8 std::string → QString，供界面显示
QString fromUtf8(const std::string &s)
{
    // 按字节长度解释为 UTF-8，不要用 fromStdString（可能按 locale）
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

// std::vector<double> → QVector<double>，才能方便地进 Qt 信号/绑图控件
QVector<double> copyVec(const std::vector<double> &v)
{
    QVector<double> o;                              // 空向量
    o.reserve(static_cast<int>(v.size()));          // 预分配，减少扩容
    for (size_t i = 0; i < v.size(); ++i)           // 逐个拷贝
        o.push_back(v[i]);                          // 追加一个采样
    return o;                                       // 返回可拷贝的 Qt 容器
}

} // namespace

// 构造：取消标志清 false；桥指针先空，等 MainWindow setConfirmBridge
DemoWorker::DemoWorker(QObject *parent) : QObject(parent), cancel_(false), bridge_(0) {}

// ---------- 探查：无 Callbacks，同步 API ----------
void DemoWorker::runProbe(const QString &folder, const QString &pol)
{
    cancel_ = false; // 新任务开始，清掉上一次的取消状态

    L0FolderProbe p; // SDK 输出结构：文件数、首末名、首文件头等
    std::string err; // 失败时可读说明

    // 调 SDK：扫描 folder 下该极化的 L0，填 p；失败返回 false
    if (!probeDataFolder(toUtf8(folder), toUtf8(pol), p, &err)) {
        emit failed(fromUtf8(err)); // 通知 UI 弹错
        emit finished();            // 无论成败都要结束 busy 状态
        return;                     // 不再往下拼文字
    }

    QString text; // 拼给人看的多行摘要
    text += QStringLiteral("极化：%1\n").arg(fromUtf8(p.pol));           // 实际极化
    text += QStringLiteral("文件数：%1\n").arg(p.fileCount);             // 匹配到的文件个数
    text += QStringLiteral("第一个：%1\n").arg(fromUtf8(p.firstFile));   // 排序后首文件
    text += QStringLiteral("最后一个：%1\n").arg(fromUtf8(p.lastFile)); // 排序后末文件
    if (p.hasHeader) { // 成功解析过首文件 512B 头才显示频率带宽
        text += QStringLiteral("中心频率：%1 GHz\n").arg(p.firstHeader.fcGHz, 0, 'f', 4);
        text += QStringLiteral("带宽：%1 MHz\n").arg(p.firstHeader.bwMHz);
    }

    emit probeDone(text); // UI：infoLabel_->setText
    emit finished();      // UI：setBusy(false)
}

// ---------- 0 级预览：带进度/取消 ----------
void DemoWorker::runPreview(const QString &folder, const QString &pol)
{
    cancel_ = false; // 新任务

    L0PreviewRequest req;              // SDK 预览请求
    req.dataFolder = toUtf8(folder);   // 数据目录 UTF-8
    req.pol = toUtf8(pol);             // 极化，如 "HH"
    req.maxFiles = 8;                  // demo 只算前 8 个，避免目录很大时卡太久

    Callbacks cb; // 回调集合；不用的成员保持空即可

    // 进度：在库线程里被调用 → 只 emit，由 Qt 排队到 UI 改进度条/日志
    cb.onProgress = [this](int p, const std::string &m) { emit progress(p, fromUtf8(m)); };

    // 一般消息日志
    cb.onMessage = [this](const std::string &m) { emit logMessage(fromUtf8(m)); };

    // 取消：库循环里查询；UI 调 requestCancel() 后这里读到 true
    cb.isCancelled = [this]() { return cancel_.load(); };

    L0PreviewResponse resp; // 输出：每个文件一条时域+频域曲线
    std::string err;        // 错误说明

    // 调 SDK 文件夹预览；失败或取消 → false
    if (!previewL0Folder(req, resp, cb, &err)) {
        emit failed(fromUtf8(err));
        emit finished();
        return;
    }

    // 把 SDK 的 C++ 结构转成可进信号的 PreviewBundle 列表
    QVector<PreviewBundle> items;
    for (size_t i = 0; i < resp.items.size(); ++i) {
        const L0PulsePreviewItem &src = resp.items[i]; // 当前文件预览
        PreviewBundle one;                              // 待填充的一条
        one.fileName = fromUtf8(src.fileName);          // 文件名
        one.tUs = copyVec(src.tUs);                     // 时间轴
        one.amp = copyVec(src.amp);                     // 幅度
        one.fMHz = copyVec(src.fMHz);                   // 频率轴
        one.spectrumDb = copyVec(src.spectrumDb);       // 频谱 dB
        one.ampPkUs = src.ampPkUs;                      // 时域峰位置
        one.fPkMHz = src.fPkMHz;                        // 频域峰位置
        items.push_back(one);                           // 加入列表
    }

    emit previewDone(items); // UI 缓存并 showPreviewIndex(0)
    emit finished();
}

// ---------- HRRP：带 BlockingQueued 确认峰距 ----------
void DemoWorker::runHrrp(const QString &folder, const QString &outDir, const QString &pol)
{
    cancel_ = false;

    HrrpRequest req;                 // HRRP 请求（autoPick 默认 false）
    req.dataFolder = toUtf8(folder); // 输入：含 *_L0_<pol>.dat 的目录
    req.outDir = toUtf8(outDir);     // 输出目录；空则库建默认名
    req.pol = toUtf8(pol);           // 极化

    Callbacks cb;
    cb.onProgress = [this](int p, const std::string &m) { emit progress(p, fromUtf8(m)); };
    cb.onMessage = [this](const std::string &m) { emit logMessage(fromUtf8(m)); };
    cb.isCancelled = [this]() { return cancel_.load(); };

    // 库算到角反峰时同步调用本 lambda（仍在 Worker 线程）
    cb.onCornerConfirm = [this](const std::vector<double> &R, const std::vector<double> & /*db*/,
                                int peakIdx, InteractSelection &sel) -> bool {
        // 没有桥就无法弹窗，直接失败
        if (!bridge_)
            return false;

        // 用自动峰下标取建议距离；下标非法则给 0
        const double suggest =
            (peakIdx >= 0 && peakIdx < static_cast<int>(R.size())) ? R[static_cast<size_t>(peakIdx)]
                                                                   : 0.0;

        bool accepted = false; // 接收槽的返回值：用户是否确定

        // BlockingQueuedConnection：
        //   把调用投递到 bridge_ 所在线程（UI），当前 Worker 线程阻塞等待；
        //   UI 跑完 askCornerConfirm 后，accepted 被填好，Worker 再继续。
        const bool invoked = QMetaObject::invokeMethod(
            bridge_, "askCornerConfirm", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, accepted),
            Q_ARG(double, suggest));

        // invoke 失败，或用户取消对话框 → 告诉库中止
        if (!invoked || !accepted)
            return false;

        // 把用户确认的距离写进选点结构；库用它覆盖自动峰（>=0 有效）
        sel.cornerRangeOverrideM = bridge_->lastRangeM();
        return true; // 继续处理
    };

    HrrpResponse resp; // 曲线 + 落盘路径
    std::string err;

    // 同步跑完整 HRRP；内部会回调 onCornerConfirm
    if (!processHrrp(req, resp, cb, &err)) {
        emit failed(fromUtf8(err));
        emit finished();
        return;
    }

    // 成功：路径 + 距离轴 + RCS 曲线给 UI 画图
    emit hrrpDone(fromUtf8(resp.hrrpPath), copyVec(resp.rangeM), copyVec(resp.rcsDbsm));
    emit finished();
}
