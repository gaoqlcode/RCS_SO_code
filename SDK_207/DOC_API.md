# SDK_207 接口使用文档

面向二次开发：RCS 像辐射定标 + 大图拼接。

| 项 | 说明 |
|----|------|
| 头文件 | `#include "rcs/rcs.hpp"` |
| 链接 | `-I.../include -L.../lib -lrcs_proc -Wl,-rpath,.../lib -pthread` |
| 路径 | UTF-8 |
| 返回值 | `process*` 失败/取消返回 `false`，可读说明写入 `*err`（可为 `nullptr`） |
| 坐标系 | 行 `y`/`nr`=距离向（第 1 行=远端）；列 `x`/`na`=方位向；ROI/偏移均为**全分辨率**像素 |
| 产品固定 | 定标始终写 `*_RCS_dBsm.tif`（`DN = round(dBsm×100)`）+ 三级 `.txt`（dBsm 真值）；拼接始终匀光 + `MOSAIC_L3.txt`（`dBsm = DN/100`），**不写** `MOSAIC_RCS.tif`。对应开关/缩放字段**不在请求结构体中**，调用方无需也无法配置 |
| 运行库 | 客户只链 `-lrcs_proc`；ToZeroData 已**静态合入**主库，无需另附 `libToZeroDataCpp11.so` |

本包公开接口：`rcsVersion`、`processRcsCalTif`、`processMosaic`、`flattenMosaic`、`prepareMosaicInputs`、`loadMosaicGrayPreview`、`listMosaicTiffs`、`processToZeroData`、`toZeroDataOutputPaths`、`toZeroDataRecoveryPath`。动态库内其它符号请勿调用。

可运行 GUI 示例：`demo/`（`bash demo/build_gui.sh` → `bash demo/run_gui.sh`）。

---

## 1. 接口说明

### 1.1 `rcsVersion`

```cpp
const char *rcsVersion();
```

返回库版本字符串（静态存储，勿 `free`），例如 `"1.0.0"`。

---

### 1.2 `processRcsCalTif` — RCS 像辐射定标

```cpp
bool processRcsCalTif(const RcsCalTifRequest &req,
                      RcsCalTifResponse &resp,
                      Callbacks &cb,
                      std::string *err = 0);
```

**功能**

1. `doCalib=true`：读 `calibRawPath`（含角反），`autoPick=false` 时回调 `onRectRoi` 框选角反缓冲区（或 `autoPick=true` 用 `calBufRoi`），积分求 ε、K、`R_near` 等。  
2. 扫描 `rawFolder` 中带 `Nr…Na…`、匹配 `band`/`pol` 的 `.raw`。  
3. 写出 int16 TIFF（`DN = round(dBsm × 100)`）与三级 `.txt`（dBsm 真值）。

**参数**

| 参数 | 说明 |
|------|------|
| `req` | 见 §2.2 `RcsCalTifRequest` |
| `resp` | 见 §2.3 `RcsCalTifResponse` |
| `cb` | 见 §3；GUI 下 `autoPick=false` 时必须实现 `onRectRoi` |
| `err` | 可选，失败说明 |

**输入 RAW**

| 项 | 约定 |
|----|------|
| 格式 | 无头 `uint16` 小端，行优先 `pixels[r*na+c]`，共 `nr×na` |
| 文件名 | 须含 `Nr{行}Na{列}`（大小写不敏感），例 `Image_Nr2048Na4602_X_HH.raw` |
| 定标景 | `calibRawPath`：含角反射器的一幅 RAW |
| 批量目录 | `rawFolder`：扫描 `.raw`/`.RAW`；文件名须含 `band`；`pol` 非空时还须含极化串 |

**输出**（写到 `outDir`，空则 `rawFolder`）

| 文件 | 说明 |
|------|------|
| `{原名去扩展名}_RCS_dBsm.tif` | Classic TIFF，int16；还原 `dBsm = DN / 100` |
| `{tgtName}{时间戳}.txt` | 三级产品（dBsm 真值）；冲突时追加原名等后缀 |

---

### 1.3 `processMosaic` — 大图拼接

```cpp
bool processMosaic(const MosaicRequest &req,
                   MosaicResponse &resp,
                   Callbacks &cb,
                   std::string *err = 0);
```

**功能**

- 优先用目录内 TIFF/JPEG；仅有 RAW 则先转 TIFF；至少 **2** 张；忽略文件名以 `MOSAIC` 开头的产物。  
- `mode=0` 自动配准；`mode=1` 使用 `offR`/`offC`。  
- 写出 `MOSAIC.tif`、`MOSAIC_显示.tif`、`MOSAIC_匀光.tif`、`MOSAIC_L3.txt`。  
- 推荐输入：定标产物 `*_RCS_dBsm.tif`；三级还原固定 `dBsm = DN / 100`（与定标 TIFF 缩放配对）。

