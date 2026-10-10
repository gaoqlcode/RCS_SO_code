#ifndef RCS_API_H
#define RCS_API_H

#include "rcs/callbacks.hpp"
#include "rcs/export.h"
#include "rcs/types.hpp"
#include <string>
#include <vector>

// =============================================================================
// rcs_api.h — librcs_proc 对外 C++11 API
//
// 约定：
//   - 面向人机交互：process* / previewL0Folder 必须带 Callbacks&
//   - 请求默认 autoPick=false，需实现选点/框选回调
//   - 路径一律 UTF-8；返回 false 表示失败/取消，可读说明写入 *err
//   - 也可 #include "rcs/rcs.hpp"
// =============================================================================

/** 返回库版本字符串，如 "1.0.0"（静态存储，勿 free） */
RCS_API const char *rcsVersion();

// ---------------------------------------------------------------------------
// 业务处理（均需 Callbacks）
// ---------------------------------------------------------------------------

/**
 * HRRP 一维距离像：合并文件夹内 *_L0_<pol>.dat，脉压/定标，写出 .hrrp/.pdat，
 * 并在 resp 中带回 rangeM / rcsDbsm / phaseDeg 供画图。
 * 默认交互：实现 cb.onCornerConfirm。
 */
RCS_API bool processHrrp(const HrrpRequest &req, HrrpResponse &resp, Callbacks &cb,
                         std::string *err = 0);

/**
 * RCS 测量：二维成像链路，写出 .rcs/.pdat，resp.targets 为各目标 RCS。
 * 默认交互：实现 onCornerPick / onTargetPick；坐标须为全分辨率下标。
 */
RCS_API bool processRcs(const RcsRequest &req, RcsResponse &resp, Callbacks &cb,
                        std::string *err = 0);

/**
 * 点频 RCS：目录内多频点 L0，输出 RCS–频率曲线与三级 .rcs。
 * 默认交互：实现 onCornerConfirm 与 onTargetPick。
 */
RCS_API bool processCwRcs(const CwRcsRequest &req, CwRcsResponse &resp, Callbacks &cb,
                          std::string *err = 0);

/**
 * 后向散射系数 σ⁰：输入已成像 uint16 LE .raw（非 L0），写出三级 .cs。
 * 默认交互：onRectRoi / onTargetRois；ROI 为全分辨率。
 * sigmaTheoryDb 库内按 10^(dB/10) 转线性再参与 K 计算。
 */
RCS_API bool processSigma0(const Sigma0Request &req, Sigma0Response &resp, Callbacks &cb,
                           std::string *err = 0);

/**
 * 0 级文件夹时域/频域预览：扫描 *_L0_<pol>.dat，按文件名序号排序，不落盘。
 */
RCS_API bool previewL0Folder(const L0PreviewRequest &req, L0PreviewResponse &resp, Callbacks &cb,
                             std::string *err = 0);

/**
 * 单文件时域/频域预览（不落盘）。
 * @param path     一个 *_L0_*.dat 全路径
 * @param pulseNo  选用第几个脉冲，从 1 起
 * @param removeDC 是否去直流（建议 true）
 * @param fsHz     采样率 Hz，<=0 时用库内默认 1.25e9
 * @param out      输出曲线与统计量
 */
RCS_API bool previewL0File(const std::string &path, int pulseNo, bool removeDC, double fsHz,
                           L0PulsePreviewItem &out, std::string *err = 0);

// ---------------------------------------------------------------------------
// 数据探查（做 UI：列文件、填下拉框、显示头参数）
// ---------------------------------------------------------------------------

/**
 * 列出 folder 下匹配 *_L0_<pol>.dat 的文件名（仅文件名），按序号升序。
 * @param pol 如 "HH"/"HV"/"VV"/"VH"
 */
RCS_API bool listL0Files(const std::string &folder, const std::string &pol,
                         std::vector<std::string> &outNames, std::string *err = 0);

/**
 * 扫描目录，返回实际存在的极化列表（顺序固定：HH,HV,VV,VH 中出现的项）。
 */
