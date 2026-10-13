#ifndef RCS_TYPES_HPP
#define RCS_TYPES_HPP

#include "rcs/export.h"
#include <complex>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// types.hpp — 请求 / 响应 / 选点结构（纯 C++，不绑 Qt）
//
// 面向人机交互界面：默认 autoPick=false，需实现 Callbacks 选点/框选。
// 界面侧自己做控件，把结果填进这些结构再调 rcs_api.h。
// =============================================================================

// ===== 内部可忽略（二次开发通常不用碰；probe 时可能读到）=====

/** 0 级文件 512B 头解析结果（probeL0File / probeDataFolder 会填） */
struct FileHeaderInfo {
    uint16_t flag;
    uint16_t lenFileH;   ///< 文件头长度，通常 512
    uint16_t lenFrmH;    ///< 帧头长度，通常 256
    std::string devName;
    uint16_t devId;
    uint16_t workMode;
    uint16_t waveform;
    uint16_t bandCode;
    float fcGHz;         ///< 中心频率 GHz
    float fStartG;
    float fStopG;
    uint16_t fStepMHz;
    uint16_t bwMHz;      ///< LFM 带宽 MHz
    uint16_t polComb;
    int year, month, day, hour, minute, second, msec;
    std::vector<uint8_t> rawHeader;

    FileHeaderInfo()
        : flag(0), lenFileH(512), lenFrmH(256), devId(0), workMode(0), waveform(0), bandCode(0),
          fcGHz(0), fStartG(0), fStopG(0), fStepMHz(0), bwMHz(0), polComb(0), year(0), month(0),
          day(0), hour(0), minute(0), second(0), msec(0)
    {
    }
};

/** 单帧头关键字段 */
struct FrameMeta {
    uint16_t sync[4];
    uint32_t N;      ///< 每帧采样点数
    uint32_t PRT;    ///< 脉冲重复周期（点）
    uint32_t tauN;   ///< 发射脉宽（点）
    uint32_t dlyN;   ///< 采集延时（点）
    uint16_t azRaw;
    int16_t rollRaw;

    FrameMeta() : N(0), PRT(0), tauN(0), dlyN(0), azRaw(0), rollRaw(0)
    {
        sync[0] = sync[1] = sync[2] = sync[3] = 0;
    }
};

/** 复数矩阵，列主序：第 p 个脉冲、第 g 个距离门 → data[p*nr + g] */
struct ComplexMatrix {
    int nr; ///< 距离门数
    int np; ///< 脉冲数
    std::vector<std::complex<double> > data;

    ComplexMatrix() : nr(0), np(0) {}

    std::complex<double> &at(int g, int p) { return data[static_cast<size_t>(p) * nr + g]; }
    const std::complex<double> &at(int g, int p) const
    {
        return data[static_cast<size_t>(p) * nr + g];
    }
    void resize(int rows, int cols)
    {
        nr = rows;
        np = cols;
        data.assign(static_cast<size_t>(rows) * cols, std::complex<double>(0.0, 0.0));
    }
};

// ===== 命名常量（填请求字段用，避免魔法数字）=====
namespace rcs {
enum AzMode { AzStrip = 1, AzSpotlight = 2 };
enum TargetMode { TargetPoint = 1, TargetExtended = 2 };
enum Sigma0TargetType { Sigma0Area = 0, Sigma0Point = 1 };
enum Sigma0BgMode { Sigma0BgNone = 0, Sigma0BgRing = 1, Sigma0BgRoi = 2 };
enum Sigma0AnchorMode { Sigma0AnchorLastRow = 0, Sigma0AnchorCenter = 1, Sigma0AnchorCrKnown = 2 };
} // namespace rcs

/** 三级文本产品文件头字段（库写 .hrrp/.rcs/.cs 时用） */
struct Level3Info {
    std::string taskName, operatorName, unit, code, source, dataSource, dataType, place, time;
    std::string calMode, calName, calSize, uncertainty;
    std::string tgtName, tgtDesc, tgtClass, tgtL, tgtW, tgtH;
    std::string devName, devClass, sysMode, pulseWidth, prf, dynRange;
    std::string temp, humidity, pressure, visibility, cloud;
    std::string measureTime, fStep, fStart, fStop, pol, tgtHeight;
    std::string azStart, azStop, elStart, elStop, fs, note;
};

