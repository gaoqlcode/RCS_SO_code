#ifndef RCS_API_H
#define RCS_API_H

#include "rcs/callbacks.hpp"
#include "rcs/export.h"
#include "rcs/types.hpp"
#include <string>
#include <vector>

// =============================================================================
// SDK_207 公开 API（仅下列符号供二次开发）
//
//   [0] rcsVersion
//   [9] processRcsCalTif              辐射定标
//   [8] processMosaic / flattenMosaic / listMosaicTiffs /
//       loadMosaicGrayPreview / prepareMosaicInputs
//   [10] processToZeroData / toZeroDataOutputPaths / toZeroDataRecoveryPath
//
// 通用约定：
//   - 路径 UTF-8
//   - process* 须传 Callbacks&；失败/取消返回 false，可读说明写入 *err（可为 nullptr）
//   - 定标强制写 *_RCS_dBsm.tif + 三级 .txt；拼接强制匀光 + MOSAIC_L3.txt，不写 MOSAIC_RCS.tif
//   - 缩放配对固定且不出现在请求结构体：定标 DN=dBsm×100；拼接三级 dBsm=DN/100
//   - ToZeroData 已静态链入 librcs_proc；客户只链 -lrcs_proc，无需另附 ToZero so
// 字段表与 GUI 示例：DOC_API.md 、demo/
// =============================================================================

// ---------------------------------------------------------------------------
// [0] 通用
// ---------------------------------------------------------------------------

/**
 * 返回动态库版本字符串（静态存储，勿 free）。
 * @return 例如 "1.0.0"
 */
RCS_API const char *rcsVersion();

// ---------------------------------------------------------------------------
// [9] RCS 像辐射定标
// ---------------------------------------------------------------------------

/**
 * 由含角反的定标 RAW 求定标系数 K，并对目录内匹配的 RAW 批量辐射定标。
 *
 * 功能：
 *   1) doCalib=true：读 calibRawPath →（autoPick=false 时 onRectRoi 框角反缓冲，
 *      或 autoPick=true 用 calBufRoi）→ 积分求 ε、K、R_near 等；
 *   2) 扫描 rawFolder 中带 Nr/Na、匹配 band/pol 的 .raw；
 *   3) 写出 int16 TIFF（DN=dBsm×100，库内固定）与三级 txt（dBsm 真值）。
 *
 * 输入约定：RAW 无头 uint16 LE；文件名含 Nr{行}Na{列}，如 Image_Nr2048Na4602_X_HH.raw。
 * 输出：{stem}_RCS_dBsm.tif 、{tgtName}{时间戳}.txt（见 DOC_API.md）。
 *
 * @param req   请求参数（见 RcsCalTifRequest 字段注释）
 * @param resp  输出：K、几何量、各文件路径与统计
 * @param cb    回调；autoPick=false 时必须实现 onRectRoi
 * @param err   可选；失败时写入中文/UTF-8 说明
 * @return true 成功（部分单文件失败会记在 files[].note，仍可能返回 true）
 */
RCS_API bool processRcsCalTif(const RcsCalTifRequest &req, RcsCalTifResponse &resp, Callbacks &cb,
                              std::string *err = 0);

// ---------------------------------------------------------------------------
// [8] 大图拼接（完整库始终包含）
// ---------------------------------------------------------------------------

/**
 * 将输入目录中多张图自动/手动配准融合，并固定输出匀光图与三级 txt。
 *
 * 功能：
 *   - 优先使用目录内 TIFF/JPEG；若仅有 RAW 则先转 TIFF；至少 2 张；忽略 MOSAIC* 文件名；
 *   - mode=0 自动配准；mode=1 使用 offR/offC；
 *   - 写出 MOSAIC.tif / MOSAIC_显示.tif / MOSAIC_匀光.tif / MOSAIC_L3.txt；
 *   - 本交付不写 MOSAIC_RCS.tif。
 *
 * 推荐输入：定标产物 *_RCS_dBsm.tif；三级 dBsm=DN/100（与定标 TIFF 缩放库内固定配对）。
 *
 * @param req   请求参数（见 MosaicRequest 字段注释）
 * @param resp  输出路径与 seamCols
 * @param cb    进度/取消等回调
 * @param err   可选；失败说明
 * @return true 成功
 */
RCS_API bool processMosaic(const MosaicRequest &req, MosaicResponse &resp, Callbacks &cb,
                           std::string *err = 0);

/**
 * 对已有拼接 TIFF 单独做匀光（不重新配准）。
 *
 * @param inTif     输入 TIFF 路径（通常为 MOSAIC.tif）
 * @param outTif    输出匀光 TIFF 路径
 * @param medOut    匀光目标中值（归一化约 0~1，常用 0.35）
 * @param seamCols  接缝列；可来自 MosaicResponse::seamCols，可空
 * @param cb        回调
 * @param err       可选；失败说明
 * @return true 成功
 */
RCS_API bool flattenMosaic(const std::string &inTif, const std::string &outTif, double medOut,
                           const std::vector<int> &seamCols, Callbacks &cb, std::string *err = 0);

/**
 * 列出目录中可用于拼接的 TIFF/JPEG 路径（已排除文件名以 MOSAIC 开头的产物）。
 * 不扫描 RAW。
 *
 * @param folder  输入目录（UTF-8）
 * @return 绝对或拼接后的文件路径列表（已按名称中数字排序）
 */
RCS_API std::vector<std::string> listMosaicTiffs(const std::string &folder);

/**
 * 加载单张 TIFF 的 8bit 灰度预览（用于界面预览/手动对齐）。
 * 定标 int16 图会按分位拉伸显示。
 *
 * @param path     TIFF 路径
 * @param step     降采样步长，≥1；越大预览越小
 * @param preview  【输出】预览图
 * @param fullW    【输出】原图宽（方位列数）
 * @param fullH    【输出】原图高（距离行数）
 * @param err      可选；失败说明
 * @return true 成功
 */
RCS_API bool loadMosaicGrayPreview(const std::string &path, int step, GrayImage &preview, int &fullW,
                                   int &fullH, std::string *err = 0);

/**
 * 仅准备拼接输入列表：有 TIFF 则直接列出；仅有 RAW 则转到 outFolder/.../raw2tif/*.tif。
 * 不执行配准融合（供手动对齐 UI 等先拿路径）。
 *
 * @param inFolder   输入目录
 * @param outFolder  输出根目录（RAW 转 TIFF 工作目录基于此）
 * @param tiffPaths  【输出】待拼 TIFF 路径列表
 * @param cb         进度/取消回调
 * @param err        可选；失败说明
 * @param rawNrHint  RAW 行数提示，0=自动
 * @param rawNaHint  RAW 列数提示，0=自动
 * @return true 成功
 */
RCS_API bool prepareMosaicInputs(const std::string &inFolder, const std::string &outFolder,
                                 std::vector<std::string> &tiffPaths, Callbacks &cb,
                                 std::string *err = 0, int rawNrHint = 0, int rawNaHint = 0);

// ---------------------------------------------------------------------------
// [10] ToZeroData — 源 DAT → 四极化 L0 DAT（可选 .sarrec）
// ---------------------------------------------------------------------------

/**
 * 源 DAT 转为 HH/VV/HV/VH 四个 L0 DAT；可选写 .sarrec 还原信息。
 * 同步阻塞；建议实现 onProgress / onMessage / isCancelled。
 * 已有输出或路径冲突时拒绝覆盖（见第三方 ToZeroData 约定）。
 */
RCS_API bool processToZeroData(const ToZeroDataRequest &req, ToZeroDataResponse &resp, Callbacks &cb,
                               std::string *err = 0);

/** 由输出基路径派生四个极化 DAT 路径（顺序 HH,VV,HV,VH） */
RCS_API std::vector<std::string> toZeroDataOutputPaths(const std::string &outputBasePath);

/** 由输出基路径派生 .sarrec 路径 */
RCS_API std::string toZeroDataRecoveryPath(const std::string &outputBasePath);

#endif
