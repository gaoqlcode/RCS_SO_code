#include "Calibration.h"
#include "Level0Dat.h"
#include "Level2Pdat.h"
#include "Level3Text.h"
#include "OutPath.h"
#include "Util.hpp"
#include "BinaryIo.h"
#include "Preprocess.h"
#include "PulseCompress.h"
#include "rcs/rcs_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <limits>
#include <regex>
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

static int firstMultiDigit(const std::string &name)
{
    static const std::regex re("(\\d{2,})");
    std::smatch m;
    if (!std::regex_search(name, m, re)) {
        static const std::regex re1("(\\d+)");
        if (!std::regex_search(name, m, re1))
            return 0;
        return std::stoi(m[1].str());
    }
    return std::stoi(m[1].str());
}

static double localPct(const std::vector<double> &x, double p)
{
    if (x.empty())
        return 0;
    std::vector<double> xs = x;
    std::sort(xs.begin(), xs.end());
    const size_t n = xs.size();
    if (n == 1)
        return xs[0];
    const double r = p / 100.0 * (static_cast<double>(n) - 1.0);
    const size_t i0 = static_cast<size_t>(std::floor(r));
    const size_t i1 = imin(i0 + 1, n - 1);
    const double w = r - std::floor(r);
    return xs[i0] * (1.0 - w) + xs[i1] * w;
}

static double localBgLevel(const std::vector<double> &prof, int g1, int g2, int N, int excl,
                           double pct)
{
    std::vector<double> maskVals;
    maskVals.reserve(static_cast<size_t>(N));
    const int lo = imax(1, g1 - excl);
    const int hi = imin(N, g2 + excl);
    for (int g = 0; g < N; ++g) {
        if (g + 1 >= lo && g + 1 <= hi)
            continue;
        maskVals.push_back(prof[static_cast<size_t>(g)]);
    }
    double bg1 = std::numeric_limits<double>::infinity();
    if (!maskVals.empty()) {
        std::nth_element(maskVals.begin(),
                         maskVals.begin() + static_cast<std::ptrdiff_t>(maskVals.size() / 2),
                         maskVals.end());
        bg1 = maskVals[maskVals.size() / 2];
    }
    const double bg2 = localPct(prof, pct);
    double bg = imin(bg1, bg2);
    if (!std::isfinite(bg))
        bg = 0;
    return bg;
}

static double netPower(const std::vector<double> &p, int g1, int g2, int N, int guard, int excl,
                       double pct)
{
    double sum = 0;
    for (int g = g1; g <= g2; ++g)
        sum += p[static_cast<size_t>(g - 1)];
    if (guard < 0)
        return sum;
    const int w = g2 - g1 + 1;
    const double bg = localBgLevel(p, g1, g2, N, excl, pct);
    double P = sum - bg * w;
    if (!std::isfinite(P)) {
        P = sum;
    }
    return P;
}

static void segNetPower(const std::vector<double> &p, int g1, int g2, int N, int guard, int excl,
                        double pct, double &P, double &bg)
{
    bg = localBgLevel(p, g1, g2, N, excl, pct);
    const int w = g2 - g1 + 1;
    P = 0;
    for (int g = g1; g <= g2; ++g)
        P += p[static_cast<size_t>(g - 1)];
    P -= bg * w;
    if (!std::isfinite(P)) {
        P = 0;
        for (int g = g1; g <= g2; ++g)
            P += p[static_cast<size_t>(g - 1)];
    }
    if (guard < 0) {
        P = 0;
        for (int g = g1; g <= g2; ++g)
            P += p[static_cast<size_t>(g - 1)];
    }
}

static double medianOf(const std::vector<double> &v, const std::vector<bool> &ok)
{
    std::vector<double> s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i < ok.size() && ok[i])
            s.push_back(v[i]);
    }
    if (s.empty())
        return 0;
    std::nth_element(s.begin(), s.begin() + static_cast<std::ptrdiff_t>(s.size() / 2), s.end());
    return s[s.size() / 2];
}

