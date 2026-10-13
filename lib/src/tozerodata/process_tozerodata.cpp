#include "rcs/rcs_api.h"

#include "tozerodata.h"

#include <array>
#include <vector>

namespace {

std::string stageName(ToZeroData::ConversionStage s)
{
    switch (s) {
    case ToZeroData::ConversionStage::Scanning:
        return "扫描源数据";
    case ToZeroData::ConversionStage::Converting:
        return "转换中";
    case ToZeroData::ConversionStage::Committing:
        return "提交输出";
    case ToZeroData::ConversionStage::SavingRecovery:
        return "写还原信息";
    }
    return "处理中";
}

} // namespace

bool processToZeroData(const ToZeroDataRequest &req, ToZeroDataResponse &resp, Callbacks &cb,
                       std::string *err)
{
    resp = ToZeroDataResponse();
    if (req.sourcePath.empty() || req.outputBasePath.empty()) {
        if (err)
            *err = "sourcePath / outputBasePath 不能为空";
        return false;
    }

    ToZeroData::ConversionOptions opt;
    opt.sourcePath = req.sourcePath;
    opt.outputBasePath = req.outputBasePath;
    opt.saveRecovery = req.saveRecovery;

    ToZeroData::ConversionCallbacks tcb;
    tcb.stage = [&](ToZeroData::ConversionStage s) {
        if (cb.onMessage)
            cb.onMessage(stageName(s));
    };
    tcb.progress = [&](int p) {
        if (cb.onProgress)
            cb.onProgress(p, stageName(ToZeroData::ConversionStage::Converting));
    };
    tcb.log = [&](const std::string &m) {
        if (cb.onMessage)
            cb.onMessage(m);
    };
    tcb.shouldCancel = [&]() { return cb.isCancelled && cb.isCancelled(); };

    ToZeroData::Converter converter;
    const ToZeroData::ConversionResult r = converter.convert(opt, tcb);

    if (r.status == ToZeroData::ConversionStatus::Canceled) {
        if (err)
            *err = r.error.empty() ? "已取消" : r.error;
        return false;
    }
    if (r.status != ToZeroData::ConversionStatus::Success) {
        if (cb.onError && !r.error.empty())
            cb.onError(r.error);
        if (err)
            *err = r.error.empty() ? "ToZeroData 转换失败" : r.error;
        return false;
    }

    resp.validRecords = r.validRecords;
    resp.skippedRecords = r.skippedRecords;
    resp.outputBytes = r.outputBytes;
    resp.recoveryPath = r.recoveryPath;
    resp.recoveryBytes = r.recoveryBytes;
    resp.sourceSha256 = r.sourceSha256;
    resp.outputs.clear();
    resp.outputs.reserve(r.outputs.size());
    for (size_t i = 0; i < r.outputs.size(); ++i) {
        ToZeroDataPolarizationOut o;
        o.polarization = r.outputs[i].polarization;
        o.path = r.outputs[i].path;
        o.frames = r.outputs[i].frames;
        o.bytes = r.outputs[i].bytes;
        resp.outputs.push_back(o);
    }
    if (cb.onProgress)
        cb.onProgress(100, "ToZeroData 完成");
    return true;
}

std::vector<std::string> toZeroDataOutputPaths(const std::string &outputBasePath)
{
    const std::array<std::string, 4> a =
        ToZeroData::Converter::outputPathsForBasePath(outputBasePath);
    return std::vector<std::string>(a.begin(), a.end());
}

std::string toZeroDataRecoveryPath(const std::string &outputBasePath)
{
    return ToZeroData::Converter::recoveryPathForBasePath(outputBasePath);
}
