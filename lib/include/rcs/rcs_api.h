#ifndef RCS_API_H
#define RCS_API_H

#include "rcs/callbacks.hpp"
#include "rcs/export.h"
#include "rcs/types.hpp"
#include <string>
#include <vector>

// =============================================================================
// rcs_api.h — librcs_proc 对外 C++11 API（完整版，与 sdk/include 同步）
//
// 约定：
//   - process* / preview* / 拼接主流程须带 Callbacks&
//   - 请求默认 autoPick=false，需实现对应选点/框选回调
//   - 路径一律 UTF-8；返回 false 表示失败/取消，可读说明写入 *err
//   - 也可 #include "rcs/rcs.hpp"
//
// 功能分组（交付时可按组保留/裁剪头文件声明；库内符号以编译开关为准）：
//   [0] 通用
//   [1] HRRP 一维距离像
//   [2] RCS 测量
//   [3] 点频 RCS
//   [4] 后向散射 σ⁰
//   [5] 0 级预览
//   [6] 数据探查 / 路径约定
//   [7] 成像 RAW 预览辅助（配合 [4]）
//   [8] 大图拼接
//   [10] 源 DAT → 四极化 L0 → 成像（可选，Linux；-DRCS_ENABLE_TOZERODATA）
//       processZeroImage / processZeroImageFolder / processToZeroData
// =============================================================================

// ---------------------------------------------------------------------------
// [0] 通用
// ---------------------------------------------------------------------------

/** 返回库版本字符串，如 "1.0.0"（静态存储，勿 free） */
RCS_API const char *rcsVersion();

// ---------------------------------------------------------------------------
// [1] HRRP 一维距离像
// 输入：含 *_L0_<pol>.dat 的目录
// 输出：.hrrp / .pdat / 1 级 .dat；曲线在 resp.rangeM / rcsDbsm / phaseDeg
// 回调：onCornerConfirm（autoPick=false）
// ---------------------------------------------------------------------------

RCS_API bool processHrrp(const HrrpRequest &req, HrrpResponse &resp, Callbacks &cb,
                         std::string *err = 0);

// ---------------------------------------------------------------------------
// [2] RCS 测量
// 输入：含 *_L0_<pol>.dat 的目录
// 输出：.rcs / .pdat / 1 级 .dat；resp.targets
// 回调：onCornerPick、onTargetPick（坐标为全分辨率下标）
// ---------------------------------------------------------------------------

RCS_API bool processRcs(const RcsRequest &req, RcsResponse &resp, Callbacks &cb,
                        std::string *err = 0);

// ---------------------------------------------------------------------------
// [3] 点频 RCS
// 输入：目录内多频点 L0
// 输出：三级 .rcs；曲线 freqGHz / rcsDbsm
// 回调：onCornerConfirm、onTargetPick
// ---------------------------------------------------------------------------

RCS_API bool processCwRcs(const CwRcsRequest &req, CwRcsResponse &resp, Callbacks &cb,
                          std::string *err = 0);

// ---------------------------------------------------------------------------
// [4] 后向散射系数 σ⁰
// 输入：已成像 uint16 LE .raw（非 L0）
// 输出：三级 .cs
// 回调：onRectRoi、onTargetRois；ROI 为全分辨率
// 辅助：见 [7]
// ---------------------------------------------------------------------------

RCS_API bool processSigma0(const Sigma0Request &req, Sigma0Response &resp, Callbacks &cb,
                           std::string *err = 0);

// ---------------------------------------------------------------------------
// [9] RCS 像辐射定标（HXSSXS_207_cal_tif）
// 角反缓冲区 → K；批量固定写出 *_RCS_dBsm.tif 与三级 .txt
// 回调：doCalib 且 autoPick=false 时 onRectRoi(stage="cal")
// ---------------------------------------------------------------------------

RCS_API bool processRcsCalTif(const RcsCalTifRequest &req, RcsCalTifResponse &resp, Callbacks &cb,
                              std::string *err = 0);

// ---------------------------------------------------------------------------
// [5] 0 级时域 / 频域预览（不落盘）
// ---------------------------------------------------------------------------