// ===== 选点 / 预览图像 =====

/** 图像上一点：全分辨率下标 */
struct InteractPick {
    int az; ///< 方位（脉冲）下标，从 0
    int rg; ///< 距离门下标，从 0
    InteractPick() : az(0), rg(0) {}
    InteractPick(int a, int r) : az(a), rg(r) {}
};

/** 人机交互选点结果（库侧）；与界面 Qt 类型分开，避免重名 */
struct InteractSelection {
    std::vector<InteractPick> corners;  ///< 角反点
    std::vector<InteractPick> targets;  ///< 点目标
    int targetMode;                     ///< 1=点目标  2=扩展（用距离段）
    std::vector<std::pair<double, double> > extendedRanges; ///< 扩展目标距离起止（米）
    double cornerRangeOverrideM; ///< HRRP 确认峰距离；<0 表示用自动峰

    InteractSelection() : targetMode(1), cornerRangeOverrideM(-1) {}
};

/** 8bit 灰度预览，行优先 pixels[y*width + x] */
struct GrayImage {
    int width;
    int height;
    std::vector<uint8_t> pixels;
    GrayImage() : width(0), height(0) {}
};

/** 矩形 ROI：全分辨率，x=方位列，y=距离行 */
struct RectRoi {
    int x;
    int y;
    int w;
    int h;
    RectRoi() : x(0), y(0), w(0), h(0) {}
    RectRoi(int xx, int yy, int ww, int hh) : x(xx), y(yy), w(ww), h(hh) {}
    bool valid() const { return w > 0 && h > 0; }
};

// ===== 常用业务：HRRP / RCS / 点频 =====

/** HRRP 请求 */
struct HrrpRequest {
    // --- 常用 ---
    std::string dataFolder;   ///< 含 *_L0_<pol>.dat 的目录
    std::string pol;          ///< 极化，默认 "HH"
    std::string tgtName;      ///< 写入文件名的目标名
    std::string outDir;       ///< 输出目录；空则库建「处理结果_HRRP_<极化>」
    double sigmaTheoryDb;     ///< 角反理论 RCS (dBsm)
    bool autoPick;            ///< false=回调 onCornerConfirm（默认）；true=跳过交互用自动峰
    // --- 高级 ---
    int cropN;                ///< 距离向裁剪点数
    int phaseMode;            ///< 相位模式，一般 0

    HrrpRequest()
        : pol("HH"), tgtName("目标"), sigmaTheoryDb(18.25), autoPick(false), cropN(8192), phaseMode(0)
    {
    }
};

/** HRRP 响应：曲线 + 落盘路径 */
struct HrrpResponse {
    std::vector<double> rangeM;   ///< 距离轴（米）
    std::vector<double> rcsDbsm;  ///< RCS (dBsm)
    std::vector<double> phaseDeg; ///< 相位（度）
    std::string l1Path;
    std::string pdatPath;
    std::string hrrpPath;
    std::string timeSpanTag;
};

/** RCS 请求 */
struct RcsRequest {
    // --- 常用 ---
    std::string dataFolder;
    std::string pol;
    std::string tgtName;
    std::string outDir;
    double sigmaTheoryDb;
    int azMode;   ///< rcs::AzStrip=1 / rcs::AzSpotlight=2（默认）
    double vSar;  ///< 条带时平台速度 m/s
    bool autoPick;
    // --- 高级 ---
    int cropN;
    int phaseMode;

    RcsRequest()
        : pol("HH"), tgtName("目标"), sigmaTheoryDb(18.25), azMode(2), vSar(10.0), autoPick(false),
          cropN(8192), phaseMode(0)
    {
    }
};

/** 单个 RCS 目标结果 */
struct RcsTargetItem {
    double rcsDbsm;
    double azViewDeg; ///< 方位视角（度）
    double rangeM;
    bool valid;
    std::string label;
    std::string rangeSpec; ///< 扩展目标写 "起~止"；点目标可空

    RcsTargetItem() : rcsDbsm(0), azViewDeg(0), rangeM(0), valid(false) {}
};

