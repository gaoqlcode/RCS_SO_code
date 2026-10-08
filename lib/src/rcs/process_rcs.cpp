#include "AzimuthFocus.h"
#include "Calibration.h"
#include "DisplayPreview.h"
#include "Level0Dat.h"
#include "Level2Pdat.h"
#include "Level3Text.h"
#include "MergeDat.h"
#include "OutPath.h"
#include "Preprocess.h"
#include "PulseCompress.h"
#include "Util.hpp"
#include "rcs/rcs_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

static bool cancelled(Callbacks &cb) { return cb.isCancelled && cb.isCancelled(); }

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

bool processRcs(const RcsRequest &req, RcsResponse &resp, Callbacks &cb, std::string *err)
{
    message(cb, "开始 RCS 处理…");
    const std::string outDir = resolveOutDir(req.outDir, req.dataFolder, rcsResultTag(req.pol));
    if (outDir.empty()) {
        if (err)
            *err = "输出目录无效";
        return false;
    }
    makeDirs(outDir);
    message(cb, "输出目录: " + outDir);

    MergeSidecar side;
    std::string merged, localErr;
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
    progress(cb, 8, "合并完成");
    std::string timeSpanTag = mergeTimeSpanTag(side);
    if (timeSpanTag.empty())
        timeSpanTag = nowCompact();
    const std::string timePretty = mergeTimeSpanPretty(side);
    const double batchTimeMs = mergeStartTimeMs(side);

    FileHeaderInfo hdr;
    FrameMeta fm;
    ComplexMatrix data;
    std::vector<uint16_t> azList;
    if (!Level0Dat::readFile(
            merged, hdr, fm, data, azList, req.phaseMode, -1,
            [&](int p) { progress(cb, 8 + p / 5, "读取…"); }, &localErr,
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

    const double fs = Level0Dat::kFsForced;
    const double pulseWidthUs = fm.tauN / fs * 1e6;
    const double prf = fs / fm.PRT;
    progress(cb, 30, "预处理…");
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
    progress(cb, 40, "脉冲压缩…");
    PulseCompress::apply(
        data, hdr.bwMHz, pulseWidthUs, fs,
        [&](int p) { progress(cb, 40 + p * 12 / 100, "脉冲压缩 " + std::to_string(p) + "%"); },
        [&]() { return cancelled(cb); });
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }

    std::vector<double> profile, R(static_cast<size_t>(data.nr));
    Calibration::meanPowerProfile(data, profile);
    {
        const double t0 = fm.dlyN / fs;
        const double dr = Level0Dat::kC / (2.0 * fs);
        for (size_t i = 0; i < R.size(); ++i)
            R[i] = Level0Dat::kC * t0 / 2.0 + static_cast<double>(i) * dr;
    }
    int pk0 = Calibration::argMax(profile);
    Calibration::cropAroundPeak(data, profile, R, pk0, req.cropN);
    int pkR = Calibration::argMax(profile);

    const double sigmaCal = std::pow(10.0, req.sigmaTheoryDb / 10.0);
    int g1c = 0, g2c = 0, bwC = 0;
    Calibration::gateWindowFromPeak(profile, pkR, 6.0, 1.0, static_cast<int>(profile.size()), 3.0, g1c,
                                    g2c, bwC);
    double Pcorner1D = 0, bgC = 0;
    Calibration::gateNetPower(profile, g1c, g2c, static_cast<int>(profile.size()), Pcorner1D, bgC);
    const double RcornerAuto = R[static_cast<size_t>(pkR)];

    const double lambda = Level0Dat::kC / (hdr.fcGHz * 1e9);
    // 选点/精修用脉压未方位压缩数据；L2 用 focused
    ComplexMatrix rangeCompressed = data;
    progress(cb, 55, "方位向压缩…");
    AzimuthFocus::apply(
        data, R, req.azMode, req.vSar, lambda, prf,
        [&](int p) { progress(cb, 55 + p * 12 / 100, "方位压缩 " + std::to_string(p) + "%"); },
        [&]() { return cancelled(cb); });
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }
    ComplexMatrix &focused = data;
    const double E_corner_2D = Calibration::cornerEnergy2D(focused, pkR, g1c, g2c);

    message(cb, "生成选点预览…");
    const GrayImage preview = makeAmpPreview(rangeCompressed);
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }

    // 角反：命令行/批处理走峰值；界面把点填进 cornerSel.corners
    InteractSelection cornerSel;
    if (req.autoPick || !cb.onCornerPick) {
        InteractPick pt;
        pt.rg = pkR;
        pt.az = rangeCompressed.np / 2;
        cornerSel.corners.push_back(pt);
    } else {
        if (!cb.onCornerPick(R, preview, rangeCompressed.nr, rangeCompressed.np, cornerSel) || cornerSel.corners.empty()) {
            if (err)
                *err = cancelled(cb) ? "已取消" : "未选择角反";
            return false;
        }
    }

    std::string measureTime = timeSpanTag;
    double timeMs = batchTimeMs, radarAz = 0, radarRoll = 0;
    if (!side.fileRadarAz.empty()) {
        radarAz = side.fileRadarAz[0] / 100.0;
        radarRoll = side.fileRadarRoll.empty() ? 0 : side.fileRadarRoll[0] / 100.0;
    }

    resp.l1Path = pathJoin(outDir, req.tgtName + timeSpanTag + ".dat");
    if (!copyFileBytes(merged, resp.l1Path)) {
        if (err)
            *err = "无法写出 1 级 dat: " + resp.l1Path;
        return false;
    }
    const std::string pdatPath = pathJoin(outDir, req.tgtName + timeSpanTag + ".pdat");
    progress(cb, 70, "保存2级 pdat…");
    std::vector<uint16_t> azCross(static_cast<size_t>(focused.np));
    for (int i = 0; i < focused.np; ++i)
        azCross[static_cast<size_t>(i)] = static_cast<uint16_t>(i);
    if (!Level2Pdat::save2D(pdatPath, focused, R, sigmaCal, RcornerAuto, E_corner_2D, hdr, fm, azCross, 100,
                            &localErr)) {
        if (err)
            *err = localErr;
        return false;
    }

    const std::vector<double> powerDb = meanPowerDb(focused);
    InteractSelection tgtSel;
    if (req.autoPick || !cb.onTargetPick) {
        InteractPick pt;
        pt.rg = pkR;
        pt.az = rangeCompressed.np / 2;
        tgtSel.targets.push_back(pt);
        tgtSel.targetMode = 1;
    } else {
        if (!cb.onTargetPick(R, preview, rangeCompressed.nr, rangeCompressed.np, powerDb, 0, tgtSel)) {
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

    constexpr double kValidMinDb = 3.0;
    std::vector<double> Pc_all, Rc_all;
    for (const InteractPick &pt : cornerSel.corners) {
        int rg = ibound(0, pt.rg, focused.nr - 1);
        int az = ibound(0, pt.az, focused.np - 1);
        int bestG = rg, bestA = az;
        double best = 0;
        for (int g = imax(0, rg - 5); g <= imin(rangeCompressed.nr - 1, rg + 5); ++g)
            for (int a = imax(0, az - 7); a <= imin(rangeCompressed.np - 1, az + 7); ++a) {
                const double v = std::norm(rangeCompressed.at(g, a));
                if (v > best) {
                    best = v;
                    bestG = g;
                    bestA = a;
                }
            }
        (void)bestA;
        bestG = Calibration::snapPeak1D(profile, bestG, 30, static_cast<int>(profile.size()));
        int g1 = 0, g2 = 0, bw = 0;
        Calibration::gateWindowFromPeak(profile, bestG, 6.0, 1.0, static_cast<int>(profile.size()), 3.0, g1,
                                        g2, bw);
        double Pnet = 0, bg = 0;
        Calibration::gateNetPower(profile, g1, g2, static_cast<int>(profile.size()), Pnet, bg);
        Pc_all.push_back(Pnet);
        Rc_all.push_back(R[static_cast<size_t>(bestG)]);
    }
    double R_ref = 0;
    for (double v : Rc_all)
        R_ref += v;
    R_ref /= imax(1, static_cast<int>(Rc_all.size()));
    double P_cal = 0;
    for (size_t i = 0; i < Pc_all.size(); ++i)
        P_cal += Pc_all[i] * std::pow(Rc_all[i] / R_ref, 4.0);
    P_cal /= imax(1, static_cast<int>(Pc_all.size()));

    resp.pdatPath = pdatPath;
    resp.rangeM = R;
    std::vector<double> profDb;
    Calibration::toRcsProfile(profile, R, sigmaCal, Pcorner1D, RcornerAuto, profDb);
    resp.profileDbsm = profDb;
    resp.targets.clear();

    if (tgtSel.targetMode == 2) {
        for (size_t i = 0; i < tgtSel.extendedRanges.size(); ++i) {
            const double r1 = tgtSel.extendedRanges[i].first;
            const double r2 = tgtSel.extendedRanges[i].second;
            int i1 = 0, i2 = static_cast<int>(R.size()) - 1;
            for (size_t k = 0; k < R.size(); ++k) {
                if (R[k] >= r1) {
                    i1 = static_cast<int>(k);
                    break;
                }
            }
            for (int k = static_cast<int>(R.size()) - 1; k >= 0; --k) {
                if (R[static_cast<size_t>(k)] <= r2) {
                    i2 = k;
                    break;
                }
            }
            if (i2 <= i1)
                continue;
            std::vector<double> bgseg;
            const int Nw = i2 - i1 + 1;
            for (int k = imax(0, i1 - Nw); k < i1; ++k)
                bgseg.push_back(profile[static_cast<size_t>(k)]);
            for (int k = i2 + 1; k <= imin(static_cast<int>(profile.size()) - 1, i2 + Nw); ++k)
                bgseg.push_back(profile[static_cast<size_t>(k)]);
            double bg = 0;
            if (!bgseg.empty()) {
                std::nth_element(bgseg.begin(),
                                 bgseg.begin() + static_cast<std::ptrdiff_t>(bgseg.size() / 2), bgseg.end());
                bg = bgseg[bgseg.size() / 2];
            }
            double sigLin = 0;
            double rMean = 0;
            int cnt = 0;
            double netSum = 0;
            for (int k = i1; k <= i2; ++k) {
                const double net = imax(profile[static_cast<size_t>(k)] - bg, 0.0);
                netSum += net;
                sigLin += net * (sigmaCal / imax(P_cal, 1e-30))
                          * std::pow(R[static_cast<size_t>(k)] / R_ref, 4.0);
                rMean += R[static_cast<size_t>(k)];
                ++cnt;
            }
            RcsTargetItem t;
            t.rangeM = rMean / imax(1, cnt);
            t.azViewDeg = 0;
            t.label = "扩展目标" + std::to_string(i + 1);
            {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.2f~%.2f", r1, r2);
                t.rangeSpec = buf;
            }
            if (netSum <= 0) {
                t.valid = false;
                message(cb, t.label + ": 段内不高于背景，不出数");
            } else {
                t.valid = true;
                t.rcsDbsm = 10.0 * std::log10(imax(sigLin, 1e-30));
            }
            resp.targets.push_back(t);
        }
    } else {
        for (size_t i = 0; i < tgtSel.targets.size(); ++i) {
            const InteractPick &pt = tgtSel.targets[i];
            int rg = ibound(0, pt.rg, rangeCompressed.nr - 1);
            int az = ibound(0, pt.az, rangeCompressed.np - 1);
            int bestG = rg, bestA = az;
            double best = 0;
            for (int g = imax(0, rg - 5); g <= imin(rangeCompressed.nr - 1, rg + 5); ++g)
                for (int a = imax(0, az - 7); a <= imin(rangeCompressed.np - 1, az + 7); ++a) {
                    const double v = std::norm(rangeCompressed.at(g, a));
                    if (v > best) {
                        best = v;
                        bestG = g;
                        bestA = a;
                    }
                }
            bestG = Calibration::snapPeak1D(profile, bestG, 30, static_cast<int>(profile.size()));
            int g1 = 0, g2 = 0, bw = 0;
            Calibration::gateWindowFromPeak(profile, bestG, 6.0, 1.0, static_cast<int>(profile.size()), 3.0,
                                            g1, g2, bw);
            double Pt = 0, bg = 0;
            Calibration::gateNetPower(profile, g1, g2, static_cast<int>(profile.size()), Pt, bg);
            const double snrGateDb =
                10.0
                * std::log10(imax(profile[static_cast<size_t>(bestG)], 1e-30) / imax(bg, 1e-30));

            RcsTargetItem t;
            t.rangeM = R[static_cast<size_t>(bestG)];
            t.azViewDeg =
                azList.empty()
                    ? bestA
                    : (azList[static_cast<size_t>(imin(bestA, static_cast<int>(azList.size()) - 1))]
                       / 100.0);
            t.label = "点目标" + std::to_string(i + 1);
            if (Pt <= 0 || snrGateDb < kValidMinDb) {
                t.valid = false;
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s @ %.1f m: 峰门仅高背景 %.2f dB(<%.0f)，不出数",
                              t.label.c_str(), t.rangeM, snrGateDb, kValidMinDb);
                message(cb, buf);
            } else {
                t.valid = true;
                t.rcsDbsm = req.sigmaTheoryDb
                            + 10.0 * std::log10(imax(Pt, 1e-30) / imax(P_cal, 1e-30))
                            + 40.0 * std::log10(t.rangeM / R_ref);
            }
            resp.targets.push_back(t);
        }
    }

    std::vector<double> rcsVals, azViews;
    std::vector<std::string> rangeSpecs;
    for (size_t ti = 0; ti < resp.targets.size(); ++ti) {
        const RcsTargetItem &tg = resp.targets[ti];
        if (!tg.valid)
            continue;
        rcsVals.push_back(tg.rcsDbsm);
        azViews.push_back(tg.azViewDeg);
        if (!tg.rangeSpec.empty())
            rangeSpecs.push_back(tg.rangeSpec);
        else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", tg.rangeM);
            rangeSpecs.push_back(buf);
        }
    }
    resp.timeSpanTag = timeSpanTag;
    resp.rcsPath = pathJoin(outDir, req.tgtName + timeSpanTag + ".rcs");
    Level3Info info;
    info.tgtName = req.tgtName;
    info.devName = hdr.devName;
    info.sysMode = "模式" + std::to_string(hdr.workMode) + "/波形" + std::to_string(hdr.waveform)
                   + "/波段" + std::to_string(hdr.bandCode);
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.4f", pulseWidthUs);
        info.pulseWidth = buf;
        std::snprintf(buf, sizeof(buf), "%.3f", prf);
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
    info.measureTime = timePretty.empty() ? measureTime : timePretty;
    info.time = info.measureTime;
    info.calMode.clear();
    info.calName.clear();
    info.note.clear();
    if (!azList.empty()) {
        uint16_t azMin = azList[0], azMax = azList[0];
        for (uint16_t a : azList) {
            azMin = imin(azMin, a);
            azMax = imax(azMax, a);
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", azMin / 100.0);
        info.azStart = buf;
        std::snprintf(buf, sizeof(buf), "%.2f", azMax / 100.0);
        info.azStop = buf;
    }
    if (!rcsVals.empty()) {
        if (!Level3Text::saveRcs(resp.rcsPath, rcsVals, azViews, rangeSpecs, timeMs, radarAz, radarRoll, info,
                                 &localErr)) {
            if (err)
                *err = localErr;
            return false;
        }
    } else {
        message(cb, "无有效目标 RCS，未写出 .rcs");
        resp.rcsPath.clear();
    }

    progress(cb, 100, "RCS 完成");
    return true;
}

