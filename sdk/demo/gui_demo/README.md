# gui_demo

程序：`rcs_gui_demo`。只链 SDK + Qt5，不拉工程里那套完整界面。

大致就是：Worker 线程调库、进度/日志 signal 回主线程、选点用 `BlockingQueuedConnection` 弹窗、曲线自己画。

## 编译

```bash
cmake --build build -j --target rcs_proc   # 仓库根
./sdk/pack_sdk.sh

cd sdk/demo/gui_demo
cmake -S . -B build
cmake --build build -j
export LD_LIBRARY_PATH=$PWD/../../lib:$LD_LIBRARY_PATH
./build/rcs_gui_demo
```

需要 Qt5 Widgets。

## 用法

选含 `*_L0_*.dat` 的目录 → 扫极化 → 探查 / 预览 / HRRP。  
HRRP 跑着会弹窗确认峰距。取消走 `isCancelled`。预览 demo 里只算前 8 个文件。

## 文件

- `DemoWorker.cpp` — 填请求、挂回调、调 `process*` / `preview*`
- `ConfirmBridge.*` — Worker 里回调切回 UI 弹窗
- `MainWindow.cpp` — 线程和控件
- `SimplePlotWidget.*` — 把 `QVector` 画成线，可换成别的图库

选点流程：`processHrrp` → `onCornerConfirm` → `invokeMethod(ConfirmBridge, BlockingQueuedConnection)` → 写 `InteractSelection`。  
RCS / 点频 / 后向散射同理，换成 `onCornerPick` / `onTargetPick` / `onRectRoi` 即可。

命令行例子见 `../cli_demo/`，接口说明 `../../接口使用说明.md`。
