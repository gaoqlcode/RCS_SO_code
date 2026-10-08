# 第三方库（静态 libtiff，编进 librcs_proc）

| 平台 | 目录 | 说明 |
|------|------|------|
| Windows MinGW 7.3 | `install/mingw73_64` | 与 207_RCS_code 相同 |
| Linux x86_64 | `install/linux_x86_64` | PC / 服务器 |
| Linux aarch64 | `install/linux_aarch64` | ARM64 板卡 / 服务器 |

**架构必须匹配**：不能把 `linux_x86_64` 的 `.a` 链进 ARM 的 `librcs_proc.so`。

## 源码（编静态库前必须有）

目录：`3rdparty/src/zlib-1.3.1`、`3rdparty/src/tiff-4.6.0`。

```bash
# ARM/新机器上若没有源码，联网下载：
chmod +x 3rdparty/scripts/*.sh
./3rdparty/scripts/fetch_sources.sh

# 或从 207 工程拷贝：
#   cp -a /path/to/207_RCS_code/3rdparty/src/zlib-1.3.1 3rdparty/src/
#   cp -a /path/to/207_RCS_code/3rdparty/src/tiff-4.6.0  3rdparty/src/
```

## 生成静态库

```bash
./3rdparty/scripts/build_linux_static.sh          # 本机 → linux_x86_64 或 linux_aarch64
# 交叉 ARM64（x86 主机）:
#   sudo apt install g++-aarch64-linux-gnu
#   ./3rdparty/scripts/build_linux_static.sh aarch64
```

## ARM 上编主工程

```bash
./3rdparty/scripts/fetch_sources.sh               # 若还没有 src
./3rdparty/scripts/build_linux_static.sh          # → install/linux_aarch64
cmake -S . -B build -DRCS_ENABLE_MOSAIC=ON -DRCS_TIFF_PREFER_STATIC=ON -DRCS_BUILD_GUI=OFF
cmake --build build -j
./sdk/pack_sdk.sh
```
