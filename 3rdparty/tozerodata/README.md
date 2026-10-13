# ToZeroData 标准 C++11 库调用说明

**版本说明：本文档基于命令行版本（main.cpp），使用标准 C++11，所有字符串为 std::string，文件流为 std::ifstream/std::ofstream，不依赖 Qt。**

本包包含 libToZeroDataCpp11.so、libToZeroDataCpp11.a、公开头文件和命令行 demo 源码，不包含核心转换源码。无需 Qt、qmake、OpenSSL 或其他第三方开发库；需要 C++11 编译器、CMake 和系统线程支持。Linux 的目录创建和不覆盖文件提交由薄的平台适配层实现，数据读写均为标准文件流。

## 编译和运行示例

在交付目录执行：

```bash
bash build_demo.sh
bash run_demo.sh --help
bash run_demo.sh --source /data/source.dat --output-base /data/output/source_L0.dat --save-recovery
```

输出目录不存在时会创建；省略 --save-recovery 则仅生成四个 DAT。Ctrl+C 协作取消，取消和失败不会提交本次半成品。退出码：0 成功、1 转换失败、2 参数错误、3 取消。

## 在甲方 C++ 工程中链接

共享库（运行期需要 .so 与 LD_LIBRARY_PATH）：

```bash
g++ -std=c++11 your_main.cpp -I/path/to/sdk/include -L/path/to/sdk/lib -lToZeroDataCpp11 -pthread -o your_app
LD_LIBRARY_PATH=/path/to/sdk/lib ./your_app
```

静态库（编译期把 .a 链入可执行文件，运行期不依赖 .so，也不需要 LD_LIBRARY_PATH）：

```bash
g++ -std=c++11 your_main.cpp -I/path/to/sdk/include /path/to/sdk/lib/libToZeroDataCpp11.a -pthread -o your_app
./your_app
```

静态链接同样必须带 -pthread，库内部使用 std::thread 与 std::mutex。libToZeroDataCpp11.a 与共享库同名，由同一份源码、同一编译参数生成，接口与行为一致。

也可按 demo/CMakeLists.txt 将库作为 IMPORTED target 接入 CMake；加 -DTOZERODATA_USE_STATIC=ON 可选用静态库。不要编译核心转换源码，不要使用旧 Qt 版头文件。库和调用方必须使用兼容的架构、编译器、libstdc++ ABI、标准库与 libc 环境；构建信息见 BUILD_INFO.txt。该接口含 std::string 和 STL 类型，属于 C++ ABI，不是跨编译器的 C ABI。

## 最小调用

```cpp
#include "tozerodata.h"
#include <iostream>

int main() {
    ToZeroData::Converter converter;
    ToZeroData::ConversionOptions options;
    options.sourcePath = "/data/source.dat";
    options.outputBasePath = "/data/output/source_L0.dat";
    options.saveRecovery = true;
    ToZeroData::ConversionCallbacks callbacks;
    callbacks.progress = [](int p) { std::cout << p << "%\n"; };
    callbacks.log = [](const std::string &s) { std::cout << s << "\n"; };
    const auto result = converter.convert(options, callbacks);
    if (result.status != ToZeroData::ConversionStatus::Success) {
        std::cerr << result.error << "\n";
        return 1;
    }
    std::cout << result.recoveryPath << "\n";
    return 0;
}
```

## 接口和线程规则

- convert 返回 ConversionResult，同步阻塞当前线程，调用方可使用自己的 std::thread；不需要事件循环。
- callbacks.stage、progress、log 在转换调用线程同步执行。shouldCancel 返回 true 时取消；也可从其他线程直接调用 converter.cancel()。
- 同一实例不允许并发或重入；收到重入调用时返回 Failed，不影响正在执行的任务。空闲时 cancel 无效，下一任务开始时复位。
- 执行期间不得销毁实例；回调不要销毁对象或抛出异常。关闭调用方程序前取消并等待工作线程结束。
- 所有路径及文本为 std::string，在 Linux 按 UTF-8 使用；字节计数与帧数为固定宽度整数。
- outputPathsForBasePath 返回 HH、VV、HV、VH 四个派生路径；recoveryPathForBasePath 返回 .sarrec 路径。

## 返回结果和文件保护

status 为 Success、Canceled 或 Failed，error 为错误信息。outputs 返回四个 DAT 的 polarization、path、frames、bytes；outputBytes 只统计四个 DAT。开启保存且成功时，recoveryPath、recoveryBytes、sourceSha256 返回伴随文件路径、大小及源数据 SHA-256；否则为空或零。

沿用原有四极化 DAT 字节格式、I/Q 转幅相规则和 SARREC01 v1 伴随格式。未对应极化的文件仍生成 512 字节文件头。.sarrec 必须与四个 DAT 一起保留，可整体移动，不可单独重命名。该接口只生成还原信息，不导出还原 API。

已有输出或源路径冲突时拒绝覆盖。使用同目录暂存，全部生成后提交，.sarrec 最后提交；失败或取消清理本次输出。Linux 使用不覆盖目标的提交操作，提交失败回滚本次已提交文件。多文件提交不能保证断电或进程崩溃时整体原子完成，请以 Success 结果为完成标志。
