# RCS 处理库 SDK（交付包）

本目录可单独拷给二次开发方使用，不必附带整份工程源码。

| 内容 | 说明 |
|------|------|
| `lib/` | 已编译的 `librcs_proc.so`（当前为 x86_64 Linux） |
| `include/rcs/` | 公开 C++ 头文件 |
| `接口使用说明.md` | 接口说明、参数、回调约定 |
| `demo/` | 最小可编译示例 |

## 运行依赖

- Linux x86_64
- C++11 及以上编译器（`libstdc++` / `glibc` 等系统自带即可）
- **不需要** `apt install libtiff5`：交付用的 `librcs_proc.so` 已把 libtiff 静态编进库内

开发机若要从源码编带拼接的库（x86 / ARM 同一套，静态编进库）：

```bash
# 本机架构 → install/linux_x86_64 或 linux_aarch64
./3rdparty/scripts/build_linux_static.sh
# ARM 交叉（在 x86 上）: ./3rdparty/scripts/build_linux_static.sh aarch64
cmake -S . -B build -DRCS_ENABLE_MOSAIC=ON -DRCS_TIFF_PREFER_STATIC=ON
cmake --build build -j && ./sdk/pack_sdk.sh
```

ARM 细节见 `3rdparty/README.md`。
## 业务接口

- `processHrrp` / `processRcs` / `processCwRcs`（点频）
- `processMosaic` 等大场景接口（本交付包若已开 mosaic；否则返回「已禁用」）

## 快速试跑 demo

```bash
cd demo
cmake -S . -B build
cmake --build build
# 仅打印版本
./build/rcs_demo
# 对一份数据跑 HRRP（自动选峰，不交互）
./build/rcs_demo hrrp /path/to/dat_folder /path/to/out_dir HH
```

链接时把 `../lib` 加到 `rpath` 或设：

```bash
export LD_LIBRARY_PATH=$PWD/../lib:$LD_LIBRARY_PATH
```

## 版本

与主工程 `rcsVersion()` 一致（当前 `1.0.0`）。

重新打包（在工程根目录、先编好 `build_linux`）：

```bash
./sdk/pack_sdk.sh
```
