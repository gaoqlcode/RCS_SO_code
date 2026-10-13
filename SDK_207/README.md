# SDK_207 — 辐射定标 + 大图拼接 + ToZeroData

面向 207 交付的精简 SDK：头文件**只声明**下列接口，二次开发勿调用其它符号。

| 组 | 功能 | 接口 |
|----|------|------|
| [0] | 版本 | `rcsVersion` |
| [9] | RCS 像辐射定标 | `processRcsCalTif` |
| [8] | 大图拼接 | `processMosaic`、`flattenMosaic`、`listMosaicTiffs`、`loadMosaicGrayPreview`、`prepareMosaicInputs` |
| [10] | ToZeroData | `processToZeroData`、`toZeroDataOutputPaths`、`toZeroDataRecoveryPath` |

**客户开发请先读：[DOC_API.md](DOC_API.md)**。Word 版：`SDK_207接口使用文档.docx`。

## 目录

```
SDK_207/
  include/rcs/          公开头
  lib/
    librcs_proc.so*     唯一运行库（ToZeroData 已静态合入）
  DOC_API.md
  demo/                 GUI 示例（定标 / 拼接 / ToZero）
  pack_sdk_207.sh
  README.md
```

客户链接：`-lrcs_proc` 即可，**不要**再链 ToZeroData。

## 约定（摘要）

- 路径 UTF-8；`process*` 须传 `Callbacks&`
- **辐射定标**：固定写出 `*_RCS_dBsm.tif` + 三级 `.txt`；GUI 用 `autoPick=false` + `onRectRoi`
- **大图拼接**：固定匀光 + `MOSAIC_L3.txt`；不写 `MOSAIC_RCS.tif`
- **缩放配对（请求结构体无对应字段）**：定标 TIFF `DN=dBsm×100`；拼接三级 `dBsm=DN/100`
- **ToZeroData**：源 DAT → 四极化 L0；可选 `.sarrec`（实现已打进主库）

## 快速开始

```bash
bash demo/build_gui.sh
bash demo/run_gui.sh
```

## 刷新库文件

须在 **Linux** 上以 `RCS_ENABLE_TOZERODATA=ON`（默认）编译后再 pack；Windows 不编入 ToZeroData。

```bash
./scripts/build_linux.sh --no-gui          # 默认开启 ToZeroData
# ./scripts/build_linux.sh --no-gui --no-tozero   # 关闭（本包 pack 会失败）
./SDK_207/pack_sdk_207.sh
```
