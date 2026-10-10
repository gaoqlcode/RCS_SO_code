#include "Level3Text.h"
#include "OutPath.h"
#include "Util.hpp"
#include "rcs/rcs_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <queue>
#include <sstream>
#include <vector>

namespace {

const double kC = 299792458.0;
const double kPi = 3.14159265358979323846;

bool cancelled(Callbacks &cb)
{
    return cb.isCancelled && cb.isCancelled();
}

void progress(Callbacks &cb, int p, const std::string &m)
{
    if (cb.onProgress)
        cb.onProgress(p, m);
}

void message(Callbacks &cb, const std::string &m)
{
    if (cb.onMessage)
        cb.onMessage(m);
}

double beamWidthDeg(double fcGHz)
{
    if (fcGHz >= 92.0 && fcGHz <= 96.0)
        return 7.0;
    return 10.0; // X / Ku / Ka
}

bool loadRawU16LE(const std::string &path, int nr, int na, std::vector<uint16_t> &out, std::string *err)
{
    if (nr <= 0 || na <= 0) {
        if (err)
            *err = "nr/na 无效";
        return false;
    }
    const int64_t need = static_cast<int64_t>(nr) * na * 2;
    const int64_t sz = fileSizeBytes(path);
    if (sz < need) {
        if (err)
            *err = "RAW 文件太小或路径不对";
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!readFileAll(path, bytes) || static_cast<int64_t>(bytes.size()) < need) {
        if (err)
            *err = "无法读取 RAW: " + path;
        return false;
    }
    // 磁盘为行主序 [Nr][Na]：与 MATLAB fread([Na,Nr],'*uint16')' 后内存布局一致
    // （fread 按列填满 Na×Nr，再转置 → 连续存放各距离行的方位采样）
    out.resize(static_cast<size_t>(nr) * static_cast<size_t>(na));
    for (size_t i = 0; i < out.size(); ++i) {
        const size_t off = i * 2;
        out[i] = static_cast<uint16_t>(bytes[off] | (bytes[off + 1] << 8));
    }
    return true;
}

// 预览色阶：按百分位拉伸（类似 MATLAB imshow([],[]) + 白场），
// 避免按峰值归一导致角反很亮、背景几乎全黑、无法框选。
GrayImage makePreview(const std::vector<uint16_t> &img, int nr, int na, int step)
{
    GrayImage g;
    if (step < 1)
        step = 8;
    g.height = (nr + step - 1) / step;
    g.width = (na + step - 1) / step;
    const size_t nPix = static_cast<size_t>(g.width) * static_cast<size_t>(g.height);
    g.pixels.assign(nPix, 0);
    if (nPix == 0 || img.empty() || nr <= 0 || na <= 0)
        return g;

    // 块均值降采样（近似 MATLAB imresize），比抽点更能压斑点、保住地物轮廓
    std::vector<double> samples(nPix, 0.0);
    for (int y = 0; y < g.height; ++y) {
        const int r0 = y * step;
        const int r1 = std::min(r0 + step, nr);
        for (int x = 0; x < g.width; ++x) {
            const int a0 = x * step;
            const int a1 = std::min(a0 + step, na);
            double s = 0.0;
            int n = 0;
            for (int r = r0; r < r1; ++r) {
                for (int a = a0; a < a1; ++a) {
                    s += img[static_cast<size_t>(r) * static_cast<size_t>(na) + a];
                    ++n;
                }
            }
            samples[static_cast<size_t>(y) * static_cast<size_t>(g.width) + x] =
                n > 0 ? s / n : 0.0;
        }
    }

    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    auto pct = [&](double p) -> double {
        const size_t i =
            static_cast<size_t>(ibound(0.0, p / 100.0 * (sorted.size() - 1),
                                       static_cast<double>(sorted.size() - 1)));
        return sorted[i];
    };
    // PS 自动色阶：两端各裁约 0.5%，拉开地物（角反可过曝）
    double lo = pct(0.5);
    double hi = pct(99.5);
    if (hi <= lo + 1.0) {
        lo = 0.0;
        hi = sorted.back() > 0.0 ? sorted.back() : 1.0;
    }
    if (hi <= lo + 1.0)
        hi = lo + 1.0;

    const double inv = 1.0 / (hi - lo);
    for (size_t i = 0; i < nPix; ++i) {
        double t = (samples[i] - lo) * inv;
        t = ibound(0.0, t, 1.0);
        g.pixels[i] = static_cast<uint8_t>(ibound(0, static_cast<int>(t * 255.0 + 0.5), 255));
    }
    return g;
}

bool clampRoi(RectRoi &roi, int nr, int na)
{
    if (!roi.valid())
        return false;
    if (roi.x < 0)
        roi.x = 0;
    if (roi.y < 0)
        roi.y = 0;
    if (roi.x + roi.w > na)
        roi.w = na - roi.x;
    if (roi.y + roi.h > nr)
        roi.h = nr - roi.y;
    return roi.w > 0 && roi.h > 0;
}

double meanPowerRoi(const std::vector<uint16_t> &img, int nr, int na, const RectRoi &roi)
{
    double s = 0;
    int n = 0;
    for (int y = roi.y; y < roi.y + roi.h; ++y) {
        for (int x = roi.x; x < roi.x + roi.w; ++x) {
            const double dn = img[static_cast<size_t>(y) * na + x];
            s += dn * dn;
            ++n;
        }
    }
    return n > 0 ? s / n : 0.0;
}

double histNoisePower(const std::vector<uint16_t> &img)
{
    // 简化：取全局最小 1% 分位的 DN，噪声功率 ≈ 2*mode^2 的替代
    std::vector<uint16_t> v = img;
    if (v.empty())
        return 0;
    const size_t k = v.size() / 100;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    const double dn = v[k];
    return 2.0 * dn * dn;
}

// 角反缓冲区内：峰值 -6dB（幅度）连通域能量与背景
bool calEpsilon(const std::vector<uint16_t> &img, int nr, int na, const RectRoi &buf, double &eps,
                int &peakRow, int &peakCol, std::string *err)
{
    RectRoi r = buf;
    if (!clampRoi(r, nr, na)) {
        if (err)
            *err = "角反缓冲区无效";
        return false;
    }
    uint16_t maxDN = 0;
    int pr = r.y, pc = r.x;
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            const uint16_t v = img[static_cast<size_t>(y) * na + x];
            if (v > maxDN) {
                maxDN = v;
                pr = y;
                pc = x;
            }
        }
    }
    peakRow = pr;
    peakCol = pc;
    const double thr = maxDN * std::pow(10.0, -6.0 / 20.0);
    const int h = r.h, w = r.w;
    std::vector<char> mask(static_cast<size_t>(h) * w, 0);
    std::vector<char> vis(static_cast<size_t>(h) * w, 0);
    auto idx = [w](int yy, int xx) { return yy * w + xx; };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double dn = img[static_cast<size_t>(r.y + y) * na + (r.x + x)];
            if (dn >= thr)
                mask[idx(y, x)] = 1;
        }
    const int sy = pr - r.y, sx = pc - r.x;
    std::queue<std::pair<int, int> > q;
    if (sy >= 0 && sy < h && sx >= 0 && sx < w && mask[idx(sy, sx)]) {
        q.push(std::make_pair(sy, sx));
        vis[idx(sy, sx)] = 1;
    } else {
        // 兜底：凡过阈的都算信号
        for (size_t i = 0; i < mask.size(); ++i)
            vis[i] = mask[i];
    }
    const int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    while (!q.empty()) {
        const int cy = q.front().first, cx = q.front().second;
        q.pop();
        for (int k = 0; k < 8; ++k) {
            const int ny = cy + dy[k], nx = cx + dx[k];
            if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                continue;
            if (!mask[idx(ny, nx)] || vis[idx(ny, nx)])
                continue;
            vis[idx(ny, nx)] = 1;
            q.push(std::make_pair(ny, nx));
        }
    }
    double sumPA = 0, sumPB = 0;
    int NA = 0, NB = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double dn = img[static_cast<size_t>(r.y + y) * na + (r.x + x)];
            const double e = dn * dn;
            if (vis[idx(y, x)]) {
                sumPA += e;
                ++NA;
            } else {
                sumPB += e;
                ++NB;
            }
        }
    }
    if (NA <= 0 || NB <= 0) {
        if (err)
            *err = "角反连通域或背景为空，请重选缓冲区";
        return false;
    }
    eps = sumPA - (static_cast<double>(NA) / NB) * sumPB;
    if (eps <= 0) {
        if (err)
            *err = "角反扣背景后能量<=0，请重选缓冲区";
        return false;
    }
    return true;
}