**输入目录 `inFolder`**

| 情况 | 规则 |
|------|------|
| 有 TIFF/JPEG | 直接使用（`tif/tiff/jpg/jpeg`） |
| 仅有 RAW | 转到 `outFolder/.../raw2tif/*.tif` 再拼 |
| TIFF+RAW 同目录 | **只用 TIFF** |
| 数量 | ≥2 |

**输出**（`outFolder` 空则旁路自动建目录）

| 文件 | 说明 |
|------|------|
| `MOSAIC.tif` | 显示链融合（uint16） |
| `MOSAIC_显示.tif` | 拼后显示版 |
| `MOSAIC_匀光.tif` | 匀光结果（必出） |
| `MOSAIC_L3.txt` | 三级 RCS（必出）；`dBsm = DN / 100` |
| `MOSAIC_RCS.tif` | 本交付不写，`resp.rcsTif` 为空 |

---

### 1.4 `flattenMosaic` — 单独匀光

```cpp
bool flattenMosaic(const std::string &inTif,
                   const std::string &outTif,
                   double medOut,
                   const std::vector<int> &seamCols,
                   Callbacks &cb,
                   std::string *err = 0);
```

对已有拼接 TIFF 单独匀光（不重新配准）。`medOut` 常用 `0.35`；`seamCols` 可来自 `MosaicResponse::seamCols`，可空。

---

### 1.5 手动拼接辅助（mode=1）

```cpp
bool prepareMosaicInputs(const std::string &inFolder,
                         const std::string &outFolder,
                         std::vector<std::string> &tiffPaths,
                         Callbacks &cb, std::string *err = 0,
                         int rawNrHint = 0, int rawNaHint = 0);

bool loadMosaicGrayPreview(const std::string &path, int step,
                           GrayImage &preview, int &fullW, int &fullH,
                           std::string *err = 0);

std::vector<std::string> listMosaicTiffs(const std::string &folder);
```

| 接口 | 用途 |
|------|------|
| `prepareMosaicInputs` | 整理待拼 TIFF 列表（有 TIFF 直接用；仅 RAW 则先转） |
| `loadMosaicGrayPreview` | 加载预览供界面摆位；`step` 为降采样步长（≥1） |
| `listMosaicTiffs` | 列出目录内可拼 TIFF/JPEG（已排除 `MOSAIC*`；不扫 RAW） |

---

### 1.6 `processToZeroData` — 源 DAT → 四极化 L0

```cpp
bool processToZeroData(const ToZeroDataRequest &req,
                       ToZeroDataResponse &resp,
                       Callbacks &cb,
                       std::string *err = 0);

std::vector<std::string> toZeroDataOutputPaths(const std::string &outputBasePath);
std::string toZeroDataRecoveryPath(const std::string &outputBasePath);
```

**功能**

- 将源 DAT 转为 HH / VV / HV / VH 四个 L0 DAT（由 `outputBasePath` 派生路径）。  
- `saveRecovery=true` 时额外写 `.sarrec` 还原信息与源 SHA-256。  
- 同步阻塞；建议实现 `onProgress` / `onMessage` / `isCancelled`。  
- 已有输出或路径冲突时拒绝覆盖；失败/取消清理本次半成品。  
- 实现来自预编译 `libToZeroDataCpp11.a`，已静态链入 `librcs_proc`；客户**不要**再单独链 ToZeroData。

**辅助**

| 接口 | 说明 |
|------|------|
| `toZeroDataOutputPaths` | 返回 4 个派生 DAT 路径（顺序 HH,VV,HV,VH） |
| `toZeroDataRecoveryPath` | 返回 `.sarrec` 路径 |

---

## 2. 字段 / 参数定义

### 2.1 基础类型与枚举

**`RectRoi`**：全分辨率矩形。`x`=方位列，`y`=距离行，`w`/`h`=宽高；`valid()` ⇔ `w>0 && h>0`。

**`GrayImage`**：8bit 预览。`width`×`height`，`pixels[y*width+x]`。

**`Level3Info`**（可选）：覆盖三级 txt 文件头；字段均为字符串，空则用库默认。常用：`taskName`、`tgtName`、`pol`、`fs`、`note` 等。GUI 可不填。

**`rcs::RcsCalAnchor`（`anchorMode`）**

