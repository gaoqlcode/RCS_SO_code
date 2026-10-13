#ifndef ZERODATAIMAGEHANDLE_CPP11_H
#define ZERODATAIMAGEHANDLE_CPP11_H
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
# if defined(ZERODATAIMAGEHANDLE_BUILD)
#  define ZERODATAIMAGEHANDLE_API __declspec(dllexport)
# else
#  define ZERODATAIMAGEHANDLE_API __declspec(dllimport)
# endif
#else
# define ZERODATAIMAGEHANDLE_API __attribute__((visibility("default")))
#endif

namespace ZeroDataImageHandle {

enum class ProcessStatus { Success, Canceled, Failed };
enum class ProcessStage { Preparing, Restoring, Processing, Publishing, Cleaning };
// 当前只完整实现 SAR；ISAR 保留在接口上（Linux 侧暂未提供 ISAR 后处理程序）。
enum class ProcessKind { Sar, Isar };

// Para.ini 的 14 个参数。键名顺序与后处理程序读取的原始 Para.ini 一致。
struct ZERODATAIMAGEHANDLE_API Parameters {
    std::string fileDir;          // 文件目录
    int beginId = 30;             // 起始文件编号
    int deltaNum = 120;           // 间隔文件数目
    int lastFileId = 6000;        // 最后一个文件编号
    int rangeOffset = 100;        // 最近成像作用距离偏移值
    int fileNum = 16;             // 文件个数
    int testMode = 0;             // 测试模式
    int aperLen = 256;            // 误差估计点数
    int tarPosY = 1711;           // 目标所在位置（y)
    int iqOrderBand1 = 1;         // 第一个波段的IQ顺序
    int iqOrderBand2 = 1;         // 第二个波段的IQ顺序
    int rangePoints = 8192;       // 距离向点数
    int errEstStart = 500;        // 误差估计开始点
    int errEstLength = 500;       // 误差估计长度

    // 固定 14 项之外的附加键值行，原样追加在末尾（例如 ISAR 的「误差估计来源」）。
    std::vector<std::pair<std::string, std::string> > extraEntries;

    // 按 Para.ini 的固定顺序输出键值对；与固定键重名的附加行会被丢弃。
    std::vector<std::pair<std::string, std::string> > keyValues() const;
    // 是否是固定 14 项之一。
    static bool isKnownKey(const std::string &key);
    // 按键名回写一个值；键名不认识或数值解析失败时返回 false，且不修改。
    bool applyKeyValue(const std::string &key, const std::string &value);
    // 算法数值校验（不含目录存在性）；空表示通过。
    std::vector<std::string> validateParameters() const;
    // 目录存在性 + 数值校验；空表示通过。
    std::vector<std::string> validate() const;
};

// 引擎（Linux 版 SAR 后处理程序）的调用配置。
struct EngineOptions {
    // 完整路径。可以是 run_*.sh 包装脚本，也可以是裸可执行文件。
    std::string executable;
    // 引擎目录：作为子进程工作目录，也是 LD_LIBRARY_PATH 里的 "."。
    std::string engineDirectory;
    // MATLAB Runtime v9.3 根目录；供 run_*.sh 作为参数传入，也用于直接 exec 时拼 LD_LIBRARY_PATH。
    std::string mcrRoot = "/usr/local/MATLAB/MATLAB_Runtime/v93";
    // MATLAB 解包缓存目录。空表示沿用 MCR 默认（~/.mcrCache9.3），这是实测可用的方式。
    std::string mcrCacheRoot;
    // 0 表示不限制；超时先 SIGTERM，再 SIGKILL。
    int timeoutMs = 0;
    // 传给引擎的额外命令行参数（MATLAB 独立程序通常为空）。
    std::vector<std::string> extraArguments;
    // 追加到子进程环境的变量。
    std::vector<std::pair<std::string, std::string> > extraEnvironment;
    // 覆盖 MCR 解包目录里编译进去的默认 Para.ini。
    bool overwriteBundledConfig = true;
};

struct ProcessingOptions {
    ProcessKind kind = ProcessKind::Sar;
    // 输入目录：含 .sarrec 与四极化 DAT；还原数据与图像产物都回到此处。
    std::string inputDirectory;
    Parameters parameters;
    EngineOptions engine;
    // false 时只做还原、不启动引擎（用于无引擎自测）。
    bool requireEngine = true;
    // 调试用：保留内部工作目录。
    bool keepWorkspace = false;
};

struct RestoredInput {
    std::string sidecar;
    std::string originalName;
    std::string sourceSha256;
    std::uint64_t originalSize = 0;
};

struct PublishedFile {
    std::string path;
    std::string suffix;
    std::uint64_t bytes = 0;
};

struct ProcessingResult {
    ProcessStatus status = ProcessStatus::Failed;
    std::string error;
    std::vector<std::string> warnings;
    bool processStarted = false;
    int exitCode = -1;
    std::uint64_t restoredBytes = 0;
    std::vector<RestoredInput> inputs;
    std::vector<PublishedFile> publishedFiles;
    std::string jobId;
    std::string recordPath;
    std::string configPath;
    std::string logPath;
    std::string workspace;
    bool ok() const { return status == ProcessStatus::Success; }
};

struct ProcessingCallbacks {
    // 全部在 process() 调用线程同步执行；不要在回调中销毁对象或抛异常。
    std::function<void(ProcessStage)> stage;
    std::function<void(int)> progress;
    std::function<void(const std::string &)> log;
    // 返回 true 协作取消；也可从其他线程直接调用 Processor::cancel()。
    std::function<bool()> shouldCancel;
};

class ZERODATAIMAGEHANDLE_API Processor {
public:
    Processor();
    ~Processor();
    Processor(const Processor &) = delete;
    Processor &operator=(const Processor &) = delete;

    ProcessingResult process(const ProcessingOptions &,
                             const ProcessingCallbacks & = ProcessingCallbacks());
    void cancel();

    // 由四极化文件路径推导 .sarrec 路径（去掉 _HH 等后缀）。
    static std::string sidecarPathForPolarizedFile(const std::string &polarizedPath);
    // 引擎默认可执行文件名。
    static std::string defaultExecutableName(ProcessKind kind);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace ZeroDataImageHandle
#endif