RCS_API bool listAvailablePolarizations(const std::string &folder, std::vector<std::string> &outPols,
                                        std::string *err = 0);

/**
 * 读取单个 L0 文件的文件头与首帧元数据，不跑完整处理链。
 * @param hdr    512B 文件头解析结果
 * @param frame0 首帧：N/PRT/脉宽/延时等
 */
RCS_API bool probeL0File(const std::string &path, FileHeaderInfo &hdr, FrameMeta &frame0,
                         std::string *err = 0);

/**
 * 探查某极化下整夹数据：文件个数、首末文件名、首文件头摘要。
 */
RCS_API bool probeDataFolder(const std::string &folder, const std::string &pol, L0FolderProbe &out,
                             std::string *err = 0);

/**
 * 从成像 RAW 文件名解析 Nr/Na，如 Image_Nr2048Na4602File....raw。
 * @param pathOrName 全路径或仅文件名均可
 */
RCS_API bool parseImagedRawSize(const std::string &pathOrName, int &nr, int &na,
                                std::string *err = 0);

RCS_API bool listImagedRawFiles(const std::string &folder, std::vector<std::string> &outNames,
                                std::string *err = 0);

/**
 * 按库约定生成默认输出目录：dataFolder/处理结果_<tag>
 * @param tag 例 "HRRP_HH"、"RCS_HH"、"后向散射"、"点频RCS_HH"
 */
RCS_API bool defaultOutDir(const std::string &dataFolder, const std::string &tag, std::string &outDir,
                           std::string *err = 0);

// ---------------------------------------------------------------------------
// 预览辅助（后向散射框选 UI 等）
// ---------------------------------------------------------------------------

/**
 * 读取成像 uint16 LE RAW，生成降采样 8bit 灰度预览。
 * 磁盘为行主序 [Nr][Na]（与 processSigma0 / MATLAB fread([Na,Nr])' 一致）。
 * @param step 降采样步长，常用 8；越小图越大
 */
RCS_API bool loadImagedRawPreview(const std::string &rawPath, int nr, int na, int step,
                                  GrayImage &preview, std::string *err = 0);

/**
 * 预览图像素矩形 → 全分辨率 RectRoi（x=方位列, y=距离行）。
 * @param fullNr 距离向行数  @param fullNa 方位向列数
 */
RCS_API bool mapPreviewRoiToFull(int previewW, int previewH, int fullNr, int fullNa,
                                 const RectRoi &previewRoi, RectRoi &fullRoi, std::string *err = 0);

/**
 * 全分辨率点 (fullAz, fullRg) → 预览像素 (prevX, prevY)，用于画标记。
 */
RCS_API bool mapFullPointToPreview(int previewW, int previewH, int fullNr, int fullNa, int fullAz,
                                   int fullRg, int &prevX, int &prevY, std::string *err = 0);

// ---------------------------------------------------------------------------
// 拼接（完整工程可选编译；客户交付 SDK 头文件不含下列声明）
// ---------------------------------------------------------------------------

/** 大场景 TIFF/RAW 自动拼接 */
RCS_API bool processMosaic(const MosaicRequest &req, MosaicResponse &resp, Callbacks &cb,
                           std::string *err = 0);

/** 拼接结果匀光 */
RCS_API bool flattenMosaic(const std::string &inTif, const std::string &outTif, double medOut,
                           const std::vector<int> &seamCols, Callbacks &cb, std::string *err = 0);

/** 列出文件夹内可用于拼接的 TIFF */
RCS_API std::vector<std::string> listMosaicTiffs(const std::string &folder);

/** 加载 TIFF 灰度预览 */
RCS_API bool loadMosaicGrayPreview(const std::string &path, int step, GrayImage &preview, int &fullW,
                                   int &fullH, std::string *err = 0);

/** 整理拼接输入：已有 TIFF 直接用，仅有 RAW 则先转换 */
RCS_API bool prepareMosaicInputs(const std::string &inFolder, const std::string &outFolder,
                                 std::vector<std::string> &tiffPaths, Callbacks &cb,
                                 std::string *err = 0, int rawNrHint = 0, int rawNaHint = 0);

#endif
