#include "rcs/rcs_api.h"

#include "zerodataimagehandle.h"

#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <string>

namespace {

bool isDir(const std::string &path)
{
    struct stat st;
    return !path.empty() && ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string soDirectory()
{
    Dl_info info;
    if (!dladdr(reinterpret_cast<void *>(&processZeroImage), &info) || !info.dli_fname)
        return std::string();
    std::string path = info.dli_fname;
    char resolved[4096];
    if (::realpath(path.c_str(), resolved))
        path = resolved;
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return std::string();
    return path.substr(0, slash);
}

std::string defaultEngineDirectory()
{
    if (const char *env = std::getenv("RCS_ZDI_ENGINE_DIR")) {
        if (isDir(env))
            return env;
    }
    const std::string libDir = soDirectory();
    if (!libDir.empty()) {
        const std::string beside = libDir + "/../engine/for_testing";
        char resolved[4096];
        if (::realpath(beside.c_str(), resolved) && isDir(resolved))
            return resolved;
    }
#ifdef RCS_ZDI_ENGINE_DIR
    if (isDir(RCS_ZDI_ENGINE_DIR))
        return RCS_ZDI_ENGINE_DIR;
#endif
    return std::string();
}

std::string stageName(ZeroDataImageHandle::ProcessStage stage)
{
    switch (stage) {
    case ZeroDataImageHandle::ProcessStage::Preparing:
        return "准备数据";
    case ZeroDataImageHandle::ProcessStage::Restoring:
        return "还原原始数据";
    case ZeroDataImageHandle::ProcessStage::Processing:
        return "调用后处理程序";
    case ZeroDataImageHandle::ProcessStage::Publishing:
        return "保存结果";
    case ZeroDataImageHandle::ProcessStage::Cleaning:
        return "清理任务";
    }
    return "处理中";
}

std::string parentDirectory(const std::string &path)
{
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return std::string();
    if (slash == 0)
        return "/";
    return path.substr(0, slash);
}

std::string joinPath(const std::string &dir, const std::string &name)
{
    if (dir.empty() || dir[dir.size() - 1] == '/')
        return dir + name;
    return dir + "/" + name;
}

bool hasSuffixI(const std::string &s, const std::string &suffix)
{
    if (s.size() < suffix.size())
        return false;
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(s[s.size() - suffix.size() + i]);
        const unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(a) != std::tolower(b))
            return false;
    }
    return true;
}

std::string fileStem(const std::string &name)
{
    const std::size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0)
        return name;
    return name.substr(0, dot);
}

bool isPolarizedL0Name(const std::string &name)
{
    const std::string stem = fileStem(name);
    return hasSuffixI(stem, "_HH") || hasSuffixI(stem, "_VV") || hasSuffixI(stem, "_HV")
           || hasSuffixI(stem, "_VH");
}

bool listSourceDats(const std::string &dir, std::vector<std::string> &names, std::string *err)
{
    names.clear();
    if (!isDir(dir)) {
        if (err)
            *err = "源目录不存在: " + dir;
        return false;
    }
    DIR *dp = ::opendir(dir.c_str());
    if (!dp) {
        if (err)
            *err = "无法读取源目录: " + dir;
        return false;
    }
    while (dirent *ent = ::readdir(dp)) {
        const std::string name = ent->d_name;
        if (name == "." || name == "..")
            continue;
        if (!hasSuffixI(name, ".dat") || isPolarizedL0Name(name))
            continue;
        const std::string full = joinPath(dir, name);
        struct stat st;
        if (::stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        names.push_back(name);
    }
    ::closedir(dp);
    std::sort(names.begin(), names.end());
    if (names.empty()) {
        if (err)
            *err = "源目录中没有可转换的 DAT: " + dir;
        return false;
    }
    return true;
}

void copyImageResult(const ZeroImageResponse &image, ZeroImageFolderResponse &resp)
{
    resp.exitCode = image.exitCode;
    resp.restoredBytes = image.restoredBytes;
    resp.inputs = image.inputs;
    resp.publishedFiles = image.publishedFiles;
    resp.warnings = image.warnings;
    resp.jobId = image.jobId;
    resp.recordPath = image.recordPath;
    resp.configPath = image.configPath;
    resp.logPath = image.logPath;
}

} // namespace