bool parseMeasureTime(const std::string &s, int &y, int &mo, int &d, int &h, int &mi, double &sec)
{
    // yyyy-MM-dd HH:mm:ss.SSS
    if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%lf", &y, &mo, &d, &h, &mi, &sec) < 6)
        return false;
    return true;
}

std::string csTimeTag(int y, int mo, int d, int h, int mi)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d", y, mo, d, h, mi);
    return buf;
}

double medianOf(std::vector<double> v)
{
    if (v.empty())
        return 0;
    const size_t n = v.size();
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n / 2), v.end());
    if (n % 2)
        return v[n / 2];
    const double a = v[n / 2];
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n / 2 - 1), v.end());
    return 0.5 * (a + v[n / 2 - 1]);
}

} // namespace

bool processSigma0(const Sigma0Request &req, Sigma0Response &resp, Callbacks &cb, std::string *err)
{
    resp = Sigma0Response();
    std::string localErr;
    if (req.rawPath.empty() || req.nr <= 0 || req.na <= 0) {
        localErr = "请指定 RAW 路径和 nr/na";
        if (err)
            *err = localErr;
        return false;
    }
    if (cancelled(cb)) {
        if (err)
            *err = "已取消";
        return false;
    }

    progress(cb, 5, "读取 RAW…");
    std::vector<uint16_t> img;
    if (!loadRawU16LE(req.rawPath, req.nr, req.na, img, &localErr)) {
        if (err)
            *err = localErr;
        return false;
    }
    const int nr = req.nr, na = req.na;
    resp.preview = makePreview(img, nr, na, 8);

    const double bw = beamWidthDeg(req.fcGHz);
    const double prf = req.kv * req.M * req.vs * req.T;
    const double rbin = kC / (2.0 * req.fsHz);
    const double deltaB = rbin;
    const double deltaA = req.vs * req.M * req.azDecim / prf;

    double rNear = 0;
    if (req.anchorMode == 1) {
        const double rCenter = req.hFlight / std::cos(req.incAngleDeg * kPi / 180.0);
        rNear = rCenter - nr * rbin / 2.0;
    } else if (req.anchorMode == 2) {
        if (req.rCalKnown <= 0 || req.rowCrHint <= 0) {
            if (err)
                *err = "crKnown 锚定需要 rCalKnown 与 rowCrHint";
            return false;
        }
        rNear = req.rCalKnown - (nr - req.rowCrHint) * rbin;
    } else {
        const double thetaLast = req.incAngleDeg - bw / 2.0;
        rNear = req.hFlight / std::cos(thetaLast * kPi / 180.0);
    }

    std::vector<double> rAxis(static_cast<size_t>(nr));
    std::vector<double> thetaRow(static_cast<size_t>(nr));
    for (int r = 0; r < nr; ++r) {
        rAxis[static_cast<size_t>(r)] = rNear + (nr - (r + 1)) * rbin;
        const double rr = rAxis[static_cast<size_t>(r)];
        double c = req.hFlight / rr;
        c = clampv(c, -1.0, 1.0);
        thetaRow[static_cast<size_t>(r)] = std::acos(c) * 180.0 / kPi;
    }

    // ----- ROI -----
    RectRoi noiseRoi = req.noiseRoi;
    RectRoi calRoi = req.calBufRoi;
    RectRoi bgRoi = req.bgRoi;
    std::vector<RectRoi> targets = req.targetRois;

    if (!req.autoPick) {
        progress(cb, 15, "交互选框…");
        if (cb.onRectRoi) {
            RectRoi tmp;
            if (cb.onRectRoi(resp.preview, nr, na, "noise", tmp) && tmp.valid())
                noiseRoi = tmp;
            if (!cb.onRectRoi(resp.preview, nr, na, "cal", calRoi) || !calRoi.valid()) {
                if (err)
                    *err = "需要角反缓冲区";
                return false;
            }
            if (req.targetType == 1 && req.bgMode == 2) {
                cb.onRectRoi(resp.preview, nr, na, "bg", bgRoi);
            }
        } else if (!calRoi.valid()) {
            if (err)
                *err = "未提供角反缓冲区，且无 onRectRoi 回调";
            return false;
        }
        if (cb.onTargetRois) {
            std::vector<RectRoi> outs;
            if (!cb.onTargetRois(resp.preview, nr, na, outs) || outs.empty()) {
                if (err)
                    *err = "未选择地物目标";
                return false;
            }
            targets = outs;
        } else if (targets.empty()) {
            if (err)
                *err = "未提供目标 ROI";
            return false;
        }
    } else {
        if (!calRoi.valid() || targets.empty()) {
            if (err)
                *err = "autoPick 需要 calBufRoi 与至少一个 targetRois";
            return false;
        }
    }

    if (!clampRoi(calRoi, nr, na)) {
        if (err)
            *err = "角反缓冲区越界";
        return false;
    }
    for (size_t i = 0; i < targets.size(); ++i) {
        if (!clampRoi(targets[i], nr, na)) {
            if (err)
                *err = "目标 ROI 无效";
            return false;
        }
    }

    double pNoise = 0;
    if (noiseRoi.valid() && clampRoi(noiseRoi, nr, na))
        pNoise = meanPowerRoi(img, nr, na, noiseRoi);
    else
        pNoise = histNoisePower(img);
    resp.pNoise = pNoise;
    message(cb, "P_noise ≈ " + std::to_string(pNoise));

    progress(cb, 40, "角反定标…");
    double epsCal = 0;
    int yc = 0, xc = 0;
    if (!calEpsilon(img, nr, na, calRoi, epsCal, yc, xc, &localErr)) {
        if (err)
            *err = localErr;
        return false;
    }
    const double sigmaCalLin = std::pow(10.0, req.sigmaTheoryDb / 10.0); // 物理正确：dBsm→线性
    const double rCal = rAxis[static_cast<size_t>(yc)];
    const double K = (epsCal * std::pow(rCal, 4.0)) / sigmaCalLin;
    resp.K = K;
    {
        std::ostringstream os;
        os << "K=" << K << "  R_cal=" << rCal << " m @ row " << yc;
        message(cb, os.str());
    }

    double bgMeanPower = 0;
    bool haveBg = false;
    if (req.targetType == 1 && req.bgMode == 2 && bgRoi.valid() && clampRoi(bgRoi, nr, na)) {
        bgMeanPower = meanPowerRoi(img, nr, na, bgRoi);
        haveBg = bgMeanPower > 0;
    }

    int yM = 0, mo = 0, d = 0, hh = 0, mi = 0;
    double sec = 0;
    if (!parseMeasureTime(req.measureTime, yM, mo, d, hh, mi, sec)) {
        if (err)
            *err = "measureTime 格式应为 yyyy-MM-dd HH:mm:ss.SSS";
        return false;
    }
    const double timeMs = std::floor((hh * 3600 + mi * 60 + sec) * 1000.0 + 0.5);

    progress(cb, 55, "计算各目标 σ⁰…");
    std::vector<double> csTms, csSig;
    for (size_t ti = 0; ti < targets.size(); ++ti) {
        if (cancelled(cb)) {
            if (err)
                *err = "已取消";
            return false;
        }
        const RectRoi &t = targets[ti];
        const int x1 = t.x, y1 = t.y, wT = t.w, hT = t.h;
        const int NT = wT * hT;
        double sumPT = 0;
        for (int y = y1; y < y1 + hT; ++y)
            for (int x = x1; x < x1 + wT; ++x) {
                const double dn = img[static_cast<size_t>(y) * na + x];
                sumPT += dn * dn;
            }

        const int rowC = y1 + hT / 2;
        const double rTar = rAxis[static_cast<size_t>(ibound(0, rowC, nr - 1))];
        const double sinTh = std::sqrt(std::max(0.0, rTar * rTar - req.hFlight * req.hFlight)) / rTar;
        const double aPix = (deltaA * deltaB) / std::max(sinTh, 1e-12);
        const double areaT = NT * aPix;

        double sumBr = 0, sumNs = 0;
        double cEst = 0;
        bool useC = false;
        double neszDb = 0;

        if (req.targetType == 1) {
            // 点目标
            if (req.bgMode == 1) {
                const int m = req.bgRingMargin;
                const int y1r = std::max(0, y1 - m), y2r = std::min(nr - 1, y1 + hT - 1 + m);
                const int x1r = std::max(0, x1 - m), x2r = std::min(na - 1, x1 + wT - 1 + m);
                double s = 0;
                int nb = 0;
                for (int y = y1r; y <= y2r; ++y) {
                    for (int x = x1r; x <= x2r; ++x) {
                        const bool inT = (y >= y1 && y < y1 + hT && x >= x1 && x < x1 + wT);
                        if (inT)
                            continue;
                        const double dn = img[static_cast<size_t>(y) * na + x];
                        s += dn * dn;
                        ++nb;
                    }
                }
                if (nb >= 100) {
                    cEst = s / nb;
                    useC = true;
                }
            } else if (req.bgMode == 2 && haveBg) {
                cEst = bgMeanPower;
                useC = true;
            }
            if (useC) {
                sumBr = NT * cEst;
            } else if (pNoise > 0) {
                sumNs = NT * pNoise;
            }
            if (pNoise > 0)
                neszDb = 10.0 * std::log10((pNoise * std::pow(rTar, 4.0)) / K);
        } else {
            if (pNoise > 0) {
                sumNs = NT * pNoise;
                neszDb = 10.0 * std::log10((pNoise * std::pow(rTar, 4.0)) / (K * aPix));
            }
        }

        const double sumCorr = std::max(sumPT - sumBr - sumNs, 0.0);
        const double sigmaTar = (sumCorr * std::pow(rTar, 4.0)) / K;

        // 逐像素（行几何）做均值/中值
        std::vector<double> sig0px;
        sig0px.reserve(static_cast<size_t>(NT));
        const double sub = (sumNs + sumBr) / std::max(NT, 1);
        for (int y = y1; y < y1 + hT; ++y) {
            const double rr = rAxis[static_cast<size_t>(y)];
            const double sinR = std::sqrt(std::max(0.0, rr * rr - req.hFlight * req.hFlight)) / rr;
            const double ap = (deltaA * deltaB) / std::max(sinR, 1e-12);
            for (int x = x1; x < x1 + wT; ++x) {
                const double dn = img[static_cast<size_t>(y) * na + x];
                const double pnet = dn * dn - sub;
                sig0px.push_back(pnet * std::pow(rr, 4.0) / (K * ap));
            }
        }
        double meanLin = 0;
        for (size_t i = 0; i < sig0px.size(); ++i)
            meanLin += sig0px[i];
        meanLin /= std::max<size_t>(1, sig0px.size());
        const double medLin = medianOf(sig0px);

        Sigma0TargetItem item;
        item.roi = t;
        item.nPix = NT;
        item.areaM2 = areaT;
        item.neszDb = neszDb;
        if (!std::isfinite(meanLin) || meanLin <= 0) {
            item.sigma0Db = -999;
            item.sigma0MedianDb = -999;
            item.sigmaTarDbsm = -999;
            item.snrDb = -1e9;
        } else {
            item.sigma0Db = 10.0 * std::log10(std::max(meanLin, 1e-300));
            item.sigma0MedianDb = 10.0 * std::log10(std::max(medLin, 1e-300));
            item.sigmaTarDbsm = 10.0 * std::log10(std::max(sigmaTar, 1e-300));
            item.snrDb = item.sigma0Db - neszDb;
        }
        resp.targets.push_back(item);
        csTms.push_back(timeMs);
        csSig.push_back(item.sigma0Db);

        std::ostringstream os;
        os << "目标" << (ti + 1) << ": σ⁰=" << item.sigma0Db << " dB, NESZ=" << neszDb << " dB";
        message(cb, os.str());
    }

    progress(cb, 85, "写 .cs…");
    std::string baseDir = req.rawPath;
    {
        const size_t p1 = baseDir.find_last_of('/');
        const size_t p2 = baseDir.find_last_of('\\');
        size_t p = std::string::npos;
        if (p1 == std::string::npos)
            p = p2;
        else if (p2 == std::string::npos)
            p = p1;
        else
            p = (p1 > p2) ? p1 : p2;
        baseDir = (p == std::string::npos) ? std::string(".") : baseDir.substr(0, p);
    }
    const std::string writeDir =
        trimStr(req.outDir).empty() ? defaultResultDir(baseDir, sigma0ResultTag())
                                    : trimStr(req.outDir);
    makeDirs(writeDir);

    Level3Info info;
    info.tgtName = req.tgtName.empty() ? "目标" : req.tgtName;
    info.measureTime = req.measureTime;
    info.time = req.measureTime;
    info.dataType = "后向散射系数";
    const std::string csName = info.tgtName + csTimeTag(yM, mo, d, hh, mi) + ".cs";
    resp.csPath = pathJoin(writeDir, csName);
    if (!Level3Text::saveCs(resp.csPath, csTms, csSig, info, &localErr)) {
        if (err)
            *err = localErr;
        return false;
    }
    progress(cb, 100, "完成");
    message(cb, "已写出 " + resp.csPath);
    return true;
}
