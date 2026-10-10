#include "BinaryIo.h"
#include "FftBackend.h"
#include "Level0Dat.h"
#include "Util.hpp"
#include "rcs/rcs_api.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <regex>
#include <sstream>
#include <vector>

namespace {

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

bool matchL0Pol(const std::string &name, const std::string &pol)
{
    const std::string u = toUpperStr(name);
    const std::string needle = std::string("_L0_") + toUpperStr(trimStr(pol)) + ".DAT";
    if (u.size() < needle.size())
        return false;
    return u.compare(u.size() - needle.size(), needle.size(), needle) == 0;
}

int firstMultiDigit(const std::string &name)
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

bool computeOne(const std::string &path, const std::string &fileName, int fileIndex,
                const L0PreviewRequest &req, L0PulsePreviewItem &item, std::string *err)
{
    item = L0PulsePreviewItem();
    item.fileName = fileName;
    item.fileIndex = fileIndex;

    std::vector<uint8_t> all;
    if (!readFileAll(path, all) || all.size() < 512) {
        if (err)
            *err = "无法读取: " + fileName;
        return false;
    }
    FileHeaderInfo hdr;
    std::vector<uint8_t> hdr512(all.begin(), all.begin() + 512);
    if (!Level0Dat::parseHeader(hdr512, hdr, err))
        return false;

    const int lenFileH = hdr.lenFileH > 0 ? hdr.lenFileH : 512;
    const int lenFrmH = hdr.lenFrmH > 0 ? hdr.lenFrmH : 256;
    if (static_cast<int64_t>(all.size()) < lenFileH + lenFrmH) {
        if (err)
            *err = "文件太短: " + fileName;
        return false;
    }

    const uint8_t *fh0 = all.data() + lenFileH;
    const uint32_t N = readU32LE(fh0 + 16);
    if (N == 0 || N > 10000000u) {
        if (err)
            *err = "帧长度 N 异常: " + fileName;
        return false;
    }

    const int nNeed = std::max(req.nAvg, req.pulseNo);
    const int64_t frameBytes = static_cast<int64_t>(lenFrmH) + static_cast<int64_t>(N) * 4;
    std::vector<std::vector<std::complex<double> > > pulses;
    pulses.reserve(static_cast<size_t>(nNeed));

    int64_t off = lenFileH;
    while (static_cast<int>(pulses.size()) < nNeed
           && off + frameBytes <= static_cast<int64_t>(all.size())) {
        const uint8_t *fh = all.data() + off;
        if (readU32LE(fh + 16) != N)
            break;
        off += lenFrmH;
        const uint8_t *raw = all.data() + off;
        std::vector<std::complex<double> > z(static_cast<size_t>(N));
        for (uint32_t g = 0; g < N; ++g) {
            const uint16_t A = readU16LE(raw + g * 4);
            const uint16_t PH = readU16LE(raw + g * 4 + 2);
            const double phi = PH / 65536.0 * 2.0 * kPi;
            z[g] = std::polar(static_cast<double>(A), phi);
        }
        pulses.push_back(z);
        off += static_cast<int64_t>(N) * 4;
    }
    if (pulses.empty()) {
        if (err)
            *err = "未读到有效帧: " + fileName;
        return false;
    }

    const int pSel = std::min(std::max(1, req.pulseNo), static_cast<int>(pulses.size()));
    item.pulseSel = pSel;
    std::vector<std::complex<double> > z = pulses[static_cast<size_t>(pSel - 1)];
    if (req.removeDC) {
        std::complex<double> mu(0, 0);
        for (size_t i = 0; i < z.size(); ++i)
            mu += z[i];
        mu /= static_cast<double>(z.size());
        for (size_t i = 0; i < z.size(); ++i)
            z[i] -= mu;
    }

    const double fs = (req.fsHz > 0) ? req.fsHz : Level0Dat::kFsForced;
    item.tUs.resize(static_cast<size_t>(N));
    item.amp.resize(static_cast<size_t>(N));
    double ambSum = 0;
    for (uint32_t i = 0; i < N; ++i) {
        item.tUs[i] = static_cast<double>(i) / fs * 1e6;
        item.amp[i] = std::abs(z[i]);
        ambSum += item.amp[i];
    }
    // 中值近似：排序取中
    std::vector<double> ampSort = item.amp;
    std::nth_element(ampSort.begin(), ampSort.begin() + static_cast<std::ptrdiff_t>(N / 2),
                     ampSort.end());
    const double amb = ampSort[N / 2];
    const double thr = std::max(5.0 * amb, 3.0);
    int g1 = 0, g2 = static_cast<int>(N) - 1;
    bool found = false;
    for (uint32_t i = 0; i < N; ++i) {
        if (item.amp[i] > thr) {
            if (!found) {
                g1 = static_cast<int>(i);
                found = true;
            }
            g2 = static_cast<int>(i);
        }
    }
    int gp = 0;
    double ampMax = item.amp[0];
    for (uint32_t i = 1; i < N; ++i) {
        if (item.amp[i] > ampMax) {
            ampMax = item.amp[i];
            gp = static_cast<int>(i);
        }
    }
    item.ampPkUs = item.tUs[static_cast<size_t>(gp)];
    item.sig1Us = item.tUs[static_cast<size_t>(g1)];
    item.sig2Us = item.tUs[static_cast<size_t>(g2)];

    // 频域
    std::vector<std::complex<double> > S = z;
    FftBackend::fft(S, false);
    // fftshift
    const size_t n = S.size();
    const size_t half = n / 2;
    std::rotate(S.begin(), S.begin() + static_cast<std::ptrdiff_t>(half), S.end());

    double sMax = 0;
    for (size_t i = 0; i < n; ++i)
        sMax = std::max(sMax, std::abs(S[i]));
    if (sMax < 1e-300)
        sMax = 1e-300;

    item.fMHz.resize(n);
    item.spectrumDb.resize(n);
    int ipk = 0;
    double absPk = 0;
    for (size_t i = 0; i < n; ++i) {
        const double fi = (static_cast<double>(i) - static_cast<double>(n) / 2.0) * (fs / n) / 1e6;
        item.fMHz[i] = fi;
        const double a = std::abs(S[i]);
        item.spectrumDb[i] = 20.0 * std::log10(a / sMax + 1e-30);
        if (a > absPk) {
            absPk = a;
            ipk = static_cast<int>(i);
        }
    }
    item.fPkMHz = item.fMHz[static_cast<size_t>(ipk)];

    double pSum = 0;
    for (size_t i = 0; i < n; ++i)
        pSum += absPk > 0 ? (std::abs(S[i]) * std::abs(S[i])) : 0;
    // 重算功率
    pSum = 0;
    std::vector<double> pw(n);
    for (size_t i = 0; i < n; ++i) {
        pw[i] = std::abs(S[i]) * std::abs(S[i]);
        pSum += pw[i];
    }
    double cum = 0;
    int k1 = 0, k2 = static_cast<int>(n) - 1;
    if (pSum > 0) {
        for (size_t i = 0; i < n; ++i) {
            cum += pw[i] / pSum;
            if (cum >= 0.05) {
                k1 = static_cast<int>(i);
                break;
            }
        }
        cum = 0;
        for (size_t i = 0; i < n; ++i) {
            cum += pw[i] / pSum;
            if (cum >= 0.95) {
                k2 = static_cast<int>(i);
                break;
            }
        }
    }
    item.occBwMHz = item.fMHz[static_cast<size_t>(k2)] - item.fMHz[static_cast<size_t>(k1)];
    (void)ambSum;
    return true;
}

} // namespace