struct RcsResponse {
    std::vector<RcsTargetItem> targets;
    std::string l1Path;
    std::string pdatPath;
    std::string rcsPath;
    std::vector<double> rangeM;      ///< 可选剖面
    std::vector<double> profileDbsm;
    std::string timeSpanTag;
};

/** 点频 RCS 请求（一文件一频点） */
struct CwRcsRequest {
    std::string dataFolder;
    std::string pol;
    std::string tgtName;
    std::string outDir;
    double sigmaTheoryDb;
    int targetMode; ///< 仅 autoPick=true 时有效：1 点目标  2 扩展
    bool autoPick;  ///< false=回调选点（默认）；true=跳过交互

    CwRcsRequest()
        : pol("HH"), tgtName("目标"), sigmaTheoryDb(18.25), targetMode(1), autoPick(false)
    {
    }
};

struct CwRcsResponse {
    std::vector<double> freqGHz;  ///< 频率轴
    std::vector<double> rcsDbsm;  ///< 对应 RCS
    std::string l3Path;           ///< 三级 .rcs
    std::string timeSpanTag;
    std::vector<std::string> l1Paths;
    std::vector<std::string> l2Paths;
};

// ===== 后向散射系数 σ⁰（已成像 RAW，非 L0）=====

struct Sigma0Request {
    // --- 常用 ---
    std::string rawPath;      ///< uint16 LE 振幅图路径
    std::string outDir;
    std::string tgtName;
    int nr;                   ///< 距离向高度（行）
    int na;                   ///< 方位向宽度（列）
    double fcGHz;             ///< 中心频率
    double vs;                ///< 平台地速 m/s
    double incAngleDeg;       ///< 名义入射角（度）
    double hFlight;           ///< 飞行高度 m
    double sigmaTheoryDb;     ///< 角反理论 RCS dBsm（库内转线性）
    int targetType;           ///< 0 区域地物  1 点目标
    int bgMode;               ///< 点目标背景：0 无 / 1 环 / 2 另框
    std::string measureTime;  ///< "yyyy-MM-dd HH:mm:ss.SSS"
    bool autoPick;            ///< false=onRectRoi/onTargetRois 框选（默认）；true=用下面预填 ROI
    RectRoi calBufRoi;        ///< 仅 autoPick=true：角反缓冲区
    std::vector<RectRoi> targetRois; ///< 仅 autoPick=true：地物目标（至少一个）
    RectRoi noiseRoi;         ///< 可选；无效则直方图估噪（交互时也可由回调 stage=noise 写入）
    RectRoi bgRoi;            ///< bgMode=roi 时
    // --- 高级（一般保持默认）---
    double fsHz;
    int kv;
    int M;
    int T;
    int azDecim;
    int anchorMode; ///< rcs::Sigma0Anchor*
    double rCalKnown;
    int rowCrHint;
    int bgRingMargin;

    Sigma0Request()
        : nr(0), na(0), fcGHz(35.0), vs(10.0), incAngleDeg(70.0), hFlight(500.0),
          sigmaTheoryDb(18.25), targetType(0), bgMode(1),
          measureTime("2026-07-17 15:27:00.000"), autoPick(false), fsHz(1.25e9), kv(48), M(8), T(2),
          azDecim(12), anchorMode(0), rCalKnown(0), rowCrHint(0), bgRingMargin(20)
    {
        tgtName = "目标";
    }
};

struct Sigma0TargetItem {
    double sigma0Db;       ///< 写入 .cs 的均值口径 dB
    double sigma0MedianDb;
    double sigmaTarDbsm;   ///< 目标总 RCS dBsm
    double areaM2;
    double neszDb;
    double snrDb;
    int nPix;
    RectRoi roi;
    Sigma0TargetItem()
        : sigma0Db(-999), sigma0MedianDb(-999), sigmaTarDbsm(-999), areaM2(0), neszDb(0),
          snrDb(0), nPix(0)
    {
    }
};

struct Sigma0Response {
    std::string csPath; ///< 写出的 .cs 路径
    double K;           ///< 定标系数
    double pNoise;
    std::vector<Sigma0TargetItem> targets;
    GrayImage preview;  ///< 降采样预览，便于界面显示
    Sigma0Response() : K(0), pNoise(0) {}
};

