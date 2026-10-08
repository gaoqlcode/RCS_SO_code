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
    include/rcs/       # 对外头：processHrrp / processRcs / processCwRcs / mosaic*
    src/
      common/ io/ signal/ hrrp/ rcs/ cw_rcs/ mosaic/ third_party/
  cli/                 # rcs_cli
  gui/                 # Qt：顶部共享数据文件夹 + RCS/HRRP/点频RCS
  tests/  testdata/  cmake/  sdk/  scripts/build_win.bat
```

## 编译（Linux）

```bash
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Release
cmake --build build_linux -j$(nproc)
cd build_linux && ctest --output-on-failure
```

常用开关：

| 选项 | 默认 | 说明 |
|------|------|------|
| `RCS_BUILD_GUI` | ON | Qt 界面 |
| `RCS_ENABLE_MOSAIC` | 有 3rdparty 静态 tiff 时 **ON**，否则 OFF | 大场景拼接；`RCS_TIFF_PREFER_STATIC=ON`（默认）把 tiff 编进库 |
| `RCS_TIFF_PREFER_STATIC` | ON | 静态链 libtiff，SDK 客户无需 `libtiff5` |

## 编译（Windows，含大图拼接）

与 [`207_RCS_code`](file:///e:/project/SAR-RCS/207_RCS/207_RCS_code) 一样：用 **Qt MinGW 7.3** + 工程内 `3rdparty/install/mingw73_64` 的 libtiff（已随仓库提供）。

**不要用 Visual Studio 生成器去链这套静态库**（那是 mingw 编的）；请：

```bat
REM 推荐一键
scripts\build_win.bat
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

仅库/不要拼接时可 `-DRCS_ENABLE_MOSAIC=OFF`（可不依赖 TIFF）。MSVC 需另备 libtiff（如 vcpkg），且 GUI 要用对应的 Qt msvc 套件。## 界面用法

1. 顶部**一次**选择含 `*_L0_*.dat` 的数据文件夹  
2. 在 **RCS测量 / HRRP一维距离像 / 点频RCS** 各页填参数并开始（共用同一输入目录）  
3. 拼接（仅 `RCS_ENABLE_MOSAIC=ON`）：独立 Tab，本页选择 TIFF/RAW 图目录  

## 终端用法

```bash
export LD_LIBRARY_PATH=$PWD/build_linux/lib:$LD_LIBRARY_PATH
./build_linux/cli/rcs_cli hrrp   --in <dat目录> --pol HH --out <输出>
./build_linux/cli/rcs_cli rcs    --in <dat目录> --pol HH --out <输出>
./build_linux/cli/rcs_cli cw-rcs --in <dat目录> --pol HH --out <输出>
```

## SDK 交付包

见 [`sdk/`](sdk/)：`.so`、头文件、`接口使用说明.md`、demo。刷新：

```bash
./sdk/pack_sdk.sh
```

## 公开 API

头文件：`lib/include/rcs/rcs_api.h`

- `processHrrp` / `processRcs` / `processCwRcs`：文件夹入参字段统一（`dataFolder/pol/outDir/...`）
- `processMosaic` 等：默认禁用  
- 进度与选点：`Callbacks`；CLI 用 `autoPick=true`


./3rdparty/scripts/build_linux_static.sh          # → install/linux_aarch64
cmake -S . -B build -DRCS_ENABLE_MOSAIC=ON -DRCS_TIFF_PREFER_STATIC=ON -DRCS_BUILD_GUI=OFF
cmake --build build -j && ./sdk/pack_sdk.sh