bool previewL0Folder(const L0PreviewRequest &req, L0PreviewResponse &resp, Callbacks &cb,
                     std::string *err)
{
    resp = L0PreviewResponse();
    if (req.dataFolder.empty() || !isDir(req.dataFolder)) {
        if (err)
            *err = "数据文件夹无效";
        return false;
    }
    const std::string pol = toUpperStr(trimStr(req.pol));
    if (pol.empty()) {
        if (err)
            *err = "请指定极化";
        return false;
    }

    std::vector<std::string> names = listFileNames(req.dataFolder);
    std::vector<std::string> kept;
    for (size_t i = 0; i < names.size(); ++i) {
        if (matchL0Pol(names[i], pol))
            kept.push_back(names[i]);
    }
    if (kept.empty()) {
        if (err)
            *err = "未找到 *_L0_" + pol + ".dat";
        return false;
    }
    std::sort(kept.begin(), kept.end(), [](const std::string &a, const std::string &b) {
        return firstMultiDigit(a) < firstMultiDigit(b);
    });
    if (req.maxFiles > 0 && static_cast<int>(kept.size()) > req.maxFiles)
        kept.resize(static_cast<size_t>(req.maxFiles));

    {
        std::ostringstream os;
        os << "匹配到 " << kept.size() << " 个 *_L0_" << pol << ".dat";
        message(cb, os.str());
    }

    for (size_t i = 0; i < kept.size(); ++i) {
        if (cancelled(cb)) {
            if (err)
                *err = "已取消";
            return false;
        }
        progress(cb, static_cast<int>(5 + 90.0 * i / std::max<size_t>(1, kept.size())),
                 "计算 " + kept[i]);
        L0PulsePreviewItem item;
        std::string localErr;
        const std::string path = pathJoin(req.dataFolder, kept[i]);
        if (!computeOne(path, kept[i], firstMultiDigit(kept[i]), req, item, &localErr)) {
            message(cb, "跳过 " + kept[i] + ": " + localErr);
            continue;
        }
        {
            std::ostringstream os;
            os << "[" << (i + 1) << "/" << kept.size() << "] " << kept[i]
               << " | 峰值 " << item.ampPkUs << " us | 谱峰 " << item.fPkMHz << " MHz | 90%BW "
               << item.occBwMHz << " MHz";
            message(cb, os.str());
        }
        resp.items.push_back(item);
    }

    if (resp.items.empty()) {
        if (err)
            *err = "没有可用的预览结果";
        return false;
    }
    progress(cb, 100, "预览计算完成");
    return true;
}

bool previewL0File(const std::string &path, int pulseNo, bool removeDC, double fsHz,
                   L0PulsePreviewItem &out, std::string *err)
{
    out = L0PulsePreviewItem();
    L0PreviewRequest req;
    req.pulseNo = pulseNo > 0 ? pulseNo : 1;
    req.removeDC = removeDC;
    req.fsHz = (fsHz > 0) ? fsHz : Level0Dat::kFsForced;
    req.nAvg = 1;
    const std::string name = path;
    size_t p1 = name.find_last_of('/');
    size_t p2 = name.find_last_of('\\');
    size_t p = std::string::npos;
    if (p1 == std::string::npos)
        p = p2;
    else if (p2 == std::string::npos)
        p = p1;
    else
        p = (p1 > p2) ? p1 : p2;
    const std::string base = (p == std::string::npos) ? name : name.substr(p + 1);
    return computeOne(path, base, firstMultiDigit(base), req, out, err);
}
