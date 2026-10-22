#ifndef RCS_TYPES_HPP
#define RCS_TYPES_HPP

#include "rcs/export.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// SDK — 仅辐射定标 [9] + 大图拼接 [8] 所需类型
// 坐标系：行 y/nr = 距离向（第 1 行=远端）；列 x/na = 方位向；ROI/偏移均为全分辨率像素。
// =============================================================================

/**
 * 三级文本产品（.txt）文件头字段。
 * 均为字符串；空字符串表示使用库内默认文案。
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
 * 本SDK 不用填。
 */
struct InteractPick {
    int az; //< 方位向列（全分辨率）
    int rg; //< 距离向行（全分辨率）
    InteractPick() : az(0), rg(0) {}
    InteractPick(int a, int r) : az(a), rg(r) {}
};

/**
 * 人机交互选点结果。
 * 本SDK 不用填。成员布局勿改。
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
} // namespace rcs

/**
 * processRcsCalTif 请求参数。
 *
 * 输入 RAW：无头 uint16 LE；文件名须含 Nr{行}Na{列}。
 * 写出 *_RCS_dBsm.tif、三级 .txt，以及同名二级 .pdat。
 */
struct RcsCalTifRequest {
    std::string calibRawPath; //< 【doCalib=true 必填】含角反的定标 RAW 路径（UTF-8）
    std::string rawFolder;    //< 【必填】待批量定标的 RAW 目录
    std::string outDir;       //< 输出目录；空则写到 rawFolder
    std::string l0HeaderPath; //< 二级 .pdat 用的 0 级 DAT；空则在 rawFolder 按极化自动找
    std::string mergeTxtPath; //< 距离 txt。空则按目标框从同名 .cplx 生成，并写入三级前两列
    RectRoi targetRoi;        //< 目标框（左上角，0 起点）。有效则只写框内三级/二级
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
    bool doCalib; //< true=框选/ROI 求 K；false=使用预填 K/R_near/Rbin；默认 true
    int maxFiles; //< 最多处理文件数；≤0=全部；默认 0
    bool autoPick;            //< false=回调 onRectRoi；true=用 calBufRoi；默认 false
    RectRoi calBufRoi;        //< 【autoPick=true 必填】角反缓冲区（全分辨率）
    double K;                 //< 【doCalib=false 必填】定标系数，须 >0
    double R_near;            //< 【doCalib=false 必填】近距斜距 m，须 >0
    double Rbin;              //< 【doCalib=false 必填】距离采样间隔 m；≤0 则用 c/(2·fs)
    Level3Info header;        //< 可选覆盖三级头；空项用默认

    RcsCalTifRequest()
        : band("X"), pol("HH"), sigmaTheoryDb(18.25), fsHz(1.25e9), incAngleDeg(70.0),
          hFlight(500.0), anchorMode(0), rCalKnown(0), rowCrHint(0), doCalib(true), maxFiles(0),
          autoPick(false), K(0), R_near(0), Rbin(0)
    {
        tgtName = "目标";
    }
};

/** 单个 RAW 定标结果 */
struct RcsCalTifFileResult {
    std::string rawName; //< 源 RAW 文件名
    std::string tifPath; //< 写出的 *_RCS_dBsm.tif；失败可空
    std::string txtPath; //< 写出的三级 .txt；失败可空
    std::string pdatPath;     //< 写出的二级 .pdat；失败可空
    std::string positionPath; //< 给成像方的目标位置；未框选可空
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
 * 输入：目录内 TIFF；至少 2 张；
 * 推荐输入定标产物 *_RCS_dBsm.tif。
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
    Level3Info header;     //< 可选覆盖三级头；空项用默认

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

// ===== [10] 源 DAT → 四极化 L0 → 成像 =====

/** 源 DAT 转四极化 L0。processZeroImage 会强制 saveRecovery=true。 */
struct ToZeroDataRequest {
    std::string sourcePath;     //< 源 DAT（UTF-8）
    std::string outputBasePath; //< 输出基路径，如 .../source_L0.dat
    bool saveRecovery;          //< true=写 .sarrec；默认 false

