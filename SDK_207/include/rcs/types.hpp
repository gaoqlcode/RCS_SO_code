#ifndef RCS_TYPES_HPP
#define RCS_TYPES_HPP

#include "rcs/export.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// SDK_207 — 仅辐射定标 [9] + 大图拼接 [8] 所需类型
// 坐标系：行 y/nr = 距离向（第 1 行=远端）；列 x/na = 方位向；ROI/偏移均为全分辨率像素。
// 详细约定与示例：SDK_207/DOC_API.md 、demo/
// =============================================================================

/**
 * 三级文本产品（.txt）文件头字段。
 * 均为字符串；空字符串表示使用库内默认文案或留空。
 * 写入分区：[TASK] [TARGET] [SYSTEM] [ENVIRONMENT] [CONDITIONS]。
 */
struct Level3Info {
    // ---- [TASK] ----
    std::string taskName;      //< 任务名称（定标默认「SAR二维像RCS定标」；拼接默认「SAR二维像RCS拼接」）
    std::string operatorName;  //< 任务负责人
    std::string unit;          //< 任务单位
    std::string code;          //< 任务代号
    std::string source;        //< 任务来源
    std::string dataSource;    //< 数据来源
    std::string dataType;      //< 数据类型（默认可含「3级 RCS 二维像(dBsm)」）
    std::string place;         //< 任务执行地点
    std::string time;          //< 任务执行时间
    std::string calMode;       //< 定标形式（默认「角反射器能量(积分)法」）
    std::string calName;       //< 定标体名称（默认「角反射器」）
    std::string calSize;       //< 定标体尺寸
    std::string uncertainty;   //< 不确定度

    // ---- [TARGET] ----
    std::string tgtName;       //< 目标名称（也影响定标三级 txt 文件名前缀）
    std::string tgtDesc;       //< 目标说明
    std::string tgtClass;      //< 目标分类
    std::string tgtL;          //< 目标长 (m)
    std::string tgtW;          //< 目标宽 (m)
    std::string tgtH;          //< 目标高 (m)

    // ---- [SYSTEM] ----
    std::string devName;       //< 设备名称
    std::string devClass;      //< 设备分类
    std::string sysMode;       //< 工作体制
    std::string pulseWidth;    //< 脉冲宽度 (μs)
    std::string prf;           //< 脉冲重复频率
    std::string dynRange;      //< 动态范围 (dB)

    // ---- [ENVIRONMENT] ----
    std::string temp;          //< 温度 (℃)
    std::string humidity;      //< 湿度 (%)
    std::string pressure;      //< 气压 (hPa)
    std::string visibility;    //< 能见度 (km)
    std::string cloud;         //< 云况

    // ---- [CONDITIONS] ----
    std::string measureTime;   //< 测量时刻
    std::string fStep;         //< 频率步长 (GHz)
    std::string fStart;        //< 起始频率 (GHz)
    std::string fStop;         //< 终止频率 (GHz)
    std::string pol;           //< 极化组合（定标空时会回填 req.pol）
    std::string tgtHeight;     //< 目标高度 (m)
    std::string azStart;       //< 起始入射方位视向角 (°)
    std::string azStop;        //< 终止入射方位视向角 (°)
    std::string elStart;       //< 起始入射俯仰视向角 (°)
    std::string elStop;        //< 终止入射俯仰视向角 (°)
    std::string fs;            //< 回波采样率（定标空时会按 fsHz 回填）
    std::string note;          //< 备注（定标空时会写入 K/R_near 等）
};

/**
 * 图像上一点（全分辨率下标）。
 * 仅完整库其它模块回调 ABI 需要；SDK_207 业务可不填。
 */
struct InteractPick {
    int az; //< 方位向列（全分辨率）
    int rg; //< 距离向行（全分辨率）
    InteractPick() : az(0), rg(0) {}
    InteractPick(int a, int r) : az(a), rg(r) {}
};

/**
 * 人机交互选点结果。
 * 仅完整库其它模块回调 ABI 需要；SDK_207 业务可不填。成员布局勿改。
 */
struct InteractSelection {
    std::vector<InteractPick> corners;                 //< 角点
    std::vector<InteractPick> targets;                 //< 目标点
    int targetMode;                                    //< 目标模式（默认 1）
    std::vector<std::pair<double, double> > extendedRanges; //< 扩展距离范围
    double cornerRangeOverrideM;                       //< 角点距离覆盖 (m)，<0 表示不用

    InteractSelection() : targetMode(1), cornerRangeOverrideM(-1) {}
};

/** 8bit 灰度预览图，行优先 pixels[y * width + x] */
struct GrayImage {
    int width;                     //< 预览宽（列数）
    int height;                    //< 预览高（行数）
    std::vector<uint8_t> pixels;   //< 像素，长度 = width * height
    GrayImage() : width(0), height(0) {}
};

