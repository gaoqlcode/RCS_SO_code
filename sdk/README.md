# RCS 处理库 SDK

二次开发用：动态库 + 头文件。界面自己做，算法在这边。

| 内容 | 说明 |
|------|------|
| `lib/` | `librcs_proc.so` |
| `include/rcs/` | 公开头（不含大图拼接） |
| `rcs.hpp` | `#include "rcs/rcs.hpp"` 一次拉齐 |
| `接口使用说明.md` | 接口说明 |
| `demo/cli_demo/` | 命令行例子 |
| `demo/gui_demo/` | Qt 例子 |

业务：`processHrrp` / `processRcs` / `processCwRcs` / `processSigma0` / `previewL0Folder` / `previewL0File`，都要传 `Callbacks`。  
探查、预览辅助见头文件。默认 `autoPick=false`。拼接不交付。

```cpp
#include "rcs/rcs.hpp"

HrrpRequest req;
req.dataFolder = "/data/in";
req.outDir = "/data/out";

Callbacks cb;
cb.onCornerConfirm = [](const std::vector<double> &R, const std::vector<double> &db,
                        int peakIdx, InteractSelection &sel) {
    // 弹窗确认峰距
    return true;
};

HrrpResponse resp;
std::string err;
processHrrp(req, resp, cb, &err);
```

Demo 怎么编见 `demo/README.md`。

内部重新打包装库：

```bash
cmake -S . -B build -DRCS_ENABLE_MOSAIC=OFF -DRCS_BUILD_GUI=OFF
cmake --build build -j && ./sdk/pack_sdk.sh
```