| 值 | 含义 |
|----|------|
| `0` `RcsCalLastRow` | 末行入射角与 `hFlight` 求 `R_near` |
| `1` `RcsCalCenter` | 场景中心斜距 |
| `2` `RcsCalCrKnown` | 已知角反斜距 `rCalKnown` + 行号 `rowCrHint`（1-based） |

**`rcs::RcsCalL3Layout`（`l3Layout`）**

| 值 | 含义 |
|----|------|
| `0` `RcsCalL3Point` | 逐点：每行「行 列 dBsm」（行列 1-based） |
| `1` `RcsCalL3Row` | 逐行：一行内空格分隔各列 dBsm |

---

### 2.2 `RcsCalTifRequest`

| 字段 | 含义 | 单位 | 默认 | 必填 |
|------|------|------|------|------|
| `calibRawPath` | 含角反的定标 RAW | 路径 | 空 | `doCalib=true` 时 |
| `rawFolder` | 待批量定标 RAW 目录 | 路径 | 空 | **是** |
| `outDir` | 输出目录；空=`rawFolder` | 路径 | 空 | 否 |
| `tgtName` | 三级文件名前缀 | — | `"目标"` | 否 |
| `band` | 波段筛文件 + 波束宽（W→7°，其它→10°） | — | `"X"` | 建议 |
| `pol` | 极化筛文件；**空=不按极化筛** | — | `"HH"` | 否 |
| `sigmaTheoryDb` | 角反理论 RCS | dBsm | 18.25 | 是 |
| `fsHz` | 采样率，`Rbin=c/(2·fs)` | Hz | 1.25e9 | 是 |
| `incAngleDeg` | 入射角（求 `R_near`） | ° | 70 | 求 K 时 |
| `hFlight` | 飞行高度 | m | 500 | 求 K 时 |
| `anchorMode` | 见 §2.1 | — | 0 | 否 |
| `rCalKnown` | 已知斜距（仅 mode=2） | m | 0 | mode=2 |
| `rowCrHint` | 角反行 1-based（仅 mode=2） | 行 | 0 | mode=2 |
| `doCalib` | true=框选求 K；false=用预填 K | — | true | GUI=true |
| `l3Layout` | 见 §2.1 | — | 0 | 否 |
| `maxFiles` | 最多文件数；≤0 全部 | 个 | 0 | 否 |
| `autoPick` | false=`onRectRoi`；true=`calBufRoi` | — | false | GUI=false |
| `calBufRoi` | 角反缓冲区（全分辨率） | 像素 | 空 | autoPick=true |
| `K` / `R_near` / `Rbin` | 预填定标量（`doCalib=false`） | — / m / m | 0 | doCalib=false |
| `header` | 覆盖三级头 | — | 空 | 否 |

说明：无 `writeTif` / `writeTxt` / `dbScale` 字段；输出与缩放由库内固定。

---

### 2.3 `RcsCalTifResponse` / `RcsCalTifFileResult`

| 字段 | 含义 | 单位 |
|------|------|------|
| `K` | 定标系数 | — |
| `R_near` / `Rbin` / `R_cal` | 近距 / 采样间隔 / 角反斜距 | m |
| `epsilon_cal` | 角反净能量 ε | — |
| `sigma_cal` | 理论 RCS 线性 `10^(dBsm/10)` | m² |
| `peakRow` / `peakCol` | 角反峰（0-based） | 像素 |
| `preview` | 定标景 8bit 预览 | — |
| `files[].rawName` | 源 RAW 名 | — |
| `files[].tifPath` / `txtPath` | 输出路径；失败可空 | 路径 |
| `files[].nr` / `na` | 尺寸 | 像素 |
| `files[].dbsmMin` / `dbsmMax` | 本景 dBsm 范围 | dBsm |
| `files[].note` | 跳过/失败原因 | — |

---

### 2.4 `MosaicRequest`

| 字段 | 含义 | 单位 | 默认 | 必填 |
|------|------|------|------|------|
| `inFolder` | 输入 TIFF/RAW 目录 | 路径 | 空 | **是** |
| `outFolder` | 输出目录；空则自动旁路目录 | 路径 | 空 | 建议 |
| `medOut` | 匀光目标中值；≤0 按 0.35 | 约 0~1 | 0.35 | 否 |
| `mode` | 0 自动；1 手动 `offR`/`offC` | — | 0 | — |
| `offR` / `offC` | 各图绝对偏移（全分辨率） | 像素 | 空 | mode=1，长度=图数 |
| `rawNrHint` / `rawNaHint` | RAW 转 TIFF 尺寸提示；0=自动 | 像素 | 0 | 否 |
| `l3Layout` | 见 §2.1 | — | 0 | 否 |
| `band` / `pol` | 写入三级头 | — | X / HH | 否 |
| `header` | 覆盖三级头 | — | 空 | 否 |