/** 扫描文件夹内 *_L0_<pol>.dat，按文件名序号排序 */
RCS_API bool previewL0Folder(const L0PreviewRequest &req, L0PreviewResponse &resp, Callbacks &cb,
                             std::string *err = 0);

/**
 * 单文件预览。
 * @param path     一个 *_L0_*.dat 全路径
 * @param pulseNo  选用第几个脉冲，从 1 起
 * @param removeDC 是否去直流（建议 true）
 * @param fsHz     采样率 Hz，<=0 时用库内默认 1.25e9
 */
RCS_API bool previewL0File(const std::string &path, int pulseNo, bool removeDC, double fsHz,
                           L0PulsePreviewItem &out, std::string *err = 0);

// ---------------------------------------------------------------------------
// [6] 数据探查 / 路径约定（做 UI：列文件、填下拉框、显示头参数）
// 常与 [1][2][3][5] 一起交付；[4] 另用 listImagedRawFiles / parseImagedRawSize
// ---------------------------------------------------------------------------

/** 列出 folder 下匹配 *_L0_<pol>.dat 的文件名（仅文件名），按序号升序 */
RCS_API bool listL0Files(const std::string &folder, const std::string &pol,
                         std::vector<std::string> &outNames, std::string *err = 0);

/** 扫描目录，返回实际存在的极化（顺序：HH,HV,VV,VH 中出现的项） */
RCS_API bool listAvailablePolarizations(const std::string &folder, std::vector<std::string> &outPols,
                                        std::string *err = 0);

/** 读单个 L0 的 512B 文件头与首帧元数据，不跑完整处理链 */
RCS_API bool probeL0File(const std::string &path, FileHeaderInfo &hdr, FrameMeta &frame0,
                         std::string *err = 0);

/** 探查某极化整夹：文件个数、首末文件名、首文件头摘要 */
RCS_API bool probeDataFolder(const std::string &folder, const std::string &pol, L0FolderProbe &out,
                             std::string *err = 0);

/** 从成像 RAW 文件名解析 Nr/Na，如 Image_Nr2048Na4602File....raw */
RCS_API bool parseImagedRawSize(const std::string &pathOrName, int &nr, int &na,
                                std::string *err = 0);

/** 列出 folder 下可解析 Nr/Na 的成像 .raw 文件名（仅文件名），按名排序 */
RCS_API bool listImagedRawFiles(const std::string &folder, std::vector<std::string> &outNames,
                                std::string *err = 0);

/**
 * 默认输出目录：dataFolder/处理结果_<tag>
 * @param tag 例 "HRRP_HH"、"RCS_HH"、"后向散射"、"点频RCS_HH"、"拼接"
 */
RCS_API bool defaultOutDir(const std::string &dataFolder, const std::string &tag, std::string &outDir,
                           std::string *err = 0);

// ---------------------------------------------------------------------------
// [7] 成像 RAW 预览辅助（配合 [4] 后向散射框选 UI）
// ---------------------------------------------------------------------------

/**
 * 读成像 uint16 LE RAW，生成降采样 8bit 灰度预览。
 * 磁盘为行主序 [Nr][Na]（与 processSigma0 / MATLAB fread([Na,Nr])' 一致）。
 * @param step 降采样步长，常用 8
 */
RCS_API bool loadImagedRawPreview(const std::string &rawPath, int nr, int na, int step,
                                  GrayImage &preview, std::string *err = 0);

/** 预览图像素矩形 → 全分辨率 RectRoi（x=方位列, y=距离行） */
RCS_API bool mapPreviewRoiToFull(int previewW, int previewH, int fullNr, int fullNa,
                                 const RectRoi &previewRoi, RectRoi &fullRoi, std::string *err = 0);

/** 全分辨率点 (fullAz, fullRg) → 预览像素 (prevX, prevY) */
RCS_API bool mapFullPointToPreview(int previewW, int previewH, int fullNr, int fullNa, int fullAz,
                                   int fullRg, int &prevX, int &prevY, std::string *err = 0);

// ---------------------------------------------------------------------------
// [8] 大图拼接（完整库始终包含）
// 输入：TIFF 或可转 TIFF 的 RAW 目录；固定输出匀光 TIFF + MOSAIC_L3.txt
// ---------------------------------------------------------------------------

