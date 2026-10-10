# Demo

两套例子，依赖 `sdk/include`、`sdk/lib`；gui 还要 Qt5。

| 目录 | 程序 | 说明 |
|------|------|------|
| `cli_demo/` | `rcs_cli_demo` | 命令行：探查、预览、交互选点、无界面冒烟 |
| `gui_demo/` | `rcs_gui_demo` | Qt：后台线程调库、弹窗确认峰距、画曲线 |

业务默认 `autoPick=false`，靠回调选点。cli 里部分命令开了 `autoPick=true`，只为没有界面时跑通。

```bash
# 仓库根，先打库
cmake --build build -j --target rcs_proc
./sdk/pack_sdk.sh

cd sdk/demo/cli_demo && cmake -S . -B build && cmake --build build -j
export LD_LIBRARY_PATH=$PWD/../../lib:$LD_LIBRARY_PATH
./build/rcs_cli_demo list /数据目录
./build/rcs_cli_demo hrrp-i /数据 /输出 HH

cd ../gui_demo && cmake -S . -B build && cmake --build build -j
./build/rcs_gui_demo
```

子目录里各有一份 README。接口说明见 `../接口使用说明.md`。
