#ifndef RCS_TYPES_HPP
#define RCS_TYPES_HPP

#include "rcs/export.h"
#include <complex>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// 请求 / 响应 / 选点结构。界面侧自己做控件，把结果填进这些结构再调库。

struct FileHeaderInfo {
    uint16_t flag;
    uint16_t lenFileH;
    uint16_t lenFrmH;
    std::string devName;
    uint16_t devId;
    uint16_t workMode;
    uint16_t waveform;
    uint16_t bandCode;
    float fcGHz;
    float fStartG;
    float fStopG;
    uint16_t fStepMHz;
    uint16_t bwMHz;
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

struct FrameMeta {
    uint16_t sync[4];
    uint32_t N;
    uint32_t PRT;
    uint32_t tauN;
    uint32_t dlyN;
    uint16_t azRaw;
    int16_t rollRaw;

    FrameMeta() : N(0), PRT(0), tauN(0), dlyN(0), azRaw(0), rollRaw(0)
    {
        sync[0] = sync[1] = sync[2] = sync[3] = 0;
    }
};

// 列主序：第 p 个脉冲、第 g 个距离门 → data[p*nr + g]
struct ComplexMatrix {
    int nr;
    int np;
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

struct Level3Info {
    std::string taskName, operatorName, unit, code, source, dataSource, dataType, place, time;
    std::string calMode, calName, calSize, uncertainty;
    std::string tgtName, tgtDesc, tgtClass, tgtL, tgtW, tgtH;
    std::string devName, devClass, sysMode, pulseWidth, prf, dynRange;
    std::string temp, humidity, pressure, visibility, cloud;
    std::string measureTime, fStep, fStart, fStop, pol, tgtHeight;
    std::string azStart, azStop, elStart, elStop, fs, note;
};

struct InteractPick {
    int az; // 方位（脉冲）下标，从 0 起
    int rg; // 距离门下标
    InteractPick() : az(0), rg(0) {}
    InteractPick(int a, int r) : az(a), rg(r) {}
};

struct InteractSelection {
    std::vector<InteractPick> corners;
    std::vector<InteractPick> targets;
    int targetMode; // 1=点目标  2=扩展目标（用距离段）
    std::vector<std::pair<double, double> > extendedRanges;
    double cornerRangeOverrideM; // HRRP 确认峰值距离，<0 表示沿用自动峰

    InteractSelection() : targetMode(1), cornerRangeOverrideM(-1) {}
};

struct GrayImage {
    int width;
    int height;
    std::vector<uint8_t> pixels; // 8bit 灰度，行优先
    GrayImage() : width(0), height(0) {}
};

struct HrrpRequest {
    std::string dataFolder;
    std::string pol;
    std::string tgtName;
    std::string outDir;
    double sigmaTheoryDb;
    int cropN;
    int phaseMode;
    bool autoPick; // true：不弹选点，直接用峰值

    HrrpRequest()
        : pol("HH"), tgtName("目标"), sigmaTheoryDb(18.25), cropN(8192), phaseMode(0), autoPick(false)
    {
    }
};

struct HrrpResponse {
    std::vector<double> rangeM;
    std::vector<double> rcsDbsm;
    std::vector<double> phaseDeg;
    std::string l1Path;
    std::string pdatPath;
    std::string hrrpPath;
    std::string timeSpanTag;
};

struct RcsRequest {
    std::string dataFolder;
    std::string pol;
    std::string tgtName;
    std::string outDir;
    double sigmaTheoryDb;
    int cropN;
    int phaseMode;
    int azMode; // 1 条带  2 聚束
    double vSar;
    bool autoPick;

    RcsRequest()
        : pol("HH"), tgtName("目标"), sigmaTheoryDb(18.25), cropN(8192), phaseMode(0), azMode(2),
          vSar(10.0), autoPick(false)
    {
    }
};

struct RcsTargetItem {
    double rcsDbsm;
    double azViewDeg;
    double rangeM;
    bool valid;
    std::string label;
    std::string rangeSpec; // 扩展目标写 "起~止"；点目标可空

    RcsTargetItem() : rcsDbsm(0), azViewDeg(0), rangeM(0), valid(false) {}
};

struct RcsResponse {
    std::vector<RcsTargetItem> targets;
    std::string l1Path;
    std::string pdatPath;
    std::string rcsPath;
    std::vector<double> rangeM;
    std::vector<double> profileDbsm;
    std::string timeSpanTag;
};

struct CwRcsRequest {
    std::string dataFolder;
    std::string pol;
    std::string tgtName;
    std::string outDir;
    double sigmaTheoryDb;
    int targetMode; // 1 点目标  2 扩展；autoPick 时用
    bool autoPick;

    CwRcsRequest()
        : pol("HH"), tgtName("目标"), sigmaTheoryDb(18.25), targetMode(1), autoPick(false)
    {
    }
};

struct CwRcsResponse {
    std::vector<double> freqGHz;
    std::vector<double> rcsDbsm;
    std::string l3Path;
    std::string timeSpanTag;
    std::vector<std::string> l1Paths;
    std::vector<std::string> l2Paths;
};

#endif
