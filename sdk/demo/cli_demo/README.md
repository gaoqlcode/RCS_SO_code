# cli_demo

程序：`rcs_cli_demo`。代码在 `main.cpp`。

默认 `autoPick=false`。`hrrp` / `rcs` / `cw` / `sigma0` 开了 `autoPick`，方便没界面时冒烟；做产品别这么干。

## 编译

```bash
# 仓库根
cmake --build build -j --target rcs_proc
./sdk/pack_sdk.sh

cd sdk/demo/cli_demo
cmake -S . -B build
cmake --build build -j
export LD_LIBRARY_PATH=$PWD/../../lib:$LD_LIBRARY_PATH
```

## 命令

```bash
./build/rcs_cli_demo list /数据目录
./build/rcs_cli_demo probe /数据目录 HH
./build/rcs_cli_demo preview /数据目录 HH

# 交互：终端确认角反峰距（对应界面里的 onCornerConfirm）
./build/rcs_cli_demo hrrp-i /数据目录 /输出目录 HH

# 无 UI 冒烟
./build/rcs_cli_demo hrrp /数据 /输出 HH
./build/rcs_cli_demo rcs  /数据 /输出 HH
./build/rcs_cli_demo cw   /数据 /输出 HH
./build/rcs_cli_demo sigma0 /path/Image_Nr....raw /输出 100,80,40,40 200,120,80,60
```

`sigma0` 的框是全分辨率 `x,y,w,h`（x 方位、y 距离）。界面里应走 `onRectRoi` / `onTargetRois`，预览坐标用 `mapPreviewRoiToFull` 转。

预览出来的 `tUs/amp`、`fMHz/spectrumDb` 自己画；HRRP 用 `rangeM` / `rcsDbsm`。

## 链接

```bash
g++ app.cpp -I/path/to/sdk/include -L/path/to/sdk/lib -lrcs_proc \
    -Wl,-rpath,/path/to/sdk/lib -pthread -o app
```

`#include "rcs/rcs.hpp"`。详见 `../../接口使用说明.md`，Qt 例子在 `../gui_demo/`。
