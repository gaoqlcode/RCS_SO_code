# RCS 处理库 SDK

给二次开发用的动态库包。界面软件由对方自己做，这里只提供算法库、头文件和说明。

| 内容 | 说明 |
|------|------|
| `lib/` | `librcs_proc.so` |
| `include/rcs/` | 公开头文件（**不含**大图拼接接口） |
| `接口使用说明.md` | 对接说明，做 UI 时先看这个 |
| `demo/` | 命令行示例（HRRP / RCS / 点频） |

## 本包提供的业务

- `processHrrp` — 一维距离像  
- `processRcs` — RCS 测量  
- `processCwRcs` — 点频 RCS  
- `rcsVersion` — 版本字符串  

大场景拼接相关接口**不交付**，请勿按旧资料去调用。

## 运行环境

- 与所编 `.so` 匹配的 Linux 架构（x86_64 或 aarch64，以实际交付为准）
- C++11 编译器；运行时需要常见的 `libstdc++` / `glibc`
- 一般**不必**再装 libtiff（客户包按关闭拼接、不链系统 tiff 的方式打包）

## 编一下 demo

```bash
cd demo
cmake -S . -B build
cmake --build build

./build/rcs_demo
./build/rcs_demo hrrp /数据目录 /输出目录 HH
./build/rcs_demo rcs  /数据目录 /输出目录 HH
./build/rcs_demo cw   /数据目录 /输出目录 HH
```

若找不到 `.so`：

```bash
export LD_LIBRARY_PATH=$PWD/../lib:$LD_LIBRARY_PATH
```

更细的参数、回调、线程注意点见 `接口使用说明.md`。

## 内部重新打包（我们这边用）

客户交付建议关拼接再编库，避免头文件与符号对不齐：

```bash
cmake -S . -B build -DRCS_ENABLE_MOSAIC=OFF -DRCS_BUILD_GUI=OFF
cmake --build build -j
./sdk/pack_sdk.sh
```

`pack_sdk.sh` 会更新 `lib/` 里的 so，并保留本目录下已整理过的对外头文件（不会把内部 mosaic 声明拷进客户包）。