/**
 * 矩形 ROI（全分辨率像素）。
 * x = 方位列，y = 距离行；valid() 要求 w>0 且 h>0。
 */
struct RectRoi {
    int x; //< 左上角方位列（全分辨率）
    int y; //< 左上角距离行（全分辨率）
    int w; //< 宽（方位向像素数）
    int h; //< 高（距离向像素数）
    RectRoi() : x(0), y(0), w(0), h(0) {}
    RectRoi(int xx, int yy, int ww, int hh) : x(xx), y(yy), w(ww), h(hh) {}
    bool valid() const { return w > 0 && h > 0; }
};

// ===== [9] RCS 像辐射定标 =====

namespace rcs {
/** 距离锚点方式：求 R_near */
enum RcsCalAnchor {
    RcsCalLastRow = 0, //< 用末行入射角（incAngleDeg − 半波束）与 hFlight
    RcsCalCenter = 1,  //< 用场景中心斜距（hFlight / cos(incAngleDeg)）
    RcsCalCrKnown = 2  //< 已知角反斜距 rCalKnown + 行号 rowCrHint（1-based）
};
/** 三级 txt [DATA] 布局 */
enum RcsCalL3Layout {
    RcsCalL3Point = 0, //< 逐点：每行「行 列 dBsm」（行列 1-based）
    RcsCalL3Row = 1    //< 逐行：一行内空格分隔各列 dBsm
};
} // namespace rcs

/**
 * processRcsCalTif 请求参数。
 *
 * 输入 RAW：无头 uint16 LE；文件名须含 Nr{行}Na{列}。
 * 库内固定写出 *_RCS_dBsm.tif（DN=dBsm×100）与三级 .txt（dBsm 真值）。
 */
struct RcsCalTifRequest {
    std::string calibRawPath; //< 【doCalib=true 必填】含角反的定标 RAW 路径（UTF-8）
    std::string rawFolder;    //< 【必填】待批量定标的 RAW 目录
    std::string outDir;       //< 输出目录；空则写到 rawFolder
    std::string tgtName;      //< 三级文件名前缀，默认「目标」
    std::string band;         //< 波段：筛文件名 + 波束宽（W→7°，其它→10°）；默认 "X"
    std::string pol;          //< 极化筛文件名；空=不按极化筛；默认 "HH"
    double sigmaTheoryDb;     //< 角反理论 RCS，单位 dBsm；默认 18.25
    double fsHz;              //< 采样率 Hz，用于 Rbin=c/(2·fs)；默认 1.25e9
    double incAngleDeg;       //< 入射角 °，求 R_near 用；默认 70
    double hFlight;           //< 飞行高度 m；默认 500
    int anchorMode;           //< 距离锚点：rcs::RcsCalLastRow/Center/CrKnown；默认 0
    double rCalKnown;         //< 【anchorMode=2 必填】已知角反斜距 m
    int rowCrHint;            //< 【anchorMode=2 必填】角反所在行（1-based）
    bool doCalib;             //< true=框选/ROI 求 K；false=使用预填 K/R_near/Rbin；默认 true
    int l3Layout;             //< 三级布局：rcs::RcsCalL3Point/Row；默认 0
    int maxFiles;             //< 最多处理文件数；≤0=全部；默认 0
    bool autoPick;            //< false=回调 onRectRoi；true=用 calBufRoi；默认 false
    RectRoi calBufRoi;        //< 【autoPick=true 必填】角反缓冲区（全分辨率）
    double K;                 //< 【doCalib=false 必填】定标系数，须 >0
    double R_near;            //< 【doCalib=false 必填】近距斜距 m，须 >0
    double Rbin;              //< 【doCalib=false 必填】距离采样间隔 m；≤0 则用 c/(2·fs)
    Level3Info header;        //< 可选覆盖三级头；空项用库默认

    RcsCalTifRequest()
        : band("X"), pol("HH"), sigmaTheoryDb(18.25), fsHz(1.25e9), incAngleDeg(70.0),
          hFlight(500.0), anchorMode(0), rCalKnown(0), rowCrHint(0), doCalib(true), l3Layout(0),
          maxFiles(0), autoPick(false), K(0), R_near(0), Rbin(0)
    {
        tgtName = "目标";
    }
};

/** 单个 RAW 定标结果 */
struct RcsCalTifFileResult {
    std::string rawName; //< 源 RAW 文件名
    std::string tifPath; //< 写出的 *_RCS_dBsm.tif；失败可空
    std::string txtPath; //< 写出的三级 .txt；失败可空
    int nr;              //< 距离向行数
    int na;              //< 方位向列数
    double dbsmMin;      //< 本景 dBsm 最小值
    double dbsmMax;      //< 本景 dBsm 最大值
    std::string note;    //< 跳过/失败原因等说明
    RcsCalTifFileResult() : nr(0), na(0), dbsmMin(0), dbsmMax(0) {}
};