/**
 * 自动配准或按给定偏移融合（见 MosaicRequest::mode）
 * 支持 uint16 / int16 RCS_dBsm TIFF；固定输出：匀光 TIFF + MOSAIC_L3.txt（不写 MOSAIC_RCS.tif）
 */
RCS_API bool processMosaic(const MosaicRequest &req, MosaicResponse &resp, Callbacks &cb,
                           std::string *err = 0);

/** 对已有拼接 TIFF 做匀光 */
RCS_API bool flattenMosaic(const std::string &inTif, const std::string &outTif, double medOut,
                           const std::vector<int> &seamCols, Callbacks &cb, std::string *err = 0);

/** 列出文件夹内可用于拼接的 TIFF（排除 MOSAIC*） */
RCS_API std::vector<std::string> listMosaicTiffs(const std::string &folder);

/** 加载 TIFF 灰度预览；fullW/fullH 为全分辨率宽高 */
RCS_API bool loadMosaicGrayPreview(const std::string &path, int step, GrayImage &preview, int &fullW,
                                   int &fullH, std::string *err = 0);

/**
 * 整理拼接输入：已有 TIFF 直接用；仅有 RAW 则先转 TIFF。
 * @param rawNrHint / rawNaHint 手动尺寸；0=自动（文件名 / 增益包络）
 */
RCS_API bool prepareMosaicInputs(const std::string &inFolder, const std::string &outFolder,
                                 std::vector<std::string> &tiffPaths, Callbacks &cb,
                                 std::string *err = 0, int rawNrHint = 0, int rawNaHint = 0);

#if defined(RCS_ENABLE_TOZERODATA) && RCS_ENABLE_TOZERODATA
// ---------------------------------------------------------------------------
// [10] 源 DAT → 四极化 L0 → 成像（两步都静态合入主库，一次调用完成）
// 回调：onProgress / onMessage / isCancelled（建议）
// 运行前提：MATLAB Runtime v9.3，以及随包 engine/for_testing
// ---------------------------------------------------------------------------

/** 仅把源 DAT 转为 HH/VV/HV/VH。saveRecovery=true 时写 .sarrec。 */
RCS_API bool processToZeroData(const ToZeroDataRequest &req, ToZeroDataResponse &resp, Callbacks &cb,
                               std::string *err = 0);

/** 由输出基路径派生四个极化 DAT 路径（HH,VV,HV,VH） */
RCS_API std::vector<std::string> toZeroDataOutputPaths(const std::string &outputBasePath);

/** 由输出基路径派生 .sarrec 路径 */
RCS_API std::string toZeroDataRecoveryPath(const std::string &outputBasePath);

/**
 * 先将源 DAT 转为四极化 L0（强制写 .sarrec），再对该目录成像。
 * sourcePath 与 outputBasePath 同时给出时走完整流程；只填 inputDirectory 时只成像。
 * 同步阻塞。客户只链 -lrcs_proc。
 */
RCS_API bool processZeroImage(const ZeroImageRequest &req, ZeroImageResponse &resp, Callbacks &cb,
                              std::string *err = 0);

/** 由四极化文件路径推导同目录 .sarrec 路径（去掉 _HH 等后缀） */
RCS_API std::string zeroImageSidecarPath(const std::string &polarizedPath);

/**
 * 将源目录这一层的 DAT 全部转为四极化 L0（强制写 .sarrec），再对输出目录成像一次。
 * 不进入子目录。文件名（不含扩展名）以 _HH/_VV/_HV/_VH 结尾的视为已是 L0，跳过。
 * 每个源文件的输出基路径为 outputDirectory/<原名>_L0.dat。
 * 任一转换失败则停止，不再成像；已完成的 L0 留在 resp.sources。
 * 同步阻塞。客户只链 -lrcs_proc。
 */
RCS_API bool processZeroImageFolder(const ZeroImageFolderRequest &req, ZeroImageFolderResponse &resp,
                                    Callbacks &cb, std::string *err = 0);
#endif // RCS_ENABLE_TOZERODATA

#endif