// ===== [9] RCS 像辐射定标（角反 K → 整景 dBsm TIFF + 三级 txt）=====
// 对应 MATLAB HXSSXS_207_cal_tif.m

namespace rcs {
enum RcsCalAnchor { RcsCalLastRow = 0, RcsCalCenter = 1, RcsCalCrKnown = 2 };
enum RcsCalL3Layout { RcsCalL3Point = 0, RcsCalL3Row = 1 };
} // namespace rcs

struct RcsCalTifRequest {
    std::string calibRawPath; ///< 含角反的定标 RAW
    std::string rawFolder;    ///< 待批量定标的 RAW 目录
    std::string outDir;       ///< 空则用 rawFolder
    std::string tgtName;      ///< 三级文件名前缀，如「林地」
    std::string band;         ///< 波段筛文件 + 波束宽：X/Ku/Ka/W
    std::string pol;          ///< 极化筛文件，空=不按极化筛
    double sigmaTheoryDb;     ///< 角反理论 RCS dBsm
    double fsHz;
    double incAngleDeg;
    double hFlight;
    int anchorMode; ///< rcs::RcsCal*
    double rCalKnown;
    int rowCrHint;
    bool doCalib;   ///< true=框选角反求 K；false=用下面预填的 K/R_near/Rbin
    int l3Layout;   ///< rcs::RcsCalL3Point / Row
    int maxFiles;   ///< <=0 全部
    bool autoPick;  ///< false=onRectRoi 框角反缓冲；true=用 calBufRoi
    RectRoi calBufRoi;
    // doCalib=false 时复用
    double K;
    double R_near;
    double Rbin;
    Level3Info header; ///< 可选覆盖三级头字段（空则用默认）
    // 库内固定（不暴露）：始终写 *_RCS_dBsm.tif + 三级 txt；DN=dBsm×100

    RcsCalTifRequest()
        : band("X"), pol("HH"), sigmaTheoryDb(18.25), fsHz(1.25e9), incAngleDeg(70.0),
          hFlight(500.0), anchorMode(0), rCalKnown(0), rowCrHint(0), doCalib(true), l3Layout(0),
          maxFiles(0), autoPick(false), K(0), R_near(0), Rbin(0)
    {
        tgtName = "目标";
    }
};

struct RcsCalTifFileResult {
    std::string rawName;
    std::string tifPath;
    std::string txtPath;
    int nr;
    int na;
    double dbsmMin;
    double dbsmMax;
    std::string note; ///< 跳过原因等
    RcsCalTifFileResult() : nr(0), na(0), dbsmMin(0), dbsmMax(0) {}
};

struct RcsCalTifResponse {
    double K;
    double R_near;
    double Rbin;
    double R_cal;
    double epsilon_cal;
    double sigma_cal;
    int peakRow;
    int peakCol;
    GrayImage preview;
    std::vector<RcsCalTifFileResult> files;
    RcsCalTifResponse()
        : K(0), R_near(0), Rbin(0), R_cal(0), epsilon_cal(0), sigma_cal(0), peakRow(0), peakCol(0)
    {
    }
};

// ===== 0 级时域/频域预览 =====

/** 单个文件的预览曲线与统计 */
struct L0PulsePreviewItem {
    std::string fileName;
    int fileIndex;  ///< 从文件名解析的序号
    int pulseSel;   ///< 实际选用的脉冲（1 起）
    std::vector<double> tUs;       ///< 时间轴 μs
    std::vector<double> amp;       ///< 时域幅度
    std::vector<double> fMHz;      ///< 频率轴 MHz
    std::vector<double> spectrumDb;///< 归一化频谱 dB
    double ampPkUs;
    double sig1Us;
    double sig2Us;
    double fPkMHz;
    double occBwMHz; ///< 90% 占用带宽

    L0PulsePreviewItem()
        : fileIndex(0), pulseSel(1), ampPkUs(0), sig1Us(0), sig2Us(0), fPkMHz(0), occBwMHz(0)
    {
    }
};