/** processRcsCalTif 输出 */
struct RcsCalTifResponse {
    double K;            //< 定标系数
    double R_near;       //< 近距斜距 m
    double Rbin;         //< 距离采样间隔 m
    double R_cal;        //< 角反峰值处斜距 m（doCalib=true 时有效）
    double epsilon_cal;  //< 角反净能量 ε
    double sigma_cal;    //< 理论 RCS 线性值 10^(dBsm/10)，单位 m²
    int peakRow;         //< 角反峰行（0-based）
    int peakCol;         //< 角反峰列（0-based）
    GrayImage preview;   //< 定标景 8bit 预览（降采样）
    std::vector<RcsCalTifFileResult> files; //< 各 RAW 处理结果
    RcsCalTifResponse()
        : K(0), R_near(0), Rbin(0), R_cal(0), epsilon_cal(0), sigma_cal(0), peakRow(0), peakCol(0)
    {
    }
};

// ===== [8] 大图拼接 =====

/**
 * processMosaic 请求参数。
 *
 * 输入：目录内 TIFF/JPEG（或仅 RAW 自动转 TIFF）；至少 2 张；忽略 MOSAIC* 产物。
 * 推荐输入定标产物 *_RCS_dBsm.tif。
 * 库内固定：始终匀光 + MOSAIC_L3.txt（dBsm=DN/100）；不写 MOSAIC_RCS.tif。
 */
struct MosaicRequest {
    std::string inFolder;  //< 【必填】输入 TIFF/RAW 目录（UTF-8）
    std::string outFolder; //< 输出目录；空则在输入旁自动建结果目录
    double medOut;         //< 匀光目标中值（归一化约 0~1）；默认 0.35；≤0 按 0.35
    int mode;              //< 0=自动配准；1=使用 offR/offC 手动偏移；默认 0
    std::vector<int> offR; //< 【mode=1】各图方位向绝对偏移（全分辨率像素），长度=图数
    std::vector<int> offC; //< 【mode=1】各图距离向绝对偏移（全分辨率像素），长度=图数
    int rawNrHint;         //< 仅 RAW→TIFF 时行数提示；0=自动；默认 0
    int rawNaHint;         //< 仅 RAW→TIFF 时列数提示；0=自动；默认 0
    int l3Layout;          //< 三级布局：0=逐点，1=逐行；默认 0
    std::string band;      //< 写入三级头的波段；默认 "X"
    std::string pol;       //< 写入三级头的极化；默认 "HH"
    Level3Info header;     //< 可选覆盖三级头；空项用库默认

    MosaicRequest()
        : medOut(0.35), mode(0), rawNrHint(0), rawNaHint(0), l3Layout(0), band("X"), pol("HH")
    {
    }
};

/** processMosaic 输出路径与接缝信息 */
struct MosaicResponse {
    std::string mosaicTif;      //< MOSAIC.tif（显示链融合，uint16）
    std::string flattenTif;     //< MOSAIC_匀光.tif（本交付必出）
    std::string displayTif;     //< MOSAIC_显示.tif
    std::string rcsTif;         //< MOSAIC_RCS.tif；本交付不写，恒为空
    std::string l3Path;         //< MOSAIC_L3.txt（本交付必出）
    std::vector<int> seamCols;  //< 接缝列坐标，可供 flattenMosaic 复用
};

// ===== [10] ToZeroData：源 DAT → 四极化 L0 =====

/** processToZeroData 请求 */
struct ToZeroDataRequest {
    std::string sourcePath;     //< 【必填】源 DAT（UTF-8）
    std::string outputBasePath; //< 【必填】输出基路径，如 .../source_L0.dat
    bool saveRecovery;          //< true=写 .sarrec；默认 false

    ToZeroDataRequest() : saveRecovery(false) {}
};

/** 单个极化输出 */
struct ToZeroDataPolarizationOut {
    std::string polarization; //< HH / VV / HV / VH
    std::string path;
    std::uint64_t frames;
    std::uint64_t bytes;
    ToZeroDataPolarizationOut() : frames(0), bytes(0) {}
};

/** processToZeroData 输出 */
struct ToZeroDataResponse {
    std::uint64_t validRecords;
    std::uint64_t skippedRecords;
    std::uint64_t outputBytes; //< 四个 DAT 合计
    std::vector<ToZeroDataPolarizationOut> outputs;
    std::string recoveryPath; //< saveRecovery 且成功时非空
    std::uint64_t recoveryBytes;
    std::string sourceSha256;
    ToZeroDataResponse()
        : validRecords(0), skippedRecords(0), outputBytes(0), recoveryBytes(0)
    {
    }
};

#endif
