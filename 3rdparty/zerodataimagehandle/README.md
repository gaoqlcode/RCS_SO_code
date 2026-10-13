# ZeroDataImageHandle 标准 C++11 库调用说明

**版本说明：本文档基于命令行版本（main.cpp），使用标准 C++11，所有字符串为 std::string，文件流为 std::ifstream/std::ofstream，不依赖 Qt。**

## 包内包含什么

| 路径 | 内容 |
| --- | --- |
| `lib/` | `libZeroDataImageHandleCpp11.a`（主交付，静态库）、`libZeroDataImageHandleCpp11.so` |
| `include/zerodataimagehandle.h` | 唯一公开头文件 |
| `demo/` | 命令行版本 `main.cpp` 与 CMake 工程 |
| `engine/for_testing/` | **Linux 版 SAR 后处理程序**（MATLAB 独立可执行程序 + `run_*.sh` + 模板 `Para.ini`） |
| `build_demo.sh` / `run_demo.sh` | 交付目录内独立构建 / 运行 |
| `README.md` / `BUILD_INFO.txt` | 本说明与构建信息 |

不包含核心实现源码。无需 Qt、qmake、OpenSSL 或其他第三方开发库；只需要 C++11 编译器、CMake 和系统线程支持。库内部对文件系统的访问、进程调用与目录提交由薄的平台适配层实现，数据读写均为标准文件流。

## 这个库做什么

把「转换数据目录」（同一目录下的四极化 DAT 与 .sarrec 还原信息）处理成图像：

1. 扫描目录中的 .sarrec，校验四极化 DAT 的长度与 SHA-256；
2. 还原出后处理程序需要的原始 SAR_Raw_XXXX.dat；
3. 按 14 项参数写出 UTF-8 的 Para.ini（文件目录指向本次还原出的数据目录）；
4. 调用 Linux 版 SAR 后处理程序，捕获其输出；
5. 收集新增的非空图像与附加产物，写回所选转换数据目录（同名覆盖）；
6. 清理内部工作目录，并在 zeroDataImageHandle-logs 下留下 JSON / INI / LOG 记录。

## 运行前置条件

**目标机必须已安装 MATLAB Runtime v9.3（R2017b）。** 后处理程序是 MATLAB Compiler 打包的独立程序，
缺少运行时会直接启动失败；本库在启动前会检查该目录并给出明确报错。

- 默认路径：`/usr/local/MATLAB/MATLAB_Runtime/v93`
- 装在别处时：命令行传 `--mcr-root <目录>`，或设环境变量 `MCR_ROOT`（`run_demo.sh` 会自动带上）。
- 若目标机不能联网，可使用 MathWorks 提供的同版本离线安装器
  （`MyAppInstaller_mcr.install`，约 924MB）；
  `build_linux.sh` 设 `ZDI_INCLUDE_MCR_INSTALLER=1` 可另外产出一个
  `..._Customer_with_MCR.tar.gz`，把该安装器一并打在包内。

## 编译和运行示例

在交付目录执行：

```bash
bash build_demo.sh
bash run_demo.sh --help
```

对一批真实数据的完整调用。**引擎已随包自带**，`run_demo.sh` 会自动使用 `engine/for_testing`，
因此只需给输入目录和与数据匹配的参数：

```bash
bash run_demo.sh \
  --input /data/usershare/data \
  --begin 1 --delta 20 --last 20 --num 20 \
  --mcr-root /usr/local/MATLAB/MATLAB_Runtime/v93 \
  --timeout 1800
```

上例会在输入目录直接产出 `Image_Nr8192Na2666File1_21Ku_{HH,HV,VV,VH}.raw`。

也可显式指定引擎：`--exe-dir <含 run_*.sh 的目录>`，或 `--engine <run_*.sh 或裸可执行文件>`。
`--no-engine` 只做还原、不启动引擎，用于无引擎自测。

Ctrl+C 协作取消。退出码：0 成功、1 失败、2 参数错误、3 取消。

## 在甲方 C++ 工程中链接

静态库（推荐：编译期把 .a 链入可执行文件，运行期不依赖 .so，也不需要 LD_LIBRARY_PATH）：

```bash
g++ -std=c++11 your_main.cpp -I/path/to/sdk/include \
    /path/to/sdk/lib/libZeroDataImageHandleCpp11.a -pthread -o your_app
./your_app
```

动态库（运行期需要 .so 与 LD_LIBRARY_PATH）：

```bash
g++ -std=c++11 your_main.cpp -I/path/to/sdk/include -L/path/to/sdk/lib \
    -lZeroDataImageHandleCpp11 -pthread -o your_app
LD_LIBRARY_PATH=/path/to/sdk/lib ./your_app
```

静态链接同样必须带 -pthread，库内部使用 std::thread 与 std::mutex，并用 flock 做跨进程互斥。两种链接方式的库同名，由同一份源码、同一编译参数生成，接口与行为一致。

也可按 demo/CMakeLists.txt 把库作为 IMPORTED target 接入 CMake；-DZERODATAIMAGEHANDLE_USE_STATIC=OFF 可改用动态库。不要使用旧 Qt 版头文件。库和调用方必须使用兼容的架构、编译器、libstdc++ ABI、标准库与 libc 环境；构建信息见 BUILD_INFO.txt。该接口含 std::string 与 STL 类型，属于 C++ ABI，不是跨编译器的 C ABI。

## 最小调用

