#include "Level3Text.h"
#include "Util.hpp"
#include <sstream>
#include <iomanip>

static void writeCommonHeader(std::ostream &ts, const Level3Info &info, bool forceEmptyCalNote)
{
    // MATLAB：定标形式 / 定标体名称 / 备注 强制留空
    const std::string calMode = forceEmptyCalNote ? std::string() : info.calMode;
    const std::string calName = forceEmptyCalNote ? std::string() : info.calName;
    const std::string note = forceEmptyCalNote ? std::string() : info.note;

    ts << "[TASK]\n"
       << "任务名称 = " << info.taskName << "\n"
       << "任务负责人 = " << info.operatorName << "\n"
       << "任务单位 = " << info.unit << "\n"
       << "任务代号 = " << info.code << "\n"
       << "任务来源 = " << info.source << "\n"
       << "数据来源 = " << info.dataSource << "\n"
       << "数据类型 = " << info.dataType << "\n"
       << "任务执行地点 = " << info.place << "\n"
       << "任务执行时间 = " << info.time << "\n"
       << "定标形式 = " << calMode << "\n"
       << "定标体名称 = " << calName << "\n"
       << "定标体尺寸 = " << info.calSize << "\n"
       << "不确定度 = " << info.uncertainty << "\n"
       << "[/TASK]\n\n"
       << "[TARGET]\n"
       << "目标名称 = " << info.tgtName << "\n"
       << "目标说明 = " << info.tgtDesc << "\n"
       << "目标分类 = " << info.tgtClass << "\n"
       << "目标长(m) = " << info.tgtL << "\n"
       << "目标宽(m) = " << info.tgtW << "\n"
       << "目标高(m) = " << info.tgtH << "\n"
       << "[/TARGET]\n\n"
       << "[SYSTEM]\n"
       << "设备名称 = " << info.devName << "\n"
       << "设备分类 = " << info.devClass << "\n"
       << "工作体制 = " << info.sysMode << "\n"
       << "脉冲宽度(μs) = " << info.pulseWidth << "\n"
       << "脉冲重复频率 = " << info.prf << "\n"
       << "动态范围(dB) = " << info.dynRange << "\n"
       << "[/SYSTEM]\n\n"
       << "[ENVIRONMENT]\n"
       << "温度(℃) = " << info.temp << "\n"
       << "湿度(%) = " << info.humidity << "\n"
       << "气压(hPa) = " << info.pressure << "\n"
       << "能见度(km) = " << info.visibility << "\n"
       << "云况 = " << info.cloud << "\n"
       << "[/ENVIRONMENT]\n\n"
       << "[CONDITIONS]\n"
       << "测量时刻 = " << info.measureTime << "\n"
       << "频率步长(GHz) = " << info.fStep << "\n"
       << "起始频率(GHz) = " << info.fStart << "\n"
       << "终止频率(GHz) = " << info.fStop << "\n"
       << "极化组合 = " << info.pol << "\n"
       << "目标高度(m) = " << info.tgtHeight << "\n"
       << "起始入射方位视向角(°) = " << info.azStart << "\n"
       << "终止入射方位视向角(°) = " << info.azStop << "\n"
       << "起始入射俯仰视向角(°) = " << info.elStart << "\n"
       << "终止入射俯仰视向角(°) = " << info.elStop << "\n"
       << "回波采样率 = " << info.fs << "\n"
       << "备注 = " << note << "\n"
       << "[/CONDITIONS]\n\n";
}

static std::string fmt(double v, int prec)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(prec) << v;
    return os.str();
}

static bool writeTextUtf8(const std::string &path, const std::string &text, std::string *err)
{
    if (!writeFileAll(path, text.data(), text.size())) {
        if (err)
            *err = "无法创建: " + path;
        return false;
    }
    return true;
}

bool Level3Text::saveHrrp(const std::string &path, const std::vector<double> &R,
                          const std::vector<double> &rcsDbsm, const std::vector<double> &phaseDeg,
                          const Level3Info &info, std::string *err)
{
    std::ostringstream f;
    writeCommonHeader(f, info, true);
    f << "[DATA]\n";
    for (size_t i = 0; i < R.size(); ++i) {
        const double rcs = (i < rcsDbsm.size()) ? rcsDbsm[i] : 0.0;
        if (i < phaseDeg.size())
            f << fmt(R[i], 3) << "  " << fmt(rcs, 2) << "  " << fmt(phaseDeg[i], 2) << "\n";
        else
            f << fmt(R[i], 3) << "  " << fmt(rcs, 2) << "\n";
    }
    f << "[/DATA]\n";
    return writeTextUtf8(path, f.str(), err);
}

bool Level3Text::saveRcs(const std::string &path, const std::vector<double> &rcsDbsm,
                         const std::vector<double> &azView, const std::vector<std::string> &rangeSpec,
                         double timeMs, double radarAz, double radarRoll, const Level3Info &info,
                         std::string *err)
{
    std::ostringstream f;
    writeCommonHeader(f, info, true);
    f << "[DATA]\n";
    for (size_t i = 0; i < rcsDbsm.size(); ++i) {
        const double az = (i < azView.size()) ? azView[i] : 0.0;
        const std::string rg = (i < rangeSpec.size()) ? rangeSpec[i] : std::string("0.00");
        f << fmt(timeMs, 3) << "  " << fmt(rcsDbsm[i], 2) << "  " << fmt(az, 2)
          << "  0.00  0.00  0.00  0.00  " << fmt(radarAz, 2) << "  " << fmt(radarRoll, 2) << "  "
          << rg << "\n";
    }
    f << "[/DATA]\n";
    return writeTextUtf8(path, f.str(), err);
}

bool Level3Text::saveCwRcs(const std::string &path, const std::vector<double> &freqGHz,
                           const std::vector<double> &rcsDbsm, const std::vector<double> &tmsMs,
                           const std::vector<double> &azDeg, const std::vector<double> &rollDeg,
                           const std::string &rangeCol, const Level3Info &info, std::string *err)
{
    (void)tmsMs;
    Level3Info info2 = info;
    if (info2.dataType.empty())
        info2.dataType = "点频单站RCS";
    std::ostringstream f;
    writeCommonHeader(f, info2, true);
    f << "[DATA]\n";
    for (size_t i = 0; i < freqGHz.size(); ++i) {
        const double rcs = (i < rcsDbsm.size()) ? rcsDbsm[i] : 0.0;
        const double az = (i < azDeg.size()) ? azDeg[i] : 0.0;
        const double roll = (i < rollDeg.size()) ? rollDeg[i] : 0.0;
        f << fmt(freqGHz[i], 6) << "  ";
        if (rcs == rcs)
            f << fmt(rcs, 2);
        else
            f << "NaN";
        f << "  0.00  0.00  0.00  0.00  0.00  " << fmt(az, 2) << "  " << fmt(roll, 2) << "  "
          << rangeCol << "\n";
    }
    f << "[/DATA]\n";
    return writeTextUtf8(path, f.str(), err);
}