说明：无 `doFlatten` / `writeRcsTif` / `writeL3` / `l3Scale` 字段；匀光、L3 输出与缩放由库内固定。

---

### 2.5 `MosaicResponse`

| 字段 | 含义 |
|------|------|
| `mosaicTif` | `MOSAIC.tif` |
| `flattenTif` | `MOSAIC_匀光.tif`（本交付应非空） |
| `displayTif` | `MOSAIC_显示.tif` |
| `rcsTif` | 本交付恒为空 |
| `l3Path` | `MOSAIC_L3.txt` |
| `seamCols` | 接缝列，可供 `flattenMosaic` 复用 |

---

### 2.6 `ToZeroDataRequest` / `ToZeroDataResponse`

| 字段 | 含义 | 默认 | 必填 |
|------|------|------|------|
| `sourcePath` | 源 DAT | 空 | **是** |
| `outputBasePath` | 输出基路径（派生四极化） | 空 | **是** |
| `saveRecovery` | 是否写 `.sarrec` | false | 否 |

| 响应字段 | 含义 |
|----------|------|
| `validRecords` / `skippedRecords` | 有效 / 跳过记录数 |
| `outputBytes` | 四个 DAT 合计字节 |
| `outputs[]` | `polarization` / `path` / `frames` / `bytes` |
| `recoveryPath` / `recoveryBytes` / `sourceSha256` | 仅 `saveRecovery` 成功时有意义 |

---

## 3. 回调实现

`Callbacks` 与完整库 ABI 一致，**成员顺序勿改**。其它 `onCorner*` / `onTarget*` 为占位，可不赋值。

| 成员 | GUI | 说明 |
|------|-----|------|
| `onProgress(int percent, string msg)` | 建议 | 0～100 + 步骤说明 |
| `isCancelled()` | 建议 | 返回 true 尽快中止 |
| `onMessage(string)` | 可选 | 信息日志 |
| `onError(string)` | 可选 | 错误日志 |
| `onRectRoi(preview, fullNr, fullNa, stage, out)` | 定标必填 | `stage` 多为 `"cal"`；`out` 须全分辨率 |

### `onRectRoi` 坐标换算（重要）

`preview` 为降采样 8bit 图。用户在预览上框选 `(px,py,pw,ph)` 后，换算为全分辨率：

```cpp
double sx = (double)fullNa / preview.width;
double sy = (double)fullNr / preview.height;
out.x = (int)std::floor(px * sx);
out.y = (int)std::floor(py * sy);
out.w = std::max(1, (int)std::floor(pw * sx));
out.h = std::max(1, (int)std::floor(ph * sy));
return out.valid();
```

`demo/RectPickDialog` 已按上式换算，回调里直接使用全分辨率矩形赋给 `out` 即可。

---

## 4. GUI 调用方式（与本工程 Worker / demo 一致）

在 **工作线程** 调库；需要框选时通过信号回到 **主线程** 弹对话框，再把结果写回 `out`。

可运行示例：`demo/CalMosaicWorker.cpp`、`demo/MainWindow.cpp`。

### 4.1 辐射定标（框选角反）

```cpp
#include "rcs/rcs.hpp"

RcsCalTifRequest req;
req.calibRawPath = calibRaw.toStdString();  // 含角反 RAW
req.rawFolder    = rawDir.toStdString();
req.outDir       = outDir.toStdString();
req.tgtName      = tgtName.toStdString();
req.band = band.toStdString();
req.pol  = pol.toStdString();               // 空串=不筛极化
req.sigmaTheoryDb = 18.25;
req.fsHz = 1.25e9;
req.incAngleDeg = 70.0;
req.hFlight = 500.0;
req.maxFiles = 0;
req.doCalib = true;
req.autoPick = false;                       // GUI：走 onRectRoi
req.l3Layout = rcs::RcsCalL3Point;

Callbacks cb;
cb.onProgress = [&](int p, const std::string &m) {
    // emit 到 UI：进度条 + 状态文字
};
cb.onMessage = [&](const std::string &m) {
    // 日志面板
};
cb.isCancelled = [&]() { return userCancelled; };
cb.onRectRoi = [&](const GrayImage &g, int fullNr, int fullNa,
                   const std::string &stage, RectRoi &out) -> bool {
    // 1) 把 g 转成 QImage，主线程弹出框选对话框
    // 2) 对话框返回全分辨率矩形（已换算）
    // 3) 写入 out
    out = RectRoi(r.x(), r.y(), r.width(), r.height());
    return out.valid();   // 用户取消则 return false
};

RcsCalTifResponse resp;
std::string err;
if (!processRcsCalTif(req, resp, cb, &err)) {
    // 失败或取消：显示 err
    return;
}
// 成功：resp.K；遍历 resp.files[].tifPath / txtPath
```