static double madScale(const std::vector<double> &v, const std::vector<bool> &ok, double med)
{
    std::vector<double> d;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i < ok.size() && ok[i])
            d.push_back(std::abs(v[i] - med));
    }
    if (d.empty())
        return 0;
    std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(d.size() / 2), d.end());
    return 1.4826 * d[d.size() / 2];
}

static bool endsWithIgnoreCase(const std::string &s, const std::string &suf)
{
    if (s.size() < suf.size())
        return false;
    for (size_t i = 0; i < suf.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(s[s.size() - suf.size() + i]);
        const unsigned char b = static_cast<unsigned char>(suf[i]);
        if (std::tolower(a) != std::tolower(b))
            return false;
    }
    return true;
}

static double headerTimeMs(const FileHeaderInfo &hdr)
{
    double usec = 0;
    if (hdr.rawHeader.size() >= 274)
        usec = readU16LE(hdr.rawHeader.data() + 272) / 1000.0;
    return (hdr.hour * 3600 + hdr.minute * 60 + hdr.second) * 1000.0 + hdr.msec + usec;
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

static GrayImage profileGrayImage(const std::vector<double> &profileDb)
{
    GrayImage img;
    img.width = 1;
    img.height = static_cast<int>(profileDb.size());
    img.pixels.resize(profileDb.size());
    double lo = profileDb[0], hi = profileDb[0];
    for (size_t i = 0; i < profileDb.size(); ++i) {
        lo = imin(lo, profileDb[i]);
        hi = imax(hi, profileDb[i]);
    }
    const double span = imax(hi - lo, 1e-6);
    for (size_t i = 0; i < profileDb.size(); ++i) {
        const double t = (profileDb[i] - lo) / span;
        img.pixels[i] = static_cast<uint8_t>(ibound(0, static_cast<int>(std::lround(t * 255.0)), 255));
    }
    return img;
}

bool processCwRcs(const CwRcsRequest &req, CwRcsResponse &resp, Callbacks &cb, std::string *err)
{
    message(cb, "开始点频 RCS 处理…");
    const std::string outDir = resolveOutDir(req.outDir, req.dataFolder, cwRcsResultTag(req.pol));
    if (outDir.empty()) {
        if (err)
            *err = "输出目录无效";
        return false;
    }
    makeDirs(outDir);
    message(cb, "输出目录: " + outDir);

    if (!isDir(req.dataFolder)) {
        if (err)
            *err = "数据目录无效";
        return false;
    }

    const std::string pol = toUpperStr(trimStr(req.pol));
    const std::string suffix = std::string("_L0_") + pol + ".dat";
    std::vector<std::string> names = listFileNames(req.dataFolder);
    struct FileItem {
        std::string path;
        std::string name;
        int fnum;
    };
    std::vector<FileItem> files;
    for (size_t i = 0; i < names.size(); ++i) {
        if (!endsWithIgnoreCase(names[i], suffix))
            continue;
        const int fn = firstMultiDigit(names[i]);
        if (fn <= 0)
            continue;
        FileItem it;
        it.name = names[i];
        it.path = pathJoin(req.dataFolder, names[i]);
        it.fnum = fn;
        files.push_back(it);
    }
    if (files.empty()) {
        if (err)
            *err = "未找到 *_L0_" + pol + ".dat";
        return false;
    }
    std::sort(files.begin(), files.end(),
              [](const FileItem &a, const FileItem &b) { return a.fnum < b.fnum; });

    const int Nf = static_cast<int>(files.size());
    message(cb, "同极化 dat " + std::to_string(Nf) + " 个");

    std::string localErr;
    FileHeaderInfo hdr0;
    FrameMeta fm0;
    ComplexMatrix dummy;
    std::vector<uint16_t> azDummy;
    if (!Level0Dat::readFile(files[0].path, hdr0, fm0, dummy, azDummy, 0, 1, nullptr, &localErr,
                             [&]() { return cancelled(cb); })) {
        if (err)
            *err = localErr;
        return false;
    }

    const double fStart = hdr0.fStartG;
    const double fStepMHz = hdr0.fStepMHz;
    if (fStepMHz <= 0) {
        if (err)
            *err = "频率步进为 0";
        return false;
    }

    std::vector<double> freq(static_cast<size_t>(Nf));
    for (int i = 0; i < Nf; ++i)
        freq[static_cast<size_t>(i)] =
            fStart + (files[static_cast<size_t>(i)].fnum - 1) * fStepMHz / 1000.0;

    const double fs = Level0Dat::kFsForced;
    const double pulseWidthUs = fm0.tauN / fs * 1e6;
    const double B = hdr0.bwMHz * 1e6;
    const double tau = fm0.tauN / fs;
    const double dr = Level0Dat::kC / (2.0 * fs);
    const double t0 = fm0.dlyN / fs;
    const int N = static_cast<int>(fm0.N);

    std::vector<double> R(static_cast<size_t>(N));
    for (int g = 0; g < N; ++g)
        R[static_cast<size_t>(g)] = Level0Dat::kC * t0 / 2.0 + g * dr;

    const double resGate = (Level0Dat::kC / (2.0 * B)) / dr;
    const int rangeGateHalf = imax(2, static_cast<int>(std::lround(1.3 * resGate)));
    const int bgExcl = imax(50, static_cast<int>(std::lround(30.0 * resGate)));
    const int bgGuard = 4;
    const double bgPct = 15.0;

    std::vector<std::vector<double> > profF(static_cast<size_t>(N));
    for (int g = 0; g < N; ++g)
        profF[static_cast<size_t>(g)].assign(static_cast<size_t>(Nf), 0.0);

    std::vector<double> tmsMs(static_cast<size_t>(Nf)), azF(static_cast<size_t>(Nf)),
        rollF(static_cast<size_t>(Nf));
    std::vector<FileHeaderInfo> hdrs(static_cast<size_t>(Nf));
    std::vector<FrameMeta> fms(static_cast<size_t>(Nf));

    progress(cb, 5, "逐频点读取与脉压…");
    for (int i = 0; i < Nf; ++i) {
        if (cancelled(cb)) {
            if (err)
                *err = "已取消";
            return false;
        }
        FileHeaderInfo hdr;
        FrameMeta fm;
        ComplexMatrix data;
        std::vector<uint16_t> azList;
        const int pctLo = 5 + (i * 45) / imax(1, Nf);
        const int pctHi = 5 + ((i + 1) * 45) / imax(1, Nf);
        if (!Level0Dat::readFile(
                files[static_cast<size_t>(i)].path, hdr, fm, data, azList, 0, -1,
                [&](int p) {
                    progress(cb, pctLo + (pctHi - pctLo) * p / 100,
                             "频点 " + std::to_string(i + 1) + "/" + std::to_string(Nf));
                },
                &localErr, [&]() { return cancelled(cb); })) {
            if (err)
                *err = localErr;
            return false;
        }
        hdrs[static_cast<size_t>(i)] = hdr;
        fms[static_cast<size_t>(i)] = fm;
        tmsMs[static_cast<size_t>(i)] = headerTimeMs(hdr);
        azF[static_cast<size_t>(i)] = fm.azRaw / 100.0;
        rollF[static_cast<size_t>(i)] = fm.rollRaw / 100.0;

        Preprocess::removeDc(data);
        PulseCompress::apply(data, hdr.bwMHz, pulseWidthUs, fs, nullptr,
                              [&]() { return cancelled(cb); });
        if (cancelled(cb)) {
            if (err)
                *err = "已取消";
            return false;
        }

        std::vector<double> prof;
        Calibration::meanPowerProfile(data, prof);
        for (int g = 0; g < N && g < static_cast<int>(prof.size()); ++g)
            profF[static_cast<size_t>(g)][static_cast<size_t>(i)] = prof[static_cast<size_t>(g)];
    }

    std::vector<double> profPick(static_cast<size_t>(N));
    for (int g = 0; g < N; ++g)
        profPick[static_cast<size_t>(g)] = profF[static_cast<size_t>(g)][0];

    std::vector<double> profileDb(static_cast<size_t>(N));
    for (int g = 0; g < N; ++g)
        profileDb[static_cast<size_t>(g)] =
            10.0 * std::log10(imax(profPick[static_cast<size_t>(g)], 1e-30));

    int gCorner = Calibration::argMax(profPick);
    InteractSelection cornerSel;
    if (req.autoPick || !cb.onCornerConfirm) {
        cornerSel.cornerRangeOverrideM = -1;
    } else {
        if (!cb.onCornerConfirm(R, profileDb, gCorner, cornerSel)) {
            if (err)
                *err = cancelled(cb) ? "已取消" : "未确认角反";
            return false;
        }
    }
    if (cornerSel.cornerRangeOverrideM > 0) {
        double bestD = 1e99;
        for (int g = 0; g < N; ++g) {
            const double d = std::abs(R[static_cast<size_t>(g)] - cornerSel.cornerRangeOverrideM);
            if (d < bestD) {
                bestD = d;
                gCorner = g;
            }
        }
    }
    const int gCorner1 = gCorner + 1;
    const int g1c = imax(1, gCorner1 - rangeGateHalf);
    const int g2c = imin(N, gCorner1 + rangeGateHalf);
    const double Rcorner = R[static_cast<size_t>(gCorner)];

    int targetMode = req.targetMode;
    std::vector<std::pair<int, int> > tgtSeg;
    int gTarget = gCorner;
    double R_tgt = Rcorner;
    std::string rangeSpec;

    InteractSelection tgtSel;
    if (req.autoPick || !cb.onTargetPick) {
        targetMode = req.targetMode;
        if (targetMode == 1) {
            gTarget = Calibration::argMax(profPick);
            tgtSeg.push_back(std::make_pair(
                imax(1, gTarget + 1 - rangeGateHalf), imin(N, gTarget + 1 + rangeGateHalf)));
            R_tgt = R[static_cast<size_t>(gTarget)];
        } else {
            if (err)
                *err = "扩展目标需交互指定距离段";
            return false;
        }
    } else {
        GrayImage preview = profileGrayImage(profileDb);
        if (!cb.onTargetPick(R, preview, N, 1, profileDb, req.targetMode, tgtSel)) {
            if (err)
                *err = "已取消";
            return false;
        }
        targetMode = tgtSel.targetMode;
        if (targetMode == 2) {
            for (size_t q = 0; q < tgtSel.extendedRanges.size(); ++q) {
                const double ra = imin(tgtSel.extendedRanges[q].first, tgtSel.extendedRanges[q].second);
                const double rb = imax(tgtSel.extendedRanges[q].first, tgtSel.extendedRanges[q].second);
                int ga = 0, gb = N - 1;
                for (int g = 0; g < N; ++g) {
                    if (R[static_cast<size_t>(g)] >= ra) {
                        ga = g;
                        break;
                    }
                }
                for (int g = N - 1; g >= 0; --g) {
                    if (R[static_cast<size_t>(g)] <= rb) {
                        gb = g;
                        break;
                    }
                }
                if (gb - ga < 1)
                    continue;
                tgtSeg.push_back(std::make_pair(ga + 1, gb + 1));
            }
            if (tgtSeg.empty()) {
                if (err)
                    *err = "未指定有效的扩展目标距离段";
                return false;
            }
            {
                const int g1 = tgtSeg[0].first;
                const int g2 = tgtSeg[0].second;
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.2f~%.2f", R[static_cast<size_t>(g1 - 1)],
                              R[static_cast<size_t>(g2 - 1)]);
                rangeSpec = buf;
            }
        } else {
            if (tgtSel.targets.empty()) {
                if (err)
                    *err = "未选择目标";
                return false;
            }
            gTarget = ibound(0, tgtSel.targets[0].rg, N - 1);
            tgtSeg.push_back(std::make_pair(
                imax(1, gTarget + 1 - rangeGateHalf), imin(N, gTarget + 1 + rangeGateHalf)));
            R_tgt = R[static_cast<size_t>(gTarget)];
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", R_tgt);
            rangeSpec = buf;
        }
    }
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }

    const int ncSeg = static_cast<int>(tgtSeg.size());
    const double sigmaCal = std::pow(10.0, req.sigmaTheoryDb / 10.0);

    std::vector<double> Pc(static_cast<size_t>(Nf));
    std::vector<std::vector<double> > PtSeg(static_cast<size_t>(Nf),
                                            std::vector<double>(static_cast<size_t>(ncSeg), 0));
    std::vector<std::vector<double> > bgSeg(static_cast<size_t>(Nf),
                                           std::vector<double>(static_cast<size_t>(ncSeg), 0));

    for (int i = 0; i < Nf; ++i) {
        std::vector<double> p(static_cast<size_t>(N));
        for (int g = 0; g < N; ++g)
            p[static_cast<size_t>(g)] = profF[static_cast<size_t>(g)][static_cast<size_t>(i)];
        Pc[static_cast<size_t>(i)] = netPower(p, g1c, g2c, N, bgGuard, bgExcl, bgPct);
        for (int q = 0; q < ncSeg; ++q) {
            double Pq = 0, bgq = 0;
            segNetPower(p, tgtSeg[static_cast<size_t>(q)].first, tgtSeg[static_cast<size_t>(q)].second,
                        N, bgGuard, bgExcl, bgPct, Pq, bgq);
            PtSeg[static_cast<size_t>(i)][static_cast<size_t>(q)] = Pq;
            bgSeg[static_cast<size_t>(i)][static_cast<size_t>(q)] = bgq;
        }
    }

    std::vector<double> Pt(static_cast<size_t>(Nf));
    for (int i = 0; i < Nf; ++i)
        Pt[static_cast<size_t>(i)] = PtSeg[static_cast<size_t>(i)][0];

    std::vector<bool> badP(static_cast<size_t>(Nf), false);
    for (int i = 0; i < Nf; ++i) {
        if (Pc[static_cast<size_t>(i)] <= 0)
            badP[static_cast<size_t>(i)] = true;
        for (int q = 0; q < ncSeg; ++q) {
            if (PtSeg[static_cast<size_t>(i)][static_cast<size_t>(q)] <= 0)
                badP[static_cast<size_t>(i)] = true;
        }
    }

    std::vector<bool> badOut(static_cast<size_t>(Nf), false);
    std::vector<bool> ok0(static_cast<size_t>(Nf));
    for (int i = 0; i < Nf; ++i)
        ok0[static_cast<size_t>(i)] = !badP[static_cast<size_t>(i)];

    int okCount = 0;
    for (int i = 0; i < Nf; ++i)
        if (ok0[static_cast<size_t>(i)])
            ++okCount;

    if (okCount >= 4) {
        for (int pass = 0; pass < 2; ++pass) {
            const double mPc = medianOf(Pc, ok0);
            const double sPc = madScale(Pc, ok0, mPc);
            std::vector<bool> oAll(static_cast<size_t>(Nf), false);
            for (int i = 0; i < Nf; ++i) {
                if (sPc > 0 && std::abs(Pc[static_cast<size_t>(i)] - mPc) > 3.0 * sPc)
                    oAll[static_cast<size_t>(i)] = true;
            }
            for (int q = 0; q < ncSeg; ++q) {
                std::vector<double> col(static_cast<size_t>(Nf));
                for (int i = 0; i < Nf; ++i)
                    col[static_cast<size_t>(i)] = PtSeg[static_cast<size_t>(i)][static_cast<size_t>(q)];
                const double mPt = medianOf(col, ok0);
                const double sPt = madScale(col, ok0, mPt);
                if (sPt > 0) {
                    for (int i = 0; i < Nf; ++i) {
                        if (std::abs(col[static_cast<size_t>(i)] - mPt) > 3.0 * sPt)
                            oAll[static_cast<size_t>(i)] = true;
                    }
                }
            }
            for (int i = 0; i < Nf; ++i) {
                if (ok0[static_cast<size_t>(i)] && oAll[static_cast<size_t>(i)])
                    badOut[static_cast<size_t>(i)] = true;
                if (ok0[static_cast<size_t>(i)] && oAll[static_cast<size_t>(i)])
                    ok0[static_cast<size_t>(i)] = false;
            }
        }
    }

    std::vector<double> sig(static_cast<size_t>(Nf), 0);
    std::vector<double> sigDb(static_cast<size_t>(Nf), 0);

    if (targetMode == 1) {
        const double r4 = std::pow(R_tgt / Rcorner, 4.0);
        for (int i = 0; i < Nf; ++i) {
            sig[static_cast<size_t>(i)] =
                sigmaCal * (Pt[static_cast<size_t>(i)] / imax(Pc[static_cast<size_t>(i)], 1e-30)) * r4;
            sigDb[static_cast<size_t>(i)] =
                10.0 * std::log10(imax(sig[static_cast<size_t>(i)], 1e-30));
        }
        if (rangeSpec.empty()) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", R_tgt);
            rangeSpec = buf;
        }
    } else {
        std::vector<double> bgPerGate(static_cast<size_t>(ncSeg));
        for (int q = 0; q < ncSeg; ++q) {
            std::vector<double> col(static_cast<size_t>(Nf));
            for (int i = 0; i < Nf; ++i)
                col[static_cast<size_t>(i)] = bgSeg[static_cast<size_t>(i)][static_cast<size_t>(q)];
            bgPerGate[static_cast<size_t>(q)] = medianOf(col, std::vector<bool>(Nf, true));
        }
        for (int i = 0; i < Nf; ++i) {
            std::vector<double> p(static_cast<size_t>(N));
            for (int g = 0; g < N; ++g)
                p[static_cast<size_t>(g)] = profF[static_cast<size_t>(g)][static_cast<size_t>(i)];
            double acc = 0;
            for (int q = 0; q < ncSeg; ++q) {
                const int g1 = tgtSeg[static_cast<size_t>(q)].first;
                const int g2 = tgtSeg[static_cast<size_t>(q)].second;
                for (int g = g1; g <= g2; ++g) {
                    const double v =
                        imax(p[static_cast<size_t>(g - 1)] - bgPerGate[static_cast<size_t>(q)], 0.0);
                    acc += v * (sigmaCal / imax(Pc[static_cast<size_t>(i)], 1e-30))
                             * std::pow(R[static_cast<size_t>(g - 1)] / Rcorner, 4.0);
                }
            }
            sig[static_cast<size_t>(i)] = acc;
            sigDb[static_cast<size_t>(i)] = 10.0 * std::log10(imax(acc, 1e-30));
        }
        const int g1 = tgtSeg[0].first;
        const int g2 = tgtSeg[0].second;
        double wSum = 0, rNum = 0;
        for (int g = g1; g <= g2; ++g) {
            double w = 0;
            for (int i = 0; i < Nf; ++i)
                w += imax(profF[static_cast<size_t>(g - 1)][static_cast<size_t>(i)], 0.0);
            wSum += w;
            rNum += w * R[static_cast<size_t>(g - 1)];
        }
        if (wSum > 0)
            R_tgt = rNum / wSum;
    }

    for (int i = 0; i < Nf; ++i) {
        if (badP[static_cast<size_t>(i)] || badOut[static_cast<size_t>(i)])
            sigDb[static_cast<size_t>(i)] = std::numeric_limits<double>::quiet_NaN();
    }

    std::string measureTime;
    std::string acqDate;
    if (hdr0.year >= 2000 && hdr0.year <= 2100) {
        char compact[32], pretty[64];
        std::snprintf(compact, sizeof(compact), "%04d%02d%02d%02d%02d%02d", hdr0.year, hdr0.month,
                      hdr0.day, hdr0.hour, hdr0.minute, hdr0.second);
        std::snprintf(pretty, sizeof(pretty), "%04d-%02d-%02d %02d:%02d:%02d", hdr0.year, hdr0.month,
                      hdr0.day, hdr0.hour, hdr0.minute, hdr0.second);
        measureTime = compact;
        acqDate = pretty;
    } else {
        measureTime = nowCompact();
        acqDate = nowPretty();
    }

    resp.freqGHz = freq;
    resp.rcsDbsm = sigDb;
    resp.timeSpanTag = measureTime;
    resp.l1Paths.clear();
    resp.l2Paths.clear();

    progress(cb, 55, "写出 1 级…");
    for (int i = 0; i < Nf; ++i) {
        if (cancelled(cb)) {
            if (err)
                *err = "已取消";
            return false;
        }
        char tag[32];
        std::snprintf(tag, sizeof(tag), "_L1-f%d.dat", files[static_cast<size_t>(i)].fnum);
        const std::string l1 = pathJoin(outDir, req.tgtName + measureTime + tag);
        if (!copyFileBytes(files[static_cast<size_t>(i)].path, l1)) {
            if (err)
                *err = "无法写出: " + l1;
            return false;
        }
        resp.l1Paths.push_back(l1);
    }

    progress(cb, 75, "写出 2 级 pdat…");
    for (int i = 0; i < Nf; ++i) {
        if (cancelled(cb)) {
            if (err)
                *err = "已取消";
            return false;
        }
        std::vector<double> p(static_cast<size_t>(N));
        for (int g = 0; g < N; ++g)
            p[static_cast<size_t>(g)] = profF[static_cast<size_t>(g)][static_cast<size_t>(i)];
        const double Pc_i = Pc[static_cast<size_t>(i)];
        std::vector<double> rcsProf;
        Calibration::toRcsProfile(p, R, sigmaCal, imax(Pc_i, 1e-30), Rcorner, rcsProf);
        std::vector<double> phaseDeg(static_cast<size_t>(N), 0.0);
        char tag[32];
        std::snprintf(tag, sizeof(tag), "_L2-f%d.pdat", files[static_cast<size_t>(i)].fnum);
        const std::string l2 = pathJoin(outDir, req.tgtName + measureTime + tag);
        if (!Level2Pdat::save1D(l2, rcsProf, phaseDeg, Rcorner, req.sigmaTheoryDb,
                                hdrs[static_cast<size_t>(i)], fms[static_cast<size_t>(i)], 100,
                                &localErr)) {
            if (err)
                *err = localErr;
            return false;
        }
        resp.l2Paths.push_back(l2);
    }

    Level3Info info;
    info.devName = hdr0.devName;
    info.sysMode = "模式" + std::to_string(hdr0.workMode) + "/波形" + std::to_string(hdr0.waveform)
                   + "/波段" + std::to_string(hdr0.bandCode);
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.4f", pulseWidthUs);
        info.pulseWidth = buf;
        std::snprintf(buf, sizeof(buf), "%.3f", fs / fm0.PRT);
        info.prf = buf;
        std::snprintf(buf, sizeof(buf), "%.4f", fStepMHz / 1000.0);
        info.fStep = buf;
        std::snprintf(buf, sizeof(buf), "%.4f", freq.front());
        info.fStart = buf;
        std::snprintf(buf, sizeof(buf), "%.4f", freq.back());
        info.fStop = buf;
        std::snprintf(buf, sizeof(buf), "%.0f", fs);
        info.fs = buf;
    }
    info.pol = pol;
    info.tgtName = req.tgtName;
    info.measureTime = acqDate;
    info.time = acqDate;
    info.dataType = "点频单站RCS";
    if (!azF.empty()) {
        double azMin = azF[0], azMax = azF[0];
        for (size_t k = 0; k < azF.size(); ++k) {
            azMin = imin(azMin, azF[k]);
            azMax = imax(azMax, azF[k]);
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", azMin);
        info.azStart = buf;
        std::snprintf(buf, sizeof(buf), "%.2f", azMax);
        info.azStop = buf;
    }

    const std::string l3 = pathJoin(outDir, req.tgtName + measureTime + ".rcs");
    progress(cb, 90, "写出 3 级 .rcs…");
    if (!Level3Text::saveCwRcs(l3, freq, sigDb, tmsMs, azF, rollF, rangeSpec, info, &localErr)) {
        if (err)
            *err = localErr;
        return false;
    }
    resp.l3Path = l3;

    progress(cb, 100, "点频 RCS 完成");
    message(cb, "已保存: " + l3);
    return true;
}
