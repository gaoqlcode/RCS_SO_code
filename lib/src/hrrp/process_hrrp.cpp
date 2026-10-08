#include "Calibration.h"
#include "Level0Dat.h"
#include "Level2Pdat.h"
#include "Level3Text.h"
#include "MergeDat.h"
#include "OutPath.h"
#include "Preprocess.h"
#include "PulseCompress.h"
#include "Util.hpp"
#include "rcs/rcs_api.h"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

static bool cancelled(Callbacks &cb)
{
    return cb.isCancelled && cb.isCancelled();
}

static void progress(Callbacks &cb, int p, const std::string &m)
{
    if (cb.onProgress)
        cb.onProgress(p, m);
}

static void message(Callbacks &cb, const std::string &m)
{
    if (cb.onMessage)
        cb.onMessage(m);
}

static std::string nowCompact()
{
    const time_t t = time(0);
    struct tm tmbuf;
#if defined(_WIN32)
    localtime_s(&tmbuf, &t);
#else
    localtime_r(&t, &tmbuf);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d", tmbuf.tm_year + 1900, tmbuf.tm_mon + 1,
                  tmbuf.tm_mday, tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec);
    return buf;
}

static std::string nowPretty()
{
    const time_t t = time(0);
    struct tm tmbuf;
#if defined(_WIN32)
    localtime_s(&tmbuf, &t);
#else
    localtime_r(&t, &tmbuf);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", tmbuf.tm_year + 1900, tmbuf.tm_mon + 1,
                  tmbuf.tm_mday, tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec);
    return buf;
}

const char *rcsVersion() { return "1.0.0"; }