    ToZeroDataRequest() : saveRecovery(false) {}
};

struct ToZeroDataPolarizationOut {
    std::string polarization; //< HH / VV / HV / VH
    std::string path;
    std::uint64_t frames;
    std::uint64_t bytes;
    ToZeroDataPolarizationOut() : frames(0), bytes(0) {}
};

struct ToZeroDataResponse {
    std::uint64_t validRecords;
    std::uint64_t skippedRecords;
    std::uint64_t outputBytes; //< 四个 DAT 合计
    std::vector<ToZeroDataPolarizationOut> outputs;
    std::string recoveryPath;
    std::uint64_t recoveryBytes;
    std::string sourceSha256;
    ToZeroDataResponse()
        : validRecords(0), skippedRecords(0), outputBytes(0), recoveryBytes(0)
    {
    }
};

/** 传给 sarIsarPostProcess 的批次参数。文件个数至少为 2。 */
struct ZeroImageParameters {
    int beginId;     //< 起始文件编号；SAR 模板默认 3
    int deltaNum;    //< 间隔文件数目；默认 120
    int lastFileId;  //< 最后一个文件编号；默认 6000
    int rangeOffset; //< 最近成像作用距离偏移值；默认 100
    int fileNum;     //< 文件个数（SAR 至少 2）；SAR 模板默认 50
    int rangePoints; //< 距离向点数；默认 8192

    ZeroImageParameters()
        : beginId(3)
        , deltaNum(120)
        , lastFileId(6000)
        , rangeOffset(100)
        , fileNum(50)
        , rangePoints(8192)
    {
    }
};

/** processZeroImage 请求：先转四极化 L0，再成像 */
struct ZeroImageRequest {
    std::string sourcePath;         //< 源 DAT。与 outputBasePath 同时给出时先转 L0
    std::string outputBasePath;     //< L0 输出基路径，如 .../source_L0.dat
    std::string inputDirectory;     //< 已有 L0 目录。空则用本次 L0 产物目录；图像写回该目录
    ZeroImageParameters parameters; //< 须与数据编号匹配
    std::string engineDirectory;  //< 后处理包根目录（含 bin/sarIsarPostProcess）；空=自动查找
    std::string engineExecutable; //< 可执行文件；空=使用包内 sarIsarPostProcess
    int timeoutMs;                //< 毫秒；0=不限制
    bool requireEngine;             //< false=只还原、不启动引擎
    bool keepWorkspace;             //< 调试：保留内部工作目录
    int kind;                       //< 0=SAR，1=ISAR
    int roiX;                       //< 目标中心列，0 起点；roiW<=0 时不传
    int roiY;                       //< 目标中心行，0 起点
    int roiW;                       //< 横向宽度点数；<=0 表示不指定框
    int roiH;                       //< 径向高度点数；<=0 时高默认等于宽
    bool exportTxt;                 //< true 时成像同时导出切片报表
    std::string imageOutputDirectory; //< 图像输出目录；空则写回 L0 目录

