# RCS_SO_code — Linux / Windows / ARM 构建说明

## 依赖

- CMake ≥ 3.14
- C++11 编译器（g++ / MinGW / clang）
- 大图拼接：优先用工程内静态 libtiff（`3rdparty/install/linux_x86_64`，编进 `librcs_proc`，**客户无需 apt 装 libtiff5**）
- Qt5（仅 GUI；Windows 示例路径见下）

```bash
# Ubuntu 开发机（不开拼接可不装 libtiff-dev）
sudo apt install build-essential cmake qtbase5-dev
# 开拼接且要免客户依赖：先 ./3rdparty/scripts/build_linux_x86_64.sh
```

## 目录结构

```text
RCS_SO_code/
  lib/                 # 动态库 librcs_proc
    include/rcs/       # 对外头：processHrrp / processRcs / processCwRcs / processSigma0 / mosaic*
    src/
      common/ io/ signal/ hrrp/ rcs/ cw_rcs/ sigma0/ mosaic/ third_party/
  cli/                 # rcs_cli
  gui/                 # Qt：RCS/HRRP/点频/后向散射（+可选拼接）
  tests/  testdata/  cmake/  sdk/  scripts/build_win.bat
```

## 编译（Linux）

```bash
# 一键：编 rcs_proc → sdk/lib → GUI/CLI → deploy/linux_*（可发布运行包）
./scripts/build_linux.sh
# ./scripts/build_linux.sh --clean      # 清 build_linux 重来
# ./scripts/build_linux.sh --no-mosaic  # 关拼接
# ./scripts/build_linux.sh --no-gui     # 只编库和 cli（仍会打 deploy）
```

发布运行（整目录拷贝）：

```bash
./deploy/linux_x86_64/run_RCS.sh
```

或手动：

```bash
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Release -DRCS_USE_SDK=OFF
cmake --build build_linux -j$(nproc)
./sdk/pack_sdk.sh   # 需 RCS_BUILD_DIR=build_linux 或默认已指向 build_linux
cd build_linux && ctest --output-on-failure
```

常用开关：

| 选项 | 默认 | 说明 |
|------|------|------|
| `RCS_USE_SDK` | **OFF** | ON 时 GUI/CLI 链 `sdk/`（需当前平台已有库），拼接关闭。Windows 一键脚本固定 OFF |
| `RCS_SDK_ROOT` | `sdk/` | SDK 根目录 |
| `RCS_BUILD_GUI` | ON | Qt 界面 |
| `RCS_ENABLE_MOSAIC` | 仅 `RCS_USE_SDK=OFF` 且有静态 tiff 时 ON | 大场景拼接 |
| `RCS_TIFF_PREFER_STATIC` | ON | 静态链 libtiff，SDK 客户无需 `libtiff5` |

```bash
# 界面/命令行走 SDK（需先有 sdk/lib）
cmake -S . -B build -DRCS_USE_SDK=ON
cmake --build build -j

# 改库源码 / 要拼接 / 打 SDK 包
cmake -S . -B build -DRCS_USE_SDK=OFF
cmake --build build -j && ./sdk/pack_sdk.sh
```

## 编译（Windows，含大图拼接）

