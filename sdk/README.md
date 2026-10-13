# RCS 处理库 SDK

二次开发用：动态库 + 头文件。界面自己做，算法在这边。

| 内容 | 说明 |
|------|------|
| `lib/` | `librcs_proc.so` / Windows DLL |
| `include/rcs/` | 公开头（完整接口，含大图拼接） |
| `rcs.hpp` | `#include "rcs/rcs.hpp"` 一次拉齐 |
| `接口使用说明.md` | 接口说明与功能分组 |
| `demo/cli_demo/` | 命令行例子 |
| `demo/gui_demo/` | Qt 例子 |

## 功能分组（交付时按组取舍）

头文件 `rcs_api.h` 已按组编号。交付某套功能时，保留对应组的声明与类型即可；库建议始终编完整版，未交付功能甲方不调用。

| 组 | 功能 | 保留接口 | 相关类型 |
|----|------|----------|----------|
| [0] | 通用 | `rcsVersion` | — |
| [1] | HRRP 一维距离像 | `processHrrp` | `HrrpRequest` / `HrrpResponse` |
| [2] | RCS 测量 | `processRcs` | `RcsRequest` / `RcsResponse` / `RcsTargetItem` |
| [3] | 点频 RCS | `processCwRcs` | `CwRcsRequest` / `CwRcsResponse` |
| [4] | 后向散射 σ⁰ | `processSigma0` | `Sigma0Request` / `Sigma0Response` / ROI |
| [5] | 0 级预览 | `previewL0Folder` / `previewL0File` | `L0Preview*` / `L0PulsePreviewItem` |
| [6] | 数据探查 | `listL0Files`、`probe*`、`parseImagedRawSize`、`defaultOutDir` 等 | `FileHeaderInfo`、`L0FolderProbe` |
| [7] | RAW 预览辅助 | `loadImagedRawPreview`、`mapPreview*` | 配合 [4] |
| [8] | 大图拼接 | `processMosaic`、`flattenMosaic`、`listMosaicTiffs`、`loadMosaicGrayPreview`、`prepareMosaicInputs` | `MosaicRequest` / `MosaicResponse` |
| [9] | RCS 像辐射定标 | `processRcsCalTif` | `RcsCalTifRequest` / `RcsCalTifResponse` |

常用组合示例：

- 只做 RCS/HRRP：保留 [0][1][2][6]，可选 [5]
- 加后向散射：再加 [4][7]
- 加整景 dBsm TIFF/三级 txt：再加 [9]
- 完整交付：全部 [0]~[9]

默认 `autoPick=false`，业务接口都要传 `Callbacks`。完整库始终含大图拼接。

```cpp
#include "rcs/rcs.hpp"

HrrpRequest req;
req.dataFolder = "/data/in";
req.outDir = "/data/out";

Callbacks cb;
cb.onCornerConfirm = [](const std::vector<double> &R, const std::vector<double> &db,
                        int peakIdx, InteractSelection &sel) {
    return true;
};

HrrpResponse resp;
std::string err;
processHrrp(req, resp, cb, &err);
```

Demo 怎么编见 `demo/README.md`。

内部重新打包装库（完整含拼接）：

```bash
cmake -S . -B build_linux -DRCS_BUILD_GUI=OFF
cmake --build build_linux -j && ./sdk/pack_sdk.sh
```