bool processHrrp(const HrrpRequest &req, HrrpResponse &resp, Callbacks &cb, std::string *err)
{
    message(cb, "开始 HRRP 处理…");
    const std::string outDir = resolveOutDir(req.outDir, req.dataFolder, hrrpResultTag(req.pol));
    if (outDir.empty()) {
        if (err)
            *err = "输出目录无效";
        return false;
    }
    makeDirs(outDir);
    message(cb, "输出目录: " + outDir);

    std::string merged;
    std::string localErr;
    MergeSidecar side;
    if (!MergeDat::mergeFolder(req.dataFolder, req.pol, merged, &side, &localErr, outDir,
                               [&]() { return cancelled(cb); })) {
        if (err)
            *err = localErr;
        return false;
    }
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }
    progress(cb, 10, "合并完成，读取数据…");
    std::string timeSpanTag = mergeTimeSpanTag(side);
    if (timeSpanTag.empty())
        timeSpanTag = nowCompact();
    const std::string timePretty = mergeTimeSpanPretty(side);

    FileHeaderInfo hdr;
    FrameMeta fm;
    ComplexMatrix data;
    std::vector<uint16_t> azList;
    if (!Level0Dat::readFile(
            merged, hdr, fm, data, azList, req.phaseMode, -1,
            [&](int p) { progress(cb, 10 + p / 5, "读取数据…"); }, &localErr,
            [&]() { return cancelled(cb); })) {
        if (err)
            *err = localErr;
        return false;
    }
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }

    progress(cb, 30, "去直流 / 直达波…");
    const double fs = Level0Dat::kFsForced;
    const double pulseWidthUs = fm.tauN / fs * 1e6;
    Preprocess::removeDc(data);
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }
    Preprocess::removeDirectWave(data, pulseWidthUs, fs);
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }

    progress(cb, 45, "脉冲压缩…");
    PulseCompress::apply(
        data, hdr.bwMHz, pulseWidthUs, fs,
        [&](int p) {
            progress(cb, 45 + p * 20 / 100, std::string("脉冲压缩 ") + std::to_string(p) + "%");
        },
        [&]() { return cancelled(cb); });
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }

    std::vector<double> profile, R;
    Calibration::meanPowerProfile(data, profile);
    R.resize(static_cast<size_t>(data.nr));
    {
        const double t0 = fm.dlyN / fs;
        const double dr = Level0Dat::kC / (2.0 * fs);
        for (size_t i = 0; i < R.size(); ++i)
            R[i] = Level0Dat::kC * t0 / 2.0 + static_cast<double>(i) * dr;
    }

    int pk = Calibration::argMax(profile);
    std::vector<double> profileDb(profile.size());
    for (size_t i = 0; i < profile.size(); ++i)
        profileDb[i] = 10.0 * std::log10(imax(profile[i], 1e-30));

    // 选点：要么自动用峰，要么交给界面回调
    InteractSelection sel;
    if (req.autoPick || !cb.onCornerConfirm) {
        sel.cornerRangeOverrideM = -1;
    } else {
        if (!cb.onCornerConfirm(R, profileDb, pk, sel)) {
            if (err)
                *err = "已取消";
            return false;
        }
    }
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }
    if (sel.cornerRangeOverrideM > 0) {
        double best = 1e99;
        for (size_t i = 0; i < R.size(); ++i) {
            const double d = std::abs(R[i] - sel.cornerRangeOverrideM);
            if (d < best) {
                best = d;
                pk = static_cast<int>(i);
            }
        }
    }

    progress(cb, 60, "裁剪距离窗…");
    Calibration::cropAroundPeak(data, profile, R, pk, req.cropN);
    pk = Calibration::argMax(profile);

    const double sigmaCal = std::pow(10.0, req.sigmaTheoryDb / 10.0);
    int g1 = 0, g2 = 0, bw = 0;
    Calibration::gateWindowFromPeak(profile, pk, 6.0, 1.0, static_cast<int>(profile.size()), 3.0, g1, g2,
                                    bw);
    double Pcorner = 0, bg = 0;
    Calibration::gateNetPower(profile, g1, g2, static_cast<int>(profile.size()), Pcorner, bg);
    const double Rcorner = R[static_cast<size_t>(pk)];

    std::vector<double> rcsDbsm, phaseDeg(static_cast<size_t>(data.nr));
    Calibration::toRcsProfile(profile, R, sigmaCal, Pcorner, Rcorner, rcsDbsm);
    for (int g = 0; g < data.nr; ++g) {
        std::complex<double> m(0, 0);
        for (int p = 0; p < data.np; ++p)
            m += data.at(g, p);
        phaseDeg[static_cast<size_t>(g)] =
            std::arg(m / static_cast<double>(imax(1, data.np))) * 180.0 / 3.14159265358979323846;
    }

    std::string measureTime, acqDate;
    if (hdr.year >= 2000 && hdr.year <= 2100) {
        char compact[32], pretty[64];
        std::snprintf(compact, sizeof(compact), "%04d%02d%02d%02d%02d%02d", hdr.year, hdr.month, hdr.day,
                      hdr.hour, hdr.minute, hdr.second);
        std::snprintf(pretty, sizeof(pretty), "%04d-%02d-%02d %02d:%02d:%02d", hdr.year, hdr.month, hdr.day,
                      hdr.hour, hdr.minute, hdr.second);
        measureTime = compact;
        acqDate = pretty;
    } else {
        measureTime = nowCompact();
        acqDate = nowPretty();
    }

    resp.rangeM = R;
    resp.rcsDbsm = rcsDbsm;
    resp.phaseDeg = phaseDeg;
    resp.timeSpanTag = timeSpanTag;
    resp.l1Path = pathJoin(outDir, req.tgtName + timeSpanTag + ".dat");
    // 正式 1 级：合并结果改名拷贝
    if (!copyFileBytes(merged, resp.l1Path)) {
        if (err)
            *err = "无法写出 1 级 dat: " + resp.l1Path;
        return false;
    }
    resp.hrrpPath = pathJoin(outDir, req.tgtName + timeSpanTag + ".hrrp");
    resp.pdatPath = pathJoin(outDir, req.tgtName + timeSpanTag + ".pdat");

    Level3Info info;
    info.devName = hdr.devName;
    info.sysMode = "模式" + std::to_string(hdr.workMode) + "/波形" + std::to_string(hdr.waveform)
                   + "/波段" + std::to_string(hdr.bandCode);
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.4f", pulseWidthUs);
        info.pulseWidth = buf;
        std::snprintf(buf, sizeof(buf), "%.3f", fs / fm.PRT);
        info.prf = buf;
        std::snprintf(buf, sizeof(buf), "%.4f", hdr.fStepMHz / 1000.0);
        info.fStep = buf;
        std::snprintf(buf, sizeof(buf), "%.4f", hdr.fStartG);
        info.fStart = buf;
        std::snprintf(buf, sizeof(buf), "%.4f", hdr.fStopG);
        info.fStop = buf;
        std::snprintf(buf, sizeof(buf), "%.0f", fs);
        info.fs = buf;
    }
    info.pol = req.pol;
    info.tgtName = req.tgtName;
    info.measureTime = timePretty.empty() ? acqDate : timePretty;
    info.time = info.measureTime;
    if (!azList.empty()) {
        uint16_t azMin = azList[0], azMax = azList[0];
        for (size_t i = 0; i < azList.size(); ++i) {
            azMin = imin(azMin, azList[i]);
            azMax = imax(azMax, azList[i]);
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", azMin / 100.0);
        info.azStart = buf;
        std::snprintf(buf, sizeof(buf), "%.2f", azMax / 100.0);
        info.azStop = buf;
    }

    progress(cb, 85, "写出 .hrrp / .pdat…");
    if (!Level3Text::saveHrrp(resp.hrrpPath, R, rcsDbsm, phaseDeg, info, &localErr)) {
        if (err)
            *err = localErr;
        return false;
    }
    if (!Level2Pdat::save1D(resp.pdatPath, rcsDbsm, phaseDeg, Rcorner, req.sigmaTheoryDb, hdr, fm, 100,
                            &localErr)) {
        if (err)
            *err = localErr;
        return false;
    }

    progress(cb, 100, "HRRP 完成");
    return true;
}