bool processZeroImage(const ZeroImageRequest &req, ZeroImageResponse &resp, Callbacks &cb,
                      std::string *err)
{
    resp = ZeroImageResponse();
    std::string imageDir = req.inputDirectory;
    const bool convertFirst = !req.sourcePath.empty() || !req.outputBasePath.empty();
    if (convertFirst) {
        if (req.sourcePath.empty() || req.outputBasePath.empty()) {
            if (err)
                *err = "sourcePath 与 outputBasePath 需同时填写";
            return false;
        }
        ToZeroDataRequest treq;
        treq.sourcePath = req.sourcePath;
        treq.outputBasePath = req.outputBasePath;
        treq.saveRecovery = true;

        Callbacks tcb;
        tcb.onMessage = cb.onMessage;
        tcb.onError = cb.onError;
        tcb.isCancelled = cb.isCancelled;
        tcb.onProgress = [&](int p, const std::string &m) {
            if (cb.onProgress)
                cb.onProgress(p / 2, m);
        };
        ToZeroDataResponse tresp;
        if (!processToZeroData(treq, tresp, tcb, err))
            return false;

        resp.validRecords = tresp.validRecords;
        resp.skippedRecords = tresp.skippedRecords;
        resp.l0Bytes = tresp.outputBytes;
        resp.l0Outputs = tresp.outputs;
        resp.recoveryPath = tresp.recoveryPath;
        resp.recoveryBytes = tresp.recoveryBytes;
        resp.sourceSha256 = tresp.sourceSha256;
        if (imageDir.empty()) {
            const std::string sample = !tresp.recoveryPath.empty()
                                           ? tresp.recoveryPath
                                           : (tresp.outputs.empty() ? std::string() : tresp.outputs[0].path);
            imageDir = parentDirectory(sample);
        }
        if (cb.onMessage)
            cb.onMessage("四极化 L0 已生成，开始成像");
    }
    if (imageDir.empty()) {
        if (err)
            *err = "请提供源 DAT 与输出基路径";
        return false;
    }

    ZeroDataImageHandle::ProcessingOptions opt;
    opt.kind = (req.kind == 1) ? ZeroDataImageHandle::ProcessKind::Isar
                               : ZeroDataImageHandle::ProcessKind::Sar;
    opt.inputDirectory = imageDir;
    opt.parameters.fileDir = imageDir;
    opt.parameters.beginId = req.parameters.beginId;
    opt.parameters.deltaNum = req.parameters.deltaNum;
    opt.parameters.lastFileId = req.parameters.lastFileId;
    opt.parameters.rangeOffset = req.parameters.rangeOffset;
    opt.parameters.fileNum = req.parameters.fileNum;
    opt.parameters.testMode = req.parameters.testMode;
    opt.parameters.aperLen = req.parameters.aperLen;
    opt.parameters.tarPosY = req.parameters.tarPosY;
    opt.parameters.iqOrderBand1 = req.parameters.iqOrderBand1;
    opt.parameters.iqOrderBand2 = req.parameters.iqOrderBand2;
    opt.parameters.rangePoints = req.parameters.rangePoints;
    opt.parameters.errEstStart = req.parameters.errEstStart;
    opt.parameters.errEstLength = req.parameters.errEstLength;
    opt.requireEngine = req.requireEngine;
    opt.keepWorkspace = req.keepWorkspace;

    if (!req.engineDirectory.empty()) {
        if (!isDir(req.engineDirectory)) {
            if (err)
                *err = "引擎目录不存在: " + req.engineDirectory;
            return false;
        }
        opt.engine.engineDirectory = req.engineDirectory;
    } else if (req.requireEngine) {
        opt.engine.engineDirectory = defaultEngineDirectory();
        if (opt.engine.engineDirectory.empty()) {
            if (err)
                *err = "未找到 SAR 后处理引擎。请设置 engineDirectory，或把 engine/for_testing 放在库旁，"
                       "或设置环境变量 RCS_ZDI_ENGINE_DIR";
            return false;
        }
    }
    if (!req.engineExecutable.empty())
        opt.engine.executable = req.engineExecutable;
    if (!req.mcrRoot.empty())
        opt.engine.mcrRoot = req.mcrRoot;
    if (!req.mcrCacheRoot.empty())
        opt.engine.mcrCacheRoot = req.mcrCacheRoot;
    opt.engine.timeoutMs = req.timeoutMs;

    ZeroDataImageHandle::ProcessingCallbacks tcb;
    tcb.stage = [&](ZeroDataImageHandle::ProcessStage s) {
        if (cb.onMessage)
            cb.onMessage(stageName(s));
    };
    tcb.progress = [&](int p) {
        if (cb.onProgress) {
            const int shown = convertFirst ? 50 + p / 2 : p;
            cb.onProgress(shown, stageName(ZeroDataImageHandle::ProcessStage::Processing));
        }
    };
    tcb.log = [&](const std::string &m) {
        if (cb.onMessage)
            cb.onMessage(m);
    };
    tcb.shouldCancel = [&]() { return cb.isCancelled && cb.isCancelled(); };

    ZeroDataImageHandle::Processor processor;
    const ZeroDataImageHandle::ProcessingResult r = processor.process(opt, tcb);

    resp.exitCode = r.exitCode;
    resp.restoredBytes = r.restoredBytes;
    resp.warnings = r.warnings;
    resp.jobId = r.jobId;
    resp.recordPath = r.recordPath;
    resp.configPath = r.configPath;
    resp.logPath = r.logPath;
    resp.inputs.reserve(r.inputs.size());
    for (std::size_t i = 0; i < r.inputs.size(); ++i) {
        ZeroImageRestoredInput in;
        in.sidecar = r.inputs[i].sidecar;
        in.originalName = r.inputs[i].originalName;
        in.sourceSha256 = r.inputs[i].sourceSha256;
        in.originalSize = r.inputs[i].originalSize;
        resp.inputs.push_back(in);
    }
    resp.publishedFiles.reserve(r.publishedFiles.size());
    for (std::size_t i = 0; i < r.publishedFiles.size(); ++i) {
        ZeroImagePublishedFile f;
        f.path = r.publishedFiles[i].path;
        f.suffix = r.publishedFiles[i].suffix;
        f.bytes = r.publishedFiles[i].bytes;
        resp.publishedFiles.push_back(f);
    }

    if (r.status == ZeroDataImageHandle::ProcessStatus::Canceled) {
        if (err)
            *err = r.error.empty() ? "已取消" : r.error;
        return false;
    }
    if (!r.ok()) {
        if (cb.onError && !r.error.empty())
            cb.onError(r.error);
        if (err)
            *err = r.error.empty() ? "零级成像失败" : r.error;
        return false;
    }
    if (cb.onProgress)
        cb.onProgress(100, "零级成像完成");
    return true;
}