### 4.2 大图拼接（自动）

```cpp
MosaicRequest req;
req.inFolder  = inDir.toStdString();    // ≥2 张 *_RCS_dBsm.tif
req.outFolder = outDir.toStdString();
req.mode = 0;                           // 自动
req.medOut = 0.35;
req.rawNrHint = 0;
req.rawNaHint = 0;

Callbacks cb;
cb.onProgress = [&](int p, const std::string &m) { /* 进度+日志 */ };
cb.isCancelled = [&]() { return userCancelled; };

MosaicResponse resp;
std::string err;
if (!processMosaic(req, resp, cb, &err)) {
    // 失败
    return;
}
// resp.mosaicTif / flattenTif / displayTif / l3Path
```

### 4.3 手动拼接（mode=1）

```cpp
Callbacks cb;
// ... onProgress / isCancelled 同上

std::vector<std::string> paths;
std::string err;
if (!prepareMosaicInputs(inFolder, outFolder, paths, cb, &err, rawNrHint, rawNaHint))
    return;

// 对每张图：
GrayImage preview;
int fullW = 0, fullH = 0;
loadMosaicGrayPreview(paths[i], /*step=*/8, preview, fullW, fullH, &err);
// 界面摆位，得到各图相对画布的绝对偏移 offR[i], offC[i]

MosaicRequest req;
req.inFolder = inFolder;
req.outFolder = outFolder;
req.mode = 1;
req.offR = offR;   // size == paths.size()
req.offC = offC;
req.medOut = 0.35;

MosaicResponse resp;
processMosaic(req, resp, cb, &err);
```

### 4.4 单独匀光

```cpp
std::string out = inTif.substr(0, inTif.find_last_of('.')) + "_匀光.tif";
flattenMosaic(inTif, out, /*medOut=*/0.35, /*seamCols=*/{}, cb, &err);
```

### 4.5 ToZeroData

```cpp
ToZeroDataRequest req;
req.sourcePath = "/data/source.dat";
req.outputBasePath = "/data/out/source_L0.dat";
req.saveRecovery = true;

Callbacks cb;
cb.onProgress = [&](int p, const std::string &m) { /* ... */ };
cb.onMessage = [&](const std::string &m) { /* ... */ };
cb.isCancelled = [&]() { return userCancelled; };

ToZeroDataResponse resp;
std::string err;
if (!processToZeroData(req, resp, cb, &err)) {
    // 失败或取消
    return;
}
// resp.outputs / resp.recoveryPath / resp.sourceSha256
```

---

## 5. 编译链接（Qt / g++）

```bash
# GUI demo
bash demo/build_gui.sh
bash demo/run_gui.sh
```

```bash
g++ your_app.cpp -I/path/to/SDK_207/include -L/path/to/SDK_207/lib \
    -lrcs_proc -Wl,-rpath,/path/to/SDK_207/lib -pthread -o your_app
```

Qt `.pro` 示例（见 `demo/demo_gui.pro`）：

```qmake
INCLUDEPATH += /path/to/SDK_207/include
LIBS += -L/path/to/SDK_207/lib -lrcs_proc
QMAKE_RPATHDIR += /path/to/SDK_207/lib
```

运行时需能加载 `librcs_proc.so*`（`LD_LIBRARY_PATH` 或 rpath）。ToZeroData 已静态合入，无额外 so。

---

## 6. 常见错误

| 现象 | 原因 |
|------|------|
| `rawFolder 为空` / 目录不存在 | 路径未填或错误 |
| `定标 RAW 文件名缺少 Nr…Na…` | 文件名无 `Nr数字Na数字` |
| `需要 onRectRoi` | `autoPick=false` 未实现回调 |
| `未找到匹配波段/极化…` | `band`/`pol` 与文件名不一致 |
| `至少需要2张图像` | 拼接输入不足 |
| 框选无效 / K 异常 | ROI 写成了预览坐标而非全分辨率 |
| ToZeroData 路径冲突 / 拒绝覆盖 | 目标 DAT 或 `.sarrec` 已存在 |
