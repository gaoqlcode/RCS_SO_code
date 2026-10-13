#ifndef TOZERODATA_CPP11_H
#define TOZERODATA_CPP11_H
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
# if defined(TOZERODATA_BUILD)
#  define TOZERODATA_API __declspec(dllexport)
# else
#  define TOZERODATA_API __declspec(dllimport)
# endif
#else
# define TOZERODATA_API __attribute__((visibility("default")))
#endif

namespace ToZeroData {
enum class ConversionStatus { Success, Canceled, Failed };
enum class ConversionStage { Scanning, Converting, Committing, SavingRecovery };
struct ConversionOptions {
    std::string sourcePath;
    std::string outputBasePath;
    bool saveRecovery = false;
};
struct PolarizationOutput {
    std::string polarization;
    std::string path;
    std::uint64_t frames = 0;
    std::uint64_t bytes = 0;
};
struct ConversionResult {
    ConversionStatus status = ConversionStatus::Failed;
    std::string error;
    std::uint64_t validRecords = 0;
    std::uint64_t skippedRecords = 0;
    std::uint64_t outputBytes = 0;
    std::vector<PolarizationOutput> outputs;
    std::string recoveryPath;
    std::uint64_t recoveryBytes = 0;
    std::string sourceSha256;
};
struct ConversionCallbacks {
    // 回调同步运行于调用 convert 的线程；不要在回调中销毁转换对象。
    std::function<void(ConversionStage)> stage;
    std::function<void(int)> progress;
    std::function<void(const std::string &)> log;
    // 返回 true 以协作取消，也可从其他线程直接调用 cancel。
    std::function<bool()> shouldCancel;
};
class TOZERODATA_API Converter {
public:
    Converter();
    ~Converter();
    Converter(const Converter &) = delete;
    Converter &operator=(const Converter &) = delete;
    ConversionResult convert(const ConversionOptions &, const ConversionCallbacks & = ConversionCallbacks());
    void cancel();
    static std::array<std::string, 4> outputPathsForBasePath(const std::string &);
    static std::string recoveryPathForBasePath(const std::string &);
private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
#endif