bool processZeroImageFolder(const ZeroImageFolderRequest &req, ZeroImageFolderResponse &resp,
                            Callbacks &cb, std::string *err)
{
    resp = ZeroImageFolderResponse();
    if (req.sourceDirectory.empty() || req.outputDirectory.empty()) {
        if (err)
            *err = "sourceDirectory / outputDirectory 不能为空";
        return false;
    }
    if (!req.outputDirectory.empty()) {
        struct stat st;
        if (::stat(req.outputDirectory.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) {
            if (err)
                *err = "输出路径不是目录: " + req.outputDirectory;
            return false;
        }
    }

    std::vector<std::string> names;
    if (!listSourceDats(req.sourceDirectory, names, err))
        return false;

    std::set<std::string> bases;
    const int total = static_cast<int>(names.size());
    for (int i = 0; i < total; ++i) {
        if (cb.isCancelled && cb.isCancelled()) {
            if (err)
                *err = "已取消";
            return false;
        }
        const std::string sourcePath = joinPath(req.sourceDirectory, names[static_cast<std::size_t>(i)]);
        const std::string outputBase = joinPath(req.outputDirectory, fileStem(names[static_cast<std::size_t>(i)]) + "_L0.dat");
        if (!bases.insert(outputBase).second) {
            if (err)
                *err = "输出基路径冲突: " + outputBase;
            return false;
        }

        if (cb.onMessage)
            cb.onMessage("转换 " + std::to_string(i + 1) + "/" + std::to_string(total) + ": " + names[static_cast<std::size_t>(i)]);

        ToZeroDataRequest treq;
        treq.sourcePath = sourcePath;
        treq.outputBasePath = outputBase;
        treq.saveRecovery = true;

        Callbacks tcb;
        tcb.onMessage = cb.onMessage;
        tcb.onError = cb.onError;
        tcb.isCancelled = cb.isCancelled;
        tcb.onProgress = [&](int p, const std::string &m) {
            if (!cb.onProgress)
                return;
            const int shown = static_cast<int>((50LL * (static_cast<long long>(i) * 100 + p)) / (total * 100));
            cb.onProgress(shown, m);
        };

        ZeroImageFolderSource item;
        item.sourcePath = sourcePath;
        item.outputBasePath = outputBase;
        if (!processToZeroData(treq, item.result, tcb, err)) {
            if (err && !err->empty())
                *err = sourcePath + ": " + *err;
            return false;
        }
        resp.sources.push_back(item);
    }

    if (cb.onMessage)
        cb.onMessage("四极化 L0 已全部生成，开始成像");

    ZeroImageRequest ireq;
    ireq.inputDirectory = req.outputDirectory;
    ireq.parameters = req.parameters;
    ireq.engineDirectory = req.engineDirectory;
    ireq.engineExecutable = req.engineExecutable;
    ireq.mcrRoot = req.mcrRoot;
    ireq.mcrCacheRoot = req.mcrCacheRoot;
    ireq.timeoutMs = req.timeoutMs;
    ireq.requireEngine = req.requireEngine;
    ireq.keepWorkspace = req.keepWorkspace;
    ireq.kind = req.kind;

    Callbacks icb;
    icb.onMessage = cb.onMessage;
    icb.onError = cb.onError;
    icb.isCancelled = cb.isCancelled;
    icb.onProgress = [&](int p, const std::string &m) {
        if (cb.onProgress)
            cb.onProgress(50 + p / 2, m);
    };

    ZeroImageResponse iresp;
    const bool ok = processZeroImage(ireq, iresp, icb, err);
    copyImageResult(iresp, resp);
    return ok;
}

std::string zeroImageSidecarPath(const std::string &polarizedPath)
{
    return ZeroDataImageHandle::Processor::sidecarPathForPolarizedFile(polarizedPath);
}