```cpp
#include "zerodataimagehandle.h"
#include <iostream>

int main() {
    ZeroDataImageHandle::ProcessingOptions options;
    options.inputDirectory = "/data/zero-level";          // 含 .sarrec 与四极化 DAT
    options.parameters.fileDir = options.inputDirectory;
    options.parameters.beginId = 1;
    options.parameters.deltaNum = 20;
    options.parameters.fileNum = 20;
    options.parameters.lastFileId = 20;
    options.engine.engineDirectory =
        "/opt/GeneralImagingProcess_20260702/for_testing";
    options.engine.mcrRoot = "/usr/local/MATLAB/MATLAB_Runtime/v93";

    ZeroDataImageHandle::ProcessingCallbacks callbacks;
    callbacks.progress = [](int p) { std::cout << p << "%\n"; };
    callbacks.log = [](const std::string &s) { std::cout << s << "\n"; };

    ZeroDataImageHandle::Processor processor;
    const ZeroDataImageHandle::ProcessingResult result = processor.process(options, callbacks);
    if (!result.ok()) { std::cerr << result.error << "\n"; return 1; }
    for (std::size_t i = 0; i < result.publishedFiles.size(); ++i)
        std::cout << result.publishedFiles[i].path << "\n";
    return 0;
}
```

## 接口和线程规则

- process 返回 ProcessingResult，同步阻塞当前线程，调用方可使用自己的 std::thread；不需要事件循环。
- callbacks.stage、progress、log 在 process 调用线程同步执行。shouldCancel 返回 true 时取消；也可从其他线程直接调用 processor.cancel()。
- 同一实例不允许并发或重入；收到重入调用时返回 Failed，不影响正在执行的任务。空闲时 cancel 无效，下一任务开始时复位。
- 执行期间不得销毁实例；回调不要销毁对象或抛出异常。关闭调用方程序前取消并等待工作线程结束。
- 所有路径及文本为 std::string，在 Linux 按 UTF-8 使用；字节计数为固定宽度整数。
- 库用 flock 保证同一时刻只有一个任务在跑（锁文件默认 /tmp/zeroDataImageHandle.lock，可用环境变量 ZERODATAIMAGEHANDLE_LOCK 覆盖）：后处理程序读取的 Para.ini 是进程外全局状态，并发会互相覆盖参数且不报错。
- sidecarPathForPolarizedFile 由四极化文件路径推回 .sarrec 路径。

## 参数与配置约定（与 Windows 版的关键差异）

- **Para.ini 必须是 UTF-8**。Linux 版后处理程序按 UTF-8 解析中文键名，GBK 会导致键名整份匹配不上、静默失效（表现为「改了参数没效果」）。库只产出 UTF-8、LF、无 BOM；读到非 UTF-8 配置文件会明确报错，不做静默回退。
- **「文件目录」必须写纯 Linux 绝对路径**。后处理程序不会剥离 file:// 前缀，写 URL 形式必然打不开；库在写入前会主动剥掉 file:// 前缀。
- 14 个键名与后处理程序读取的字符串逐字节一致，其中「目标所在位置（y)」是全角左括号 + 半角右括号，不可改为对称括号。
- 固定 14 项之外的附加键（例如 ISAR 用的「误差估计来源」）可通过 Parameters::extraEntries 原样追加；与固定键重名的附加行会被丢弃。
- 后处理程序会在 MCR 解包目录里查找编译进去的默认 Para.ini（存在就优先）。库会在每次运行前用当前参数覆盖它，并始终在工作目录写好 Para.ini；首次运行缓存尚未生成时，会在补写后再重试一次。
- 默认沿用 MCR 自身的缓存目录（~/.mcrCache9.3）。如需隔离，可用 EngineOptions::mcrCacheRoot 指定，此时库会在该目录下查找并覆盖内置配置。
- **`文件个数` 至少要填 2**。后处理程序处理到第 2 个文件时才计算航迹角，填 1 会因 `未定义函数或变量 'TrackAngle'` 中止（实测：同一份 20 组数据下，`文件个数=1` 失败、`文件个数=2` 成功）。`文件个数` 同时决定一次成像的方位向点数，输出文件名里的 `Nr…Na…` 会随之变化。
- **参数必须与数据匹配**：模板默认「起始 30、间隔 120、个数 16」需要编号 0030/0150… 的文件。请按实际数据传 `--begin / --delta / --num / --last`。另外，后处理程序只按参数指定的编号去读文件，磁盘上多放数据并不会改变它的行为。

## 返回结果和文件保护

status 为 Success、Canceled 或 Failed，error 为错误信息。inputs 返回每组还原出的原始文件名、长度与 SHA-256；publishedFiles 返回回写到输入目录的产物路径与字节数；recordPath / configPath / logPath 指向本次任务的 JSON 记录、用户参数 INI 与任务日志。

后处理程序退出码非 0 时仍会收集并保存已生成的非空图像（数据或参数不匹配时算法可能中止，属既有行为）。未生成任何非空图像则报错。产物按后缀白名单收集：图像 raw/tif/tiff/png/jpg/jpeg/bmp，附加产物 mat/fig/txt/csv/bin；同名图像直接覆盖，产物不得覆盖输入数据。内部工作目录在所有正常结束、失败和取消路径中清理，权限或占用导致清理失败会在 warnings 中报告。

当前 Linux 版只完整支持 SAR；ISAR 在接口上保留（ProcessKind），但需要另行提供 ISAR 后处理程序。

## 运行前提

- MATLAB Runtime v9.3（R2017b），默认 /usr/local/MATLAB/MATLAB_Runtime/v93 —— **需在目标机安装**。
- Linux 版 SAR 后处理程序 —— **已随包自带**（`engine/for_testing/`，含 `run_GeneralImagingProcess_20260702.sh`）。
- 输入目录所在磁盘需同时容纳本次还原的原始数据（每组约等于原始文件大小）、MCR 缓存与图像结果。
