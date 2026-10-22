#include "rcs/rcs_api.h"

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool isDir(const std::string &path)
{
    struct stat st;
    return !path.empty() && ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool isFile(const std::string &path)
{
    struct stat st;
    return !path.empty() && ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
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

std::string resolveDir(const std::string &path)
{
    if (path.empty())
        return std::string();
    char resolved[4096];
    if (::realpath(path.c_str(), resolved) && isDir(resolved))
        return resolved;
    if (isDir(path))
        return path;
    return std::string();
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

bool hasSpace(const std::string &path)
{
    return path.find(' ') != std::string::npos;
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

std::string postExecutableIn(const std::string &dir)
{
    if (dir.empty())
        return std::string();
    const std::string nested = joinPath(joinPath(dir, "bin"), "sarIsarPostProcess");
    if (isFile(nested))
        return nested;
    const std::string direct = joinPath(dir, "sarIsarPostProcess");
    if (isFile(direct))
        return direct;
    return std::string();
}

std::string exporterIn(const std::string &dir)
{
    if (dir.empty())
        return std::string();
    const std::string nested = joinPath(joinPath(dir, "bin"), "SarTxtExporter");
    if (isFile(nested))
        return nested;
    const std::string direct = joinPath(dir, "SarTxtExporter");
    if (isFile(direct))
        return direct;
    return std::string();
}

std::string defaultPostRoot()
{
    if (const char *env = std::getenv("RCS_POST_ROOT")) {
        const std::string dir = resolveDir(env);
        if (!postExecutableIn(dir).empty())
            return dir;
    }
    if (const char *env = std::getenv("RCS_ZDI_ENGINE_DIR")) {
        const std::string dir = resolveDir(env);
        if (!postExecutableIn(dir).empty())
            return dir;
        const std::string parent = resolveDir(parentDirectory(dir));
        if (!postExecutableIn(parent).empty())
            return parent;
    }
    const std::string libDir = soDirectory();
    if (!libDir.empty()) {
        const std::string beside = resolveDir(libDir + "/../engine");
        if (!postExecutableIn(beside).empty())
            return beside;
    }
#ifdef RCS_POST_ROOT
    {
        const std::string compiled = resolveDir(RCS_POST_ROOT);
        if (!postExecutableIn(compiled).empty())
            return compiled;
    }
#endif
    return std::string();
}

const char *exitText(int code)
{
    switch (code) {
    case 1:
        return "命令行参数错误";
    case 2:
        return "扫描或批次错误";
    case 3:
        return "还原失败";
    case 4:
        return "成像失败";
    case 5:
        return "磁盘或内存不足";
    case 6:
        return "产物回写失败";
    default:
        return "后处理程序失败";
    }
}

struct RunResult {
    int exitCode;
    std::string output;
    RunResult() : exitCode(-1) {}
};

RunResult runCommand(const std::vector<std::string> &args, int timeoutMs,
                     const std::function<bool()> &cancelled,
                     const std::function<void(const std::string &)> &onLine)
{
    RunResult result;
    if (args.empty()) {
        result.output = "没有可执行文件";
        return result;
    }
    int pipefd[2];
    if (::pipe(pipefd) != 0) {
        result.output = "无法创建管道";
        return result;
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        result.output = "无法启动后处理程序";
        return result;
    }
    if (pid == 0) {
        ::setpgid(0, 0);
        ::close(pipefd[0]);
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        if (pipefd[1] != STDOUT_FILENO && pipefd[1] != STDERR_FILENO)
            ::close(pipefd[1]);
        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (std::size_t i = 0; i < args.size(); ++i)
            argv.push_back(const_cast<char *>(args[i].c_str()));
        argv.push_back(0);
        // 后处理程序按其自身 RUNPATH 使用 engine/lib。
        ::unsetenv("LD_LIBRARY_PATH");
        ::unsetenv("QT_PLUGIN_PATH");
        ::unsetenv("QT_QPA_PLATFORM_PLUGIN_PATH");
        ::execv(argv[0], argv.data());
        const char *msg = "无法执行后处理程序\n";
        if (::write(STDOUT_FILENO, msg, std::strlen(msg)) < 0)
            _exit(127);
        _exit(127);
    }
    ::setpgid(pid, pid);
    ::close(pipefd[1]);
    const int flags = ::fcntl(pipefd[0], F_GETFL, 0);
    if (flags >= 0)
        ::fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    std::string pending;
    const time_t start = ::time(0);
    bool killed = false;
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(pipefd[0], &rfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        const int sel = ::select(pipefd[0] + 1, &rfds, 0, 0, &tv);
        if (sel > 0 && FD_ISSET(pipefd[0], &rfds)) {
            char buf[4096];
            const ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
            if (n > 0) {
                pending.append(buf, buf + n);
                result.output.append(buf, buf + n);
                std::size_t pos = 0;
                for (;;) {
                    const std::size_t nl = pending.find('\n', pos);
                    if (nl == std::string::npos)
                        break;
                    std::string line = pending.substr(pos, nl - pos);
                    if (!line.empty() && line[line.size() - 1] == '\r')
                        line.erase(line.size() - 1);
                    if (onLine && !line.empty())
                        onLine(line);
                    pos = nl + 1;
                }
                if (pos > 0)
                    pending.erase(0, pos);
            } else if (n == 0) {
                break;
            }
        }
        int status = 0;
        const pid_t w = ::waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            if (WIFEXITED(status))
                result.exitCode = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                result.exitCode = 128 + WTERMSIG(status);
            else
                result.exitCode = -1;
            break;
        }
        if (!killed && cancelled && cancelled()) {
            ::kill(-pid, SIGTERM);
            killed = true;
            result.exitCode = -2;
        }
        if (!killed && timeoutMs > 0) {
            const time_t elapsed = ::time(0) - start;
            if (elapsed * 1000 >= timeoutMs) {
                ::kill(-pid, SIGTERM);
                killed = true;
                result.exitCode = -3;
            }
        }
    }
    char drain[4096];
    for (;;) {
        const ssize_t n = ::read(pipefd[0], drain, sizeof(drain));
        if (n > 0) {
            pending.append(drain, drain + n);
            result.output.append(drain, drain + n);
        } else {
            break;
        }
    }
    ::close(pipefd[0]);
    if (onLine) {
        std::size_t pos = 0;
        while (pos < pending.size()) {
            const std::size_t nl = pending.find('\n', pos);
            const std::size_t end = nl == std::string::npos ? pending.size() : nl;
            std::string line = pending.substr(pos, end - pos);
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (!line.empty())
                onLine(line);
            if (nl == std::string::npos)
                break;
            pos = nl + 1;
        }
    }
    int status = 0;
    if (::waitpid(pid, &status, 0) == pid && result.exitCode == -1) {
        if (WIFEXITED(status))
            result.exitCode = WEXITSTATUS(status);
    }
    if (killed && result.exitCode > 0)
        result.exitCode = cancelled && cancelled() ? -2 : result.exitCode;
    return result;
}

std::string suffixOf(const std::string &path)
{
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return std::string();
    return path.substr(dot);
}

void collectPublished(const std::string &dir, const std::map<std::string, std::uint64_t> &before,
                      std::vector<ZeroImagePublishedFile> &out)
{
    if (!isDir(dir))
        return;
    DIR *dp = ::opendir(dir.c_str());
    if (!dp)
        return;
    while (dirent *ent = ::readdir(dp)) {
        const std::string name = ent->d_name;
        if (name == "." || name == "..")
            continue;
        const bool image = (name.find("Image_") == 0 || name.find("IsarImage_") == 0);
        if (!image)
            continue;
        if (!(hasSuffixI(name, ".raw") || hasSuffixI(name, ".tif") || hasSuffixI(name, ".tiff")
              || hasSuffixI(name, ".cplx") || hasSuffixI(name, ".txt")))
            continue;
        const std::string full = joinPath(dir, name);
        struct stat st;
        if (::stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        const std::map<std::string, std::uint64_t>::const_iterator it = before.find(full);
        if (it != before.end() && it->second == static_cast<std::uint64_t>(st.st_mtime))
            continue;
        ZeroImagePublishedFile f;
        f.path = full;
        f.suffix = suffixOf(name);
        f.bytes = static_cast<std::uint64_t>(st.st_size);
        out.push_back(f);
    }
    ::closedir(dp);
}

std::map<std::string, std::uint64_t> snapshotImages(const std::string &dir)
{
    std::map<std::string, std::uint64_t> before;
    if (!isDir(dir))
        return before;
    DIR *dp = ::opendir(dir.c_str());
    if (!dp)
        return before;
    while (dirent *ent = ::readdir(dp)) {
        const std::string name = ent->d_name;
        if (name.find("Image_") != 0 && name.find("IsarImage_") != 0)
            continue;
        const std::string full = joinPath(dir, name);
        struct stat st;
        if (::stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            before[full] = static_cast<std::uint64_t>(st.st_mtime);
    }
    ::closedir(dp);
    return before;
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

std::string resolvePostExe(const ZeroImageRequest &req, std::string *err)
{
    if (!req.engineExecutable.empty()) {
        if (!isFile(req.engineExecutable)) {
            if (err)
                *err = "引擎程序不存在: " + req.engineExecutable;
            return std::string();
        }
        return req.engineExecutable;
    }
    std::string root = req.engineDirectory;
    if (!root.empty() && !isDir(root)) {
        if (err)
            *err = "引擎目录不存在: " + root;
        return std::string();
    }
    if (root.empty())
        root = defaultPostRoot();
    const std::string exe = postExecutableIn(root);
    if (exe.empty()) {
        if (err)
            *err = "未找到 sarIsarPostProcess。请设置 engineDirectory 为后处理包根目录，"
                   "或把含 bin/sarIsarPostProcess 的 engine 放在库旁";
        return std::string();
    }
    return exe;
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
    if (!isDir(imageDir)) {
        if (err)
            *err = "成像输入目录不存在: " + imageDir;
        return false;
    }

    if (!req.requireEngine) {
        if (cb.onMessage)
            cb.onMessage("只还原，不启动成像");
    }

    std::string localErr;
    const std::string exe = resolvePostExe(req, &localErr);
    if (exe.empty()) {
        if (err)
            *err = localErr;
        return false;
    }

    const std::string outDir =
        req.imageOutputDirectory.empty() ? imageDir : req.imageOutputDirectory;
    if (hasSpace(imageDir) || hasSpace(outDir) || hasSpace(exe)) {
        if (err)
            *err = "路径不能含空格: " + (hasSpace(imageDir) ? imageDir : outDir);
        return false;
    }

    std::vector<std::string> args;
    args.push_back(exe);
    args.push_back("--kind");
    args.push_back(req.kind == 1 ? "isar" : "sar");
    args.push_back("--input");
    args.push_back(imageDir);
    if (!req.imageOutputDirectory.empty()) {
        args.push_back("--output");
        args.push_back(req.imageOutputDirectory);
    }
    args.push_back("--begin");
    args.push_back(std::to_string(req.parameters.beginId));
    args.push_back("--last");
    args.push_back(std::to_string(req.parameters.lastFileId));
    args.push_back("--delta");
    args.push_back(std::to_string(req.parameters.deltaNum));
    args.push_back("--num");
    args.push_back(std::to_string(req.parameters.fileNum));
    args.push_back("--range-offset");
    args.push_back(std::to_string(req.parameters.rangeOffset));
    args.push_back("--nr");
    args.push_back(std::to_string(req.parameters.rangePoints));
    if (req.roiW > 0) {
        args.push_back("--roi");
        args.push_back(std::to_string(req.roiX));
        args.push_back(std::to_string(req.roiY));
        args.push_back(std::to_string(req.roiW));
        if (req.roiH > 0)
            args.push_back(std::to_string(req.roiH));
    }
    if (req.exportTxt)
        args.push_back("--export-txt");
    if (!req.requireEngine)
        args.push_back("--restore-only");
    if (req.keepWorkspace)
        args.push_back("--keep-work");

    const std::map<std::string, std::uint64_t> before = snapshotImages(outDir);
    if (cb.onMessage)
        cb.onMessage(req.requireEngine ? "调用 sarIsarPostProcess" : "调用 sarIsarPostProcess --restore-only");

    const RunResult ran = runCommand(
        args, req.timeoutMs, [&]() { return cb.isCancelled && cb.isCancelled(); },
        [&](const std::string &line) {
            if (cb.onMessage)
                cb.onMessage(line);
            const std::string tag = "[发布] ";
            const std::size_t at = line.find(tag);
            if (at == std::string::npos)
                return;
            std::string name = line.substr(at + tag.size());
            const std::size_t sp = name.find(' ');
            if (sp != std::string::npos)
                name = name.substr(0, sp);
            if (name.empty())
                return;
            const std::string full = name[0] == '/' ? name : joinPath(outDir, name);
            struct stat st;
            if (::stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
                return;
            ZeroImagePublishedFile f;
            f.path = full;
            f.suffix = suffixOf(full);
            f.bytes = static_cast<std::uint64_t>(st.st_size);
            resp.publishedFiles.push_back(f);
        });

    resp.exitCode = ran.exitCode;
    if (ran.exitCode == -2) {
        if (err)
            *err = "已取消";
        return false;
    }
    if (ran.exitCode == -3) {
        if (err)
            *err = "后处理超时";
        return false;
    }
    if (ran.exitCode != 0) {
        std::string why = exitText(ran.exitCode < 0 ? 0 : ran.exitCode);
        if (ran.exitCode >= 1 && ran.exitCode <= 6)
            why = exitText(ran.exitCode);
        else if (ran.exitCode != 0)
            why = "后处理程序退出码 " + std::to_string(ran.exitCode);
        if (err)
            *err = why;
        if (cb.onError)
            cb.onError(why);
        return false;
    }
    if (resp.publishedFiles.empty())
        collectPublished(outDir, before, resp.publishedFiles);
    if (cb.onProgress)
        cb.onProgress(100, req.requireEngine ? "零级成像完成" : "还原完成");
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
        const std::string outputBase =
            joinPath(req.outputDirectory, fileStem(names[static_cast<std::size_t>(i)]) + "_L0.dat");
        if (!bases.insert(outputBase).second) {
            if (err)
                *err = "输出基路径冲突: " + outputBase;
            return false;
        }

        if (cb.onMessage)
            cb.onMessage("转换 " + std::to_string(i + 1) + "/" + std::to_string(total) + ": "
                         + names[static_cast<std::size_t>(i)]);

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
            const int shown =
                static_cast<int>((50LL * (static_cast<long long>(i) * 100 + p)) / (total * 100));
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
    ireq.timeoutMs = req.timeoutMs;
    ireq.requireEngine = req.requireEngine;
    ireq.keepWorkspace = req.keepWorkspace;
    ireq.kind = req.kind;
    ireq.roiX = req.roiX;
    ireq.roiY = req.roiY;
    ireq.roiW = req.roiW;
    ireq.roiH = req.roiH;
    ireq.exportTxt = req.exportTxt;
    ireq.imageOutputDirectory = req.imageOutputDirectory;

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
    std::string path = polarizedPath;
    const std::size_t slash = path.find_last_of('/');
    const std::string dir = slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const std::size_t dot = name.find_last_of('.');
    std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
    const char *pols[] = {"_HH", "_VV", "_HV", "_VH"};
    for (int i = 0; i < 4; ++i) {
        const std::string pol = pols[i];
        if (stem.size() >= pol.size()
            && stem.compare(stem.size() - pol.size(), pol.size(), pol) == 0) {
            stem.erase(stem.size() - pol.size());
            break;
        }
    }
    return dir + stem + ".sarrec";
}

bool exportSliceTxt(const SliceTxtRequest &req, SliceTxtResponse &resp, Callbacks &cb,
                    std::string *err)
{
    resp = SliceTxtResponse();
    if (req.imagePath.empty()) {
        if (err)
            *err = "imagePath 为空";
        return false;
    }
    if (hasSpace(req.imagePath) || hasSpace(req.outPath)) {
        if (err)
            *err = "路径不能含空格";
        return false;
    }
    std::string root = req.engineDirectory;
    if (!root.empty() && !isDir(root)) {
        if (err)
            *err = "引擎目录不存在: " + root;
        return false;
    }
    if (root.empty())
        root = defaultPostRoot();
    const std::string exe = exporterIn(root);
    if (exe.empty()) {
        if (err)
            *err = "未找到 SarTxtExporter";
        return false;
    }
    std::vector<std::string> args;
    args.push_back(exe);
    args.push_back(req.imagePath);
    if (req.roiW > 0) {
        args.push_back("--roi");
        args.push_back(std::to_string(req.roiX));
        args.push_back(std::to_string(req.roiY));
        args.push_back(std::to_string(req.roiW));
        if (req.roiH > 0)
            args.push_back(std::to_string(req.roiH));
    }
    if (!req.outPath.empty()) {
        args.push_back("--out");
        args.push_back(req.outPath);
    }
    if (!req.taskName.empty()) {
        args.push_back("--task-name");
        args.push_back(req.taskName);
    }
    if (!req.taskLeader.empty()) {
        args.push_back("--task-leader");
        args.push_back(req.taskLeader);
    }
    if (!req.targetName.empty()) {
        args.push_back("--target-name");
        args.push_back(req.targetName);
    }
    if (cb.onMessage)
        cb.onMessage("调用 SarTxtExporter");
    const RunResult ran = runCommand(
        args, req.timeoutMs, [&]() { return cb.isCancelled && cb.isCancelled(); },
        [&](const std::string &line) {
            if (cb.onMessage)
                cb.onMessage(line);
            const std::string tag = "输出目标路径:";
            const std::size_t at = line.find(tag);
            if (at != std::string::npos) {
                std::string p = line.substr(at + tag.size());
                while (!p.empty() && (p[0] == ' ' || p[0] == '\t'))
                    p.erase(p.begin());
                resp.txtPath = p;
            }
        });
    resp.exitCode = ran.exitCode;
    if (ran.exitCode == -2) {
        if (err)
            *err = "已取消";
        return false;
    }
    if (ran.exitCode != 0) {
        if (err)
            *err = ran.exitCode == 1 ? "切片导出失败" : "SarTxtExporter 退出码 " + std::to_string(ran.exitCode);
        return false;
    }
    if (resp.txtPath.empty()) {
        if (!req.outPath.empty())
            resp.txtPath = req.outPath;
        else {
            std::string base = req.imagePath;
            const std::size_t dot = base.find_last_of('.');
            const std::size_t slash = base.find_last_of('/');
            if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
                base = base.substr(0, dot);
            resp.txtPath = base + ".txt";
        }
    }
    if (cb.onProgress)
        cb.onProgress(100, "切片报表已导出");
    return true;
}