struct L0PreviewRequest {
    std::string dataFolder;
    std::string pol;
    double fsHz;    ///< 采样率，默认 1.25e9
    int pulseNo;    ///< 选用脉冲，1 起
    int nAvg;       ///< >1 时预留平均功率谱（当前可不带回）
    bool removeDC;  ///< 建议 true
    int maxFiles;   ///< <=0 表示全部

    L0PreviewRequest()
        : pol("HH"), fsHz(1.25e9), pulseNo(1), nAvg(1), removeDC(true), maxFiles(0)
    {
    }
};

struct L0PreviewResponse {
    std::vector<L0PulsePreviewItem> items;
};

// ===== 数据探查 =====

/** probeDataFolder 的汇总结果 */
struct L0FolderProbe {
    std::string pol;
    int fileCount;
    std::string firstFile;
    std::string lastFile;
    FileHeaderInfo firstHeader; ///< hasHeader 为 true 时有效
    bool hasHeader;

    L0FolderProbe() : fileCount(0), hasHeader(false) {}
};

// ===== [8] 大图拼接 =====

struct MosaicRequest {
    std::string inFolder;  ///< 输入 TIFF/RAW 目录（支持 uint16 或 int16 RCS_dBsm）
    std::string outFolder; ///< 输出目录
    double medOut;         ///< 匀光目标中值（归一化约 0~1）
    int mode;              ///< 0 自动配准  1 使用 offR/offC
    std::vector<int> offR; ///< mode=1：各图方位向绝对偏移（全分辨率像素）
    std::vector<int> offC; ///< mode=1：各图距离向绝对偏移
    int rawNrHint;         ///< 仅 RAW 时可选尺寸提示；0=自动
    int rawNaHint;
    int l3Layout;          ///< 0=逐点  1=逐行
    std::string band;      ///< 三级头：波段
    std::string pol;       ///< 三级头：极化
    Level3Info header;     ///< 可选覆盖三级头字段
    // 库内固定（不暴露）：始终匀光 + MOSAIC_L3.txt（dBsm=DN/100）；不写 MOSAIC_RCS.tif

    MosaicRequest()
        : medOut(0.35), mode(0), rawNrHint(0), rawNaHint(0), l3Layout(0), band("X"), pol("HH")
    {
    }
};

struct MosaicResponse {
    std::string mosaicTif;  ///< MOSAIC.tif（显示链融合，uint16）
    std::string flattenTif; ///< MOSAIC_匀光.tif（必出）
    std::string displayTif; ///< MOSAIC_显示.tif
    std::string rcsTif;     ///< 本交付不写，恒为空
    std::string l3Path;     ///< MOSAIC_L3.txt（必出）
    std::vector<int> seamCols;
};

#if defined(RCS_ENABLE_TOZERODATA) && RCS_ENABLE_TOZERODATA
// ===== [10] ToZeroData：源 DAT → 四极化 L0 DAT（+可选 .sarrec）；仅 Linux 可选编译 =====

struct ToZeroDataRequest {
    std::string sourcePath;     ///< 源 DAT（UTF-8）
    std::string outputBasePath; ///< 输出基路径，如 .../source_L0.dat（派生 HH/VV/HV/VH）
    bool saveRecovery;          ///< true=额外写 .sarrec 还原信息；默认 false

    ToZeroDataRequest() : saveRecovery(false) {}
};

struct ToZeroDataPolarizationOut {
    std::string polarization; ///< HH / VV / HV / VH
    std::string path;
    std::uint64_t frames;
    std::uint64_t bytes;
    ToZeroDataPolarizationOut() : frames(0), bytes(0) {}
};

struct ToZeroDataResponse {
    std::uint64_t validRecords;
    std::uint64_t skippedRecords;
    std::uint64_t outputBytes; ///< 仅四个 DAT 合计
    std::vector<ToZeroDataPolarizationOut> outputs;
    std::string recoveryPath; ///< saveRecovery 且成功时非空
    std::uint64_t recoveryBytes;
    std::string sourceSha256;
    ToZeroDataResponse()
        : validRecords(0), skippedRecords(0), outputBytes(0), recoveryBytes(0)
    {
    }
};
#endif // RCS_ENABLE_TOZERODATA

#endif