    ZeroImageRequest()
        : timeoutMs(0)
        , requireEngine(true)
        , keepWorkspace(false)
        , kind(0)
        , roiX(0)
        , roiY(0)
        , roiW(0)
        , roiH(0)
        , exportTxt(false)
    {
    }
};

/** 回写到输入目录的产物 */
struct ZeroImagePublishedFile {
    std::string path;
    std::string suffix;
    std::uint64_t bytes;
    ZeroImagePublishedFile() : bytes(0) {}
};

/** 还原出的一组原始数据 */
struct ZeroImageRestoredInput {
    std::string sidecar;
    std::string originalName;
    std::string sourceSha256;
    std::uint64_t originalSize;
    ZeroImageRestoredInput() : originalSize(0) {}
};

/** processZeroImage 输出：先是 L0，再是图像 */
struct ZeroImageResponse {
    std::uint64_t validRecords;
    std::uint64_t skippedRecords;
    std::uint64_t l0Bytes;
    std::vector<ToZeroDataPolarizationOut> l0Outputs;
    std::string recoveryPath;
    std::uint64_t recoveryBytes;
    std::string sourceSha256;
    int exitCode; //< 后处理程序退出码；未启动为 -1
    std::uint64_t restoredBytes;
    std::vector<ZeroImageRestoredInput> inputs;
    std::vector<ZeroImagePublishedFile> publishedFiles;
    std::vector<std::string> warnings;
    std::string jobId;
    std::string recordPath;
    std::string configPath;
    std::string logPath;
    ZeroImageResponse()
        : validRecords(0)
        , skippedRecords(0)
        , l0Bytes(0)
        , recoveryBytes(0)
        , exitCode(-1)
        , restoredBytes(0)
    {
    }
};

/** 文件夹批量转换中的一个源 DAT 及其 L0 结果 */
struct ZeroImageFolderSource {
    std::string sourcePath;     //< 源 DAT
    std::string outputBasePath; //< 本次 L0 基路径，如 .../name_L0.dat
    ToZeroDataResponse result;
};

/**
 * processZeroImageFolder 请求。
 * 将该目录这一层的源 DAT 全部转为四极化 L0（强制 .sarrec），再对输出目录成像一次。
 */
struct ZeroImageFolderRequest {
    std::string sourceDirectory;    //< 原始 DAT 所在目录（不进入子目录）
    std::string outputDirectory;    //< 全部 L0 与 .sarrec 写入此目录，图像也写回此处
    ZeroImageParameters parameters; //< 成像参数；须与还原出的 SAR_Raw 编号匹配
    std::string engineDirectory;  //< 后处理包根目录（含 bin/sarIsarPostProcess）；空=自动查找
    std::string engineExecutable; //< 可执行文件；空=使用包内 sarIsarPostProcess
    int timeoutMs;                  //< 毫秒；0=不限制
    bool requireEngine;             //< false=只还原、不启动引擎
    bool keepWorkspace;             //< 调试：保留内部工作目录
    int kind;                       //< 0=SAR，1=ISAR
    int roiX;
    int roiY;
    int roiW;
    int roiH;
    bool exportTxt;
    std::string imageOutputDirectory; //< 图像输出目录；空则写回 outputDirectory

    ZeroImageFolderRequest()
        : timeoutMs(0)
        , requireEngine(true)
        , keepWorkspace(false)
        , kind(0)
        , roiX(0)
        , roiY(0)
        , roiW(0)
        , roiH(0)
        , exportTxt(false)
    {
    }
};

/** processZeroImageFolder 输出：每个源文件的 L0，以及一次成像的产物 */
struct ZeroImageFolderResponse {
    std::vector<ZeroImageFolderSource> sources;
    int exitCode; //< 后处理程序退出码；未启动为 -1
    std::uint64_t restoredBytes;
    std::vector<ZeroImageRestoredInput> inputs;
    std::vector<ZeroImagePublishedFile> publishedFiles;
    std::vector<std::string> warnings;
    std::string jobId;
    std::string recordPath;
    std::string configPath;
    std::string logPath;
    ZeroImageFolderResponse() : exitCode(-1), restoredBytes(0) {}
};

/** 对已有 .cplx（或同名 .tif/.raw）按框导出切片报表，不重新成像。 */
struct SliceTxtRequest {
    std::string imagePath;
    int roiX;
    int roiY;
    int roiW; //< <=0 时由程序在图像中心附近找最强点
    int roiH; //< <=0 时高默认等于宽
    std::string outPath;
    std::string taskName;
    std::string taskLeader;
    std::string targetName;
    std::string engineDirectory;
    int timeoutMs;
    SliceTxtRequest() : roiX(0), roiY(0), roiW(0), roiH(0), timeoutMs(0) {}
};

struct SliceTxtResponse {
    std::string txtPath;
    int exitCode;
    SliceTxtResponse() : exitCode(-1) {}
};

#endif