与 [`207_RCS_code`](file:///e:/project/SAR-RCS/207_RCS/207_RCS_code) 一样：用 **Qt MinGW 7.3** + 工程内 `3rdparty/install/mingw73_64` 的 libtiff（已随仓库提供）。

**不要用 Visual Studio 生成器去链这套静态库**（那是 mingw 编的）；请：

```bat
REM 一键：编库 → sdk\lib → GUI/CLI → deploy\win64\（含 Qt/MinGW 运行时）
scripts\build_win.bat
REM 或: .\scripts\build_win.ps1
REM 发布：拷贝 deploy\win64\ 整夹，双击 RCS.exe
```

或手动：

```bat
set PATH=C:\Qt\Qt5.12.8\5.12.8\mingw73_64\bin;C:\Qt\Qt5.12.8\Tools\mingw730_64\bin;%PATH%
cd RCS_SO_code
rmdir /s /q build_win
cmake -S . -B build_win -G "MinGW Makefiles" ^
  -DCMAKE_PREFIX_PATH=C:\Qt\Qt5.12.8\5.12.8\mingw73_64 ^
  -DRCS_ENABLE_MOSAIC=ON
cmake --build build_win -j
```

配置成功时应看到：`Using bundled libtiff: 3rdparty/install/mingw73_64`、`Mosaic ENABLED`。

仅库/不要拼接时可 `-DRCS_ENABLE_MOSAIC=OFF`（可不依赖 TIFF）。MSVC 需另备 libtiff（如 vcpkg），且 GUI 要用对应的 Qt msvc 套件。

## 界面用法

1. 顶部**一次**选择含 `*_L0_*.dat` 的数据文件夹  
2. 在 **RCS测量 / HRRP一维距离像 / 点频RCS** 各页填参数并开始（共用同一输入目录）  
3. **后向散射系数**：本页选择已成像 `uint16` LE `.raw`，交互框选噪声/角反/地物  
4. 拼接（仅 `RCS_ENABLE_MOSAIC=ON`）：独立 Tab，本页选择 TIFF/RAW 图目录  

## 终端用法

```bash
export LD_LIBRARY_PATH=$PWD/build_linux/lib:$LD_LIBRARY_PATH
./build_linux/cli/rcs_cli hrrp   --in <dat目录> --pol HH --out <输出>
./build_linux/cli/rcs_cli rcs    --in <dat目录> --pol HH --out <输出>
./build_linux/cli/rcs_cli cw-rcs --in <dat目录> --pol HH --out <输出>
./build_linux/cli/rcs_cli sigma0 --raw <img.raw> --nr 2048 --na 4602 \
  --cal x,y,w,h --tgt x,y,w,h --out <输出>
```

## 文档（对着代码读）

- [`docs/源码导读.md`](docs/源码导读.md)：建议阅读顺序 + 大段源码（OutDirField、Worker、去直流、BOM、裁剪、吸附、deploy）  
- [`docs/代码详解.md`](docs/代码详解.md)：调用链、Page/Worker、输出目录统一、demo  
- [`docs/模块详解.md`](docs/模块详解.md)：工程结构、输出约定、编译/deploy、各业务 API 与踩坑  

## 发布目录 deploy/

| 平台 | 路径 | 说明 |
|------|------|------|
| Linux | `deploy/linux_<arch>/` | `run_RCS.sh`；含 `librcs_proc` + Qt 等 |
| Windows | `deploy/win64/` | 双击 `RCS.exe`；含 Qt/MinGW 运行时 |

一键编译末尾自动生成；也可单独：`./scripts/deploy_linux.sh` / `.\scripts\deploy_win.ps1`。

## SDK 交付包

见 [`sdk/`](sdk/)：`.so`、头文件、`接口使用说明.md`、demo。刷新：

```bash
./sdk/pack_sdk.sh
```

## 公开 API

头文件：`lib/include/rcs/rcs_api.h`

- `processHrrp` / `processRcs` / `processCwRcs`：文件夹入参字段统一（`dataFolder/pol/outDir/...`）
- `processSigma0`：已成像 RAW → 后向散射 `.cs`（SDK 已开放）
- `processMosaic` 等：默认禁用（SDK 客户头不含）  
- 进度与选点：`Callbacks`；CLI / demo 用 `autoPick=true`


./3rdparty/scripts/build_linux_static.sh          # → install/linux_aarch64
cmake -S . -B build -DRCS_ENABLE_MOSAIC=ON -DRCS_TIFF_PREFER_STATIC=ON -DRCS_BUILD_GUI=OFF
cmake --build build -j && ./sdk/pack_sdk.sh