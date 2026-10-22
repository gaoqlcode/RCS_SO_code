#ifndef RCS_API_H
#define RCS_API_H

#include "rcs/callbacks.hpp"
#include "rcs/export.h"
#include "rcs/types.hpp"
#include <string>
#include <vector>

// =============================================================================
// SDK 公开 API（仅下列符号供二次开发）
//
//   [0] rcsVersion
//   [9] processRcsCalTif              辐射定标
//   [8] processMosaic / flattenMosaic / listMosaicTiffs /
//       loadMosaicGrayPreview / prepareMosaicInputs
//   [10] processZeroImage（源 DAT → 四极化 L0 → 成像）
//       processZeroImageFolder（目录内全部源 DAT → L0 → 成像）
//       processToZeroData / toZeroDataOutputPaths / toZeroDataRecoveryPath
//       zeroImageSidecarPath
//
// 通用约定：
//   - 路径 UTF-8
//   - process* 须传 Callbacks&；失败/取消返回 false，可读说明写入 *err（可为 nullptr）
// =============================================================================

// ---------------------------------------------------------------------------
// [0] 通用
// ---------------------------------------------------------------------------

/**
 * 返回动态库版本字符串。
 * @return 例如 "1.0.0"
 */
RCS_API const char *rcsVersion();

// ---------------------------------------------------------------------------
// [9] RCS 像 辐射定标
// ---------------------------------------------------------------------------

/**
 * 由含角反的定标 RAW 求定标系数 K，并对目录内匹配的 RAW 批量辐射定标。
 *
 * 功能：
 *   1) doCalib=true：读 calibRawPath →（autoPick=false 时 onRectRoi 框角反缓冲，
 *      或 autoPick=true 用 calBufRoi）→ 积分求 ε、K、R_near 等；
 *   2) 扫描 rawFolder 中带 Nr/Na、匹配 band/pol 的 .raw；
 *   3) 写出 int16 TIFF、三级 txt，以及同名二级 .pdat。
 *
 * 输入约定：RAW 无头 uint16 LE；文件名含 Nr{行}Na{列}，如 Image_Nr2048Na4602_X_HH.raw。
 * 输出：{stem}_RCS_dBsm.tif 、{tgtName}{时间戳}.txt。
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
// [8] 大图拼接
// ---------------------------------------------------------------------------

/**
 * 将输入目录中多张图自动/手动配准融合，并固定输出匀光图与三级 txt。
 *
 * 功能：
 *   - 优先使用目录内 TIFF；若仅有 RAW 则先转 TIFF；至少 2 张；
 *   - mode=0 自动配准；mode=1 使用 offR/offC；
 *
 * 推荐输入：定标产物 *_RCS_dBsm.tif。
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
 * 对已有拼接 TIFF 单独做匀光。
 *
 * @param inTif     输入 TIFF 路径
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
 * 列出目录中可用于拼接的 TIFF 路径。
 * 不扫描 RAW。
 *
 * @param folder  输入目录（UTF-8）
 * @return 绝对或拼接后的文件路径列表（已按名称中数字排序）
 */
RCS_API std::vector<std::string> listMosaicTiffs(const std::string &folder);

/**
 * 加载单张 TIFF 的 8bit 灰度预览（用于界面预览/手动对齐）。
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
 * 不执行配准融合。
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
// [10] 源 DAT → 四极化 L0 → 成像（同一功能的两步，processZeroImage 一次完成）
// ---------------------------------------------------------------------------

/**
 * 仅把源 DAT 转为 HH/VV/HV/VH 四个 L0 DAT，可选写 .sarrec。
 * 已有输出或路径冲突时拒绝覆盖。
 */
RCS_API bool processToZeroData(const ToZeroDataRequest &req, ToZeroDataResponse &resp, Callbacks &cb,
                               std::string *err = 0);

/** 由输出基路径派生四个极化 DAT 路径（顺序 HH,VV,HV,VH） */
RCS_API std::vector<std::string> toZeroDataOutputPaths(const std::string &outputBasePath);

/** 由输出基路径派生 .sarrec 路径 */
RCS_API std::string toZeroDataRecoveryPath(const std::string &outputBasePath);

/**
 * 先将源 DAT 转为四极化 L0（强制写 .sarrec），再对该目录成像，图像写回 L0 所在目录。
 *
 * sourcePath 与 outputBasePath 同时给出时走完整流程。
 * 只填 inputDirectory、不填源 DAT 时，只对已有 L0 目录成像。
 * 一次调用完成。客户只链 -lrcs_proc。
 * 后处理程序是 engine/bin/sarIsarPostProcess，自带运行库，不需要安装 MATLAB。
 * SAR 会先读 SAR_Raw_0001.dat，ISAR 会先探测 SAR_Raw_0011.dat，这两个编号必须存在。
 * 同步阻塞。建议实现 onProgress / onMessage / isCancelled。
 * kind=0 为 SAR，kind=1 为 ISAR。SAR 的文件个数至少为 2，且须与数据编号匹配。
 *
 * @param req   源 DAT、L0 基路径、成像参数
 * @param resp  L0 四极化路径，以及成像产物
 * @param cb    进度 / 日志 / 取消。转换约占 0～50，成像约占 50～100
 * @param err   可选；失败说明
 * @return true 两步都成功
 */
RCS_API bool processZeroImage(const ZeroImageRequest &req, ZeroImageResponse &resp, Callbacks &cb,
                              std::string *err = 0);

/**
 * 由四极化文件路径推导同目录 .sarrec 路径（去掉 _HH / _VV / _HV / _VH 后缀）。
 */
RCS_API std::string zeroImageSidecarPath(const std::string &polarizedPath);

/**
 * 将源目录这一层的原始 DAT 全部转为四极化 L0（强制写 .sarrec），再对输出目录成像一次。
 * 图像写回 outputDirectory。
 *
 * 只扫描 sourceDirectory 这一层，不进入子目录。扩展名须为 .dat（大小写不限）。
 * 文件名去掉扩展名后以 _HH、_VV、_HV、_VH 结尾的，视为已经是四极化 L0，不参与转换。
 * 每个源文件写成 outputDirectory/<原文件名去扩展名>_L0.dat，并派生 HH/VV/HV/VH 与 .sarrec。
 * 按文件名排序后依次转换。某个文件转换失败则停止，不再成像；已经写成的 L0 保留，并记在 resp.sources。
 * 成像仍按 parameters 里的起始编号、间隔、文件个数、最后编号读取还原出的 SAR_Raw_编号.dat，
 * 文件个数至少为 2。目录里多放的文件不会自动全部参与这一次成像。
 * 同步阻塞。建议实现 onProgress / onMessage / isCancelled。转换约占 0～50，成像约占 50～100。
 *
 * @param req   源目录、L0 输出目录、成像参数
 * @param resp  每个源文件的 L0 结果，以及一次成像的产物
 * @param cb    进度 / 日志 / 取消
 * @param err   可选；失败说明
 * @return true 全部转换成功且成像成功
 */
RCS_API bool processZeroImageFolder(const ZeroImageFolderRequest &req, ZeroImageFolderResponse &resp,
                                    Callbacks &cb, std::string *err = 0);

/** 对已有图像的 .cplx 按框导出切片报表。imagePath 也可传同名 .tif/.raw。 */
RCS_API bool exportSliceTxt(const SliceTxtRequest &req, SliceTxtResponse &resp, Callbacks &cb,
                            std::string *err = 0);

#endif
