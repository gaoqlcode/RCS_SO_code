#include "MosaicAlgorithm.h"
#include <tiffio.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "Util.hpp"
#include "OutPath.h"
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>

#ifndef NAN
#define NAN (std::numeric_limits<float>::quiet_NaN())
#endif

static inline int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

static std::string trimCopy(std::string s)
{
    const char *ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static std::string toUpperAscii(std::string s)
{
    for (char &c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}

static std::string toLowerAscii(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

static std::string fileNameOnly(const std::string &path)
{
    return fileNameOf(path);
}

static std::string extensionLower(const std::string &path)
{
    const std::string name = fileNameOf(path);
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= name.size())
        return std::string();
    return toLowerAscii(name.substr(dot + 1));
}


namespace {

int hwThreads()
{
    const unsigned n = std::thread::hardware_concurrency();
    return static_cast<int>(n == 0 ? 2 : std::min(n, 8u));
}

static inline uint16_t readBe16(const uint8_t *p)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

/** 读 16bit 单波段 TIFF → int32（兼容 uint16 振幅图 / int16 RCS_dBsm×100） */
bool readTiff16AsI32(const std::string &path, std::vector<int32_t> &pix, uint32_t &w, uint32_t &h,
                     bool &isSigned, std::string *err)
{
    TIFF *tif = TIFFOpen(path.c_str(), "r");
    if (!tif) {
        if (err) *err = std::string("无法打开 TIFF: ") + path;
        return false;
    }
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    uint16_t bps = 0, spp = 0, sf = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bps);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sf);
    if (bps != 16 || spp != 1) {
        TIFFClose(tif);
        if (err) *err = "仅支持 16bit 单波段灰度 TIFF（uint16 或 int16）";
        return false;
    }
    isSigned = (sf == SAMPLEFORMAT_INT);
    pix.resize(static_cast<size_t>(w) * h);
    std::vector<uint16_t> row(w);
    for (uint32_t r = 0; r < h; ++r) {
        if (TIFFReadScanline(tif, row.data(), r) < 0) {
            TIFFClose(tif);
            if (err) *err = "读扫描行失败";
            return false;
        }
        const size_t base = static_cast<size_t>(r) * w;
        if (isSigned) {
            for (uint32_t c = 0; c < w; ++c)
                pix[base + c] = static_cast<int32_t>(static_cast<int16_t>(row[c]));
        } else {
            for (uint32_t c = 0; c < w; ++c)
                pix[base + c] = static_cast<int32_t>(row[c]);
        }
    }
    TIFFClose(tif);
    return true;
}

bool readTiffU16(const std::string &path, std::vector<uint16_t> &pix, uint32_t &w, uint32_t &h, std::string *err)
{
    bool isSigned = false;
    std::vector<int32_t> tmp;
    if (!readTiff16AsI32(path, tmp, w, h, isSigned, err))
        return false;
    pix.resize(tmp.size());
    for (size_t i = 0; i < tmp.size(); ++i) {
        int32_t v = tmp[i];
        if (v < 0)
            v = 0;
        if (v > 65535)
            v = 65535;
        pix[i] = static_cast<uint16_t>(v);
    }
    return true;
}

/**
 * 加载拼接用显示链图像：定标 int16 先整体平移到非负再送配准/融合
 * （与 MATLAB db2disp 一致；纯值导出另读原图）
 */
bool loadDisplayImgs(const std::vector<std::string> &paths, std::vector<std::vector<uint16_t>> &imgs,
                     std::vector<uint32_t> &widths, std::vector<uint32_t> &heights, double &dbLoOut,
                     std::function<void(int, const std::string &)> progress,
                     std::function<bool()> cancelled, std::string *err)
{
    const int n = static_cast<int>(paths.size());
    imgs.assign(static_cast<size_t>(n), {});
    widths.assign(static_cast<size_t>(n), 0);
    heights.assign(static_cast<size_t>(n), 0);
    std::vector<std::vector<int32_t>> raw(static_cast<size_t>(n));
    bool anySigned = false;
    int32_t mn = 0;
    bool haveMn = false;
    for (int i = 0; i < n; ++i) {
        if (cancelled && cancelled())
            return false;
        if (progress)
            progress(5 + i * 12 / std::max(n, 1),
                     std::string("读取 ") + fileNameOnly(paths[static_cast<size_t>(i)]));
        bool isSigned = false;
        if (!readTiff16AsI32(paths[static_cast<size_t>(i)], raw[static_cast<size_t>(i)],
                             widths[static_cast<size_t>(i)], heights[static_cast<size_t>(i)], isSigned,
                             err))
            return false;
        anySigned = anySigned || isSigned;
        const auto &p = raw[static_cast<size_t>(i)];
        for (size_t k = 0; k < p.size(); k += 64) { // 抽样找黑场，避免扫整幅两次
            const int32_t v = p[k];
            if (!haveMn || v < mn) {
                mn = v;
                haveMn = true;
            }
        }
        for (size_t k = 0; k < p.size(); ++k) {
            if (p[k] < 0) {
                anySigned = true;
                if (p[k] < mn)
                    mn = p[k];
            }
        }
    }
    double dbLo = 0;
    if (anySigned && haveMn) {
        dbLo = static_cast<double>(mn) - 1.0; // MATLAB: 最小值 → 1
        if (progress) {
            std::ostringstream os;
            os << "显示映射: 定标最小值 " << mn << " → 整体 +" << (-dbLo) << "（只影响看图链）";
            progress(18, os.str());
        }
    }
    dbLoOut = dbLo;
    for (int i = 0; i < n; ++i) {
        auto &dst = imgs[static_cast<size_t>(i)];
        const auto &src = raw[static_cast<size_t>(i)];
        dst.resize(src.size());
        for (size_t k = 0; k < src.size(); ++k) {
            double v = static_cast<double>(src[k]) - dbLo;
            if (v < 0)
                v = 0;
            if (v > 65535)
                v = 65535;
            dst[k] = static_cast<uint16_t>(std::llround(v));
        }
        raw[static_cast<size_t>(i)].clear();
        raw[static_cast<size_t>(i)].shrink_to_fit();
    }
    return true;
}

bool writeTiffU16(const std::string &path, uint32_t w, uint32_t h, const uint16_t *pix, std::string *err)
{
    TIFF *tif = TIFFOpen(path.c_str(), "w");
    if (!tif) {
        if (err) *err = std::string("无法创建 TIFF: ") + path;
        return false;
    }
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, w);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, h);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, h);
    for (uint32_t row = 0; row < h; ++row) {
        if (TIFFWriteScanline(tif, const_cast<uint16_t *>(pix + row * w), row) < 0) {
            TIFFClose(tif);
            if (err) *err = "写扫描行失败";
            return false;
        }
    }
    TIFFClose(tif);
    return true;
}

static bool regexFirstGroup(const std::string &text, const std::regex &re, std::string &out)
{
    std::smatch m;
    if (!std::regex_search(text, m, re) || m.size() < 2) return false;
    out = m[1].str();
    return true;
}

static int64_t parseInt64(const std::string &s)
{
    try {
        return std::stoll(s);
    } catch (...) {
        return 0;
    }
}

static int parseInt(const std::string &s)
{
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;
    }
}

/** 按文件名数字排序（MATLAB sortByNumIn） */
void sortByNumIn(std::vector<std::string> &pl)
{
    if (pl.size() < 2) return;
    struct Item {
        std::string path;
        int64_t num;
        int ord;
    };
    std::vector<Item> items;
    items.reserve(pl.size());
    const std::regex reEnd(R"((\d+)\s*$)");
    const std::regex reFile(R"(File\s*(\d+))", std::regex_constants::icase);
    const std::regex reAny(R"((\d+))");
    for (int i = 0; i < static_cast<int>(pl.size()); ++i) {
        const std::string base = fileStem(pl[static_cast<size_t>(i)]);
        int64_t num = i;
        std::string cap;
        if (regexFirstGroup(base, reEnd, cap) || regexFirstGroup(base, reFile, cap)) {
            num = parseInt64(cap);
        } else {
            std::string last;
            for (std::sregex_iterator it(base.begin(), base.end(), reAny), end; it != end; ++it) {
                if ((*it).size() >= 2) last = (*it)[1].str();
            }
            if (!last.empty()) num = parseInt64(last);
        }
        items.push_back({pl[static_cast<size_t>(i)], num, i});
    }
    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        if (a.num != b.num) return a.num < b.num;
        return a.ord < b.ord;
    });
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
        pl[static_cast<size_t>(i)] = items[i].path;
}

void listInputs(const std::string &folder, std::vector<std::string> &tifs, std::vector<std::string> &raws)
{
    tifs.clear();
    raws.clear();
    if (!isDir(folder))
        return;
    std::vector<std::string> names = listFileNames(folder);
    for (size_t i = 0; i < names.size(); ++i) {
        const std::string fp = pathJoin(folder, names[i]);
        if (toUpperAscii(names[i]).compare(0, 6, "MOSAIC") == 0)
            continue;
        const std::string ext = extensionLower(fp);
        if (ext == "tif" || ext == "tiff" || ext == "jpg" || ext == "jpeg")
            tifs.push_back(fp);
        else if (ext == "raw")
            raws.push_back(fp);
    }
    sortByNumIn(tifs);
    sortByNumIn(raws);
}

std::vector<std::string> listTiffs(const std::string &folder)
{
    std::vector<std::string> tifs, raws;
    listInputs(folder, tifs, raws);
    (void)raws;
    return tifs;
}

double nccOfVec(const std::vector<double> &x, const std::vector<double> &y)
{
    if (x.size() != y.size() || x.empty()) return 0;
    double sx = 0, sy = 0, sxy = 0, sx2 = 0, sy2 = 0;
    const int n = static_cast<int>(x.size());
    for (int i = 0; i < n; ++i) {
        sx += x[static_cast<size_t>(i)];
        sy += y[static_cast<size_t>(i)];
        sxy += x[static_cast<size_t>(i)] * y[static_cast<size_t>(i)];
        sx2 += x[static_cast<size_t>(i)] * x[static_cast<size_t>(i)];
        sy2 += y[static_cast<size_t>(i)] * y[static_cast<size_t>(i)];
    }
    const double mx = sx / n, my = sy / n;
    const double num = sxy - n * mx * my;
    const double den = std::sqrt(std::max(0.0, sx2 - n * mx * mx) * std::max(0.0, sy2 - n * my * my));
    return den < 1e-12 ? 0.0 : num / den;
}


double envScore(std::vector<double> p)
{
    const int n = static_cast<int>(p.size());
    if (n < 16) return 0;
    double mean = 0;
    for (double v : p) mean += v;
    mean /= n;
    for (double &v : p) v -= mean;
    double s2 = 0;
    for (double v : p) s2 += v * v;
    const double s = std::sqrt(s2 / n);
    if (s <= 1e-12) return 0;
    for (double &v : p) v /= s;
    const int k = std::max(1, n / 16);
    std::vector<double> sm(static_cast<size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        double sum = 0;
        int cnt = 0;
        for (int j = i - k / 2; j <= i + k / 2; ++j) {
            if (j < 0 || j >= n) continue;
            sum += p[static_cast<size_t>(j)];
            ++cnt;
        }
        sm[static_cast<size_t>(i)] = sum / std::max(1, cnt);
    }
    const double cc = nccOfVec(p, sm);
    std::vector<double> q = p;
    std::sort(q.begin(), q.end());
    const double dyn = q[std::max(0, static_cast<int>(std::llround(0.90 * n)) - 1)]
                       - q[std::max(0, static_cast<int>(std::llround(0.10 * n)) - 1)];
    return std::max(dyn, 0.0) * std::max(cc, 0.0);
}

double envelopeOfRaw(const std::string &path, int64_t L)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    f.seekg(0, std::ios::end);
    const int64_t fileBytes = static_cast<int64_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    const int64_t nb = fileBytes / 2;
    const int M = static_cast<int>(std::min<int64_t>(1200, nb / L));
    if (M < 16) return 0;
    const int step = std::max(1, static_cast<int>(L / 384));
    const int nS = static_cast<int>((L + step - 1) / step);
    std::vector<double> colSum(static_cast<size_t>(nS), 0.0);
    int m = 0;
    std::vector<char> rowBytes(static_cast<size_t>(L * 2));
    for (int k = 0; k < M; ++k) {
        const int64_t pos = static_cast<int64_t>(k) * L * 2;
        f.seekg(pos);
        if (!f) break;
        f.read(rowBytes.data(), static_cast<std::streamsize>(rowBytes.size()));
        if (f.gcount() != static_cast<std::streamsize>(rowBytes.size())) break;
        ++m;
        int si = 0;
        for (int64_t c = 0; c < L; c += step, ++si) {
            const uint8_t *p = reinterpret_cast<const uint8_t *>(rowBytes.data()) + c * 2;
            colSum[static_cast<size_t>(si)] += readBe16(p);
        }
    }
    if (m < 16) return 0;
    for (double &v : colSum) v /= m;
    return envScore(colSum);
}

bool detectRawDims(const std::string &path, int nrHint, int naHint, int nrPrefer,
                   int &Nr, int &Na, std::string &how, std::string *err)
{
    if (nrHint > 0 && naHint > 0) {
        Nr = nrHint;
        Na = naHint;
        how = "配置指定尺寸";
        return true;
    }
    const int64_t bytes = fileSizeBytes(path);
    if (bytes < 4 || (bytes % 2) != 0) {
        if (err) *err = std::string("RAW 大小异常: ") + path;
        return false;
    }
    const int64_t nb = bytes / 2;
    const std::string base = fileStem(path);

    if (nrPrefer > 0 && (nb % nrPrefer) == 0) {
        Nr = nrPrefer;
        Na = static_cast<int>(nb / nrPrefer);
        how = "沿用第1张行长度 " + std::to_string(Nr) + "(本张列 " + std::to_string(Na) + ")";
        return true;
    }

    const std::regex reNr(R"(Nr\s*(\d+))", std::regex_constants::icase);
    const std::regex reNa(R"(Na\s*(\d+))", std::regex_constants::icase);
    std::smatch m1, m2;
    const bool hasNr = std::regex_search(base, m1, reNr) && m1.size() >= 2;
    const bool hasNa = std::regex_search(base, m2, reNa) && m2.size() >= 2;
    if (hasNr && hasNa) {
        Nr = parseInt(m1[1].str());
        Na = parseInt(m2[1].str());
        if (static_cast<int64_t>(Nr) * Na == nb) {
            how = "文件名 Nr/Na";
            return true;
        }
    }
    const std::regex reXx(R"((\d+)\s*[xX\*]\s*(\d+))");
    std::smatch mx;
    if (std::regex_search(base, mx, reXx) && mx.size() >= 3) {
        const int N1 = parseInt(mx[1].str());
        const int N2 = parseInt(mx[2].str());
        if (static_cast<int64_t>(N1) * N2 == nb) {
            Nr = N1;
            Na = N2;
            how = "文件名 长x宽";
            return true;
        }
    }

    std::vector<int64_t> cands;
    for (int e = 8; e <= 17; ++e) {
        const int64_t L = 1LL << e;
        if ((nb % L) == 0 && (nb / L) >= 256)
            cands.push_back(L);
    }
    if (cands.empty()) {
        if (err)
            *err = std::string("无法自动判定 RAW 尺寸: ") + base + "（文件名写 Nr行Na列 或填提示尺寸）";
        return false;
    }
    std::vector<double> ev(cands.size());
    double emax = 0;
    for (size_t k = 0; k < cands.size(); ++k) {
        ev[k] = envelopeOfRaw(path, cands[k]);
        emax = std::max(emax, ev[k]);
    }
    size_t kk = 0;
    for (size_t k = 0; k < cands.size(); ++k) {
        if (ev[k] >= 0.90 * emax) {
            kk = k;
            break;
        }
    }
    Nr = static_cast<int>(cands[kk]);
    Na = static_cast<int>(nb / cands[kk]);
    how = "数据自动判定(行长 " + std::to_string(Nr) + ")";
    return true;
}

bool rawToTiffBe(const std::string &rawPath, int Nr, int Na, const std::string &outTif, std::string *err)
{
    std::ifstream f(rawPath, std::ios::binary);
    if (!f) {
        if (err) *err = std::string("无法打开 RAW: ") + rawPath;
        return false;
    }
    const int64_t need = static_cast<int64_t>(Nr) * Na * 2;
    f.seekg(0, std::ios::end);
    const int64_t fsz = static_cast<int64_t>(f.tellg());
    if (fsz < need) {
        if (err) *err = std::string("RAW 数据不足: ") + rawPath;
        return false;
    }
    f.seekg(0, std::ios::beg);
    std::vector<char> buf(static_cast<size_t>(need));
    f.read(buf.data(), static_cast<std::streamsize>(need));
    if (f.gcount() != static_cast<std::streamsize>(need)) {
        if (err) *err = std::string("RAW 读取不完整: ") + rawPath;
        return false;
    }
    std::vector<uint16_t> pix(static_cast<size_t>(Nr) * static_cast<size_t>(Na));
    const uint8_t *src = reinterpret_cast<const uint8_t *>(buf.data());
    for (int r = 0; r < Nr; ++r) {
        for (int c = 0; c < Na; ++c) {
            const int64_t off = (static_cast<int64_t>(r) * Na + c) * 2;
            pix[static_cast<size_t>(r) * static_cast<size_t>(Na) + static_cast<size_t>(c)] = readBe16(src + off);
        }
    }
    return writeTiffU16(outTif, static_cast<uint32_t>(Na), static_cast<uint32_t>(Nr), pix.data(), err);
}

bool tiffSizeMatches(const std::string &path, int Nr, int Na)
{
    TIFF *tif = TIFFOpen(path.c_str(), "r");
    if (!tif) return false;
    uint32_t w = 0, h = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFClose(tif);
    return static_cast<int>(w) == Na && static_cast<int>(h) == Nr;
}

double percentileSorted(std::vector<double> &v, double p)
{
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    const size_t k = static_cast<size_t>(clampv(std::round(p / 100.0 * static_cast<double>(n)), 0.0, static_cast<double>(n - 1)));
    return v[std::min(k, n - 1)];
}

double percentileSample(const std::vector<uint16_t> &img, double p, int stride = 16)
{
    std::vector<double> v;
    v.reserve(img.size() / static_cast<size_t>(std::max(1, stride)) + 1);
    for (size_t i = 0; i < img.size(); i += static_cast<size_t>(stride)) {
        if (img[i] > 0)
            v.push_back(img[i]);
    }
    if (v.empty()) {
        for (size_t i = 0; i < img.size(); i += static_cast<size_t>(stride))
            v.push_back(img[i]);
    }
    return percentileSorted(v, p);
}

float ramp01(float x)
{
    const float t = clampv(x, 0.f, 1.f);
    return 0.5f - 0.5f * std::cos(3.14159265358979323846f * t);
}

void downsampleU16(const std::vector<uint16_t> &img, uint32_t W, uint32_t H, int step,
                   std::vector<float> &out, int &ow, int &oh)
{
    step = std::max(1, step);
    ow = static_cast<int>((W + step - 1) / step);
    oh = static_cast<int>((H + step - 1) / step);
    out.resize(static_cast<size_t>(ow * oh));
    for (int y = 0; y < oh; ++y) {
        for (int x = 0; x < ow; ++x) {
            const uint32_t xx = std::min(W - 1, static_cast<uint32_t>(x * step));
            const uint32_t yy = std::min(H - 1, static_cast<uint32_t>(y * step));
            out[static_cast<size_t>(y * ow + x)] = static_cast<float>(img[static_cast<size_t>(yy * W + xx)]);
        }
    }
}

void boxMedianFlat(const std::vector<float> &A, int aw, int ah, int w,
                   std::vector<float> &B, int &br, int &bc)
{
    br = std::max(1, ah / w);
    bc = std::max(1, aw / w);
    B.assign(static_cast<size_t>(br * bc), 1.f);
    for (int a = 0; a < br; ++a) {
        const int r0 = a * w;
        const int r1 = std::min(ah, r0 + w);
        for (int b = 0; b < bc; ++b) {
            const int c0 = b * w;
            const int c1 = std::min(aw, c0 + w);
            std::vector<float> blk;
            blk.reserve(static_cast<size_t>((r1 - r0) * (c1 - c0)));
            for (int r = r0; r < r1; ++r)
                for (int c = c0; c < c1; ++c)
                    blk.push_back(A[static_cast<size_t>(r * aw + c)]);
            if (blk.empty()) continue;
            std::nth_element(blk.begin(), blk.begin() + blk.size() / 2, blk.end());
            B[static_cast<size_t>(a * bc + b)] = blk[blk.size() / 2];
        }
    }
}

void smoothGrid3(std::vector<float> &A, int m, int n)
{
    std::vector<float> out(A.size());
    for (int r = 0; r < m; ++r) {
        for (int c = 0; c < n; ++c) {
            double s = 0;
            int cnt = 0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    const int rr = r + dr, cc = c + dc;
                    if (rr < 0 || cc < 0 || rr >= m || cc >= n) continue;
                    s += A[static_cast<size_t>(rr * n + cc)];
                    ++cnt;
                }
            }
            out[static_cast<size_t>(r * n + c)] = static_cast<float>(s / std::max(1, cnt));
        }
    }
    A.swap(out);
}

float sampleBilinear(const std::vector<float> &G, int gw, int gh, float x, float y)
{
    if (gw <= 0 || gh <= 0) return 1.f;
    x = clampv(x, 0.f, static_cast<float>(gw - 1));
    y = clampv(y, 0.f, static_cast<float>(gh - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(gw - 1, x0 + 1);
    const int y1 = std::min(gh - 1, y0 + 1);
    const float fx = x - x0, fy = y - y0;
    const float v00 = G[static_cast<size_t>(y0 * gw + x0)];
    const float v10 = G[static_cast<size_t>(y0 * gw + x1)];
    const float v01 = G[static_cast<size_t>(y1 * gw + x0)];
    const float v11 = G[static_cast<size_t>(y1 * gw + x1)];
    return (1 - fy) * ((1 - fx) * v00 + fx * v10) + fy * ((1 - fx) * v01 + fx * v11);
}

/** 低频盒滤波（与 MATLAB lowBand 一致，cumsum O(N)） */
void lowBand(const std::vector<float> &A, int m, int n, int W, std::vector<float> &L)
{
    const int hw = W / 2;
    std::vector<double> C((static_cast<size_t>(m) * (n + 1)), 0.0);
    for (int r = 0; r < m; ++r) {
        double run = 0;
        C[static_cast<size_t>(r * (n + 1))] = 0;
        for (int c = 0; c < n; ++c) {
            run += A[static_cast<size_t>(r * n + c)];
            C[static_cast<size_t>(r * (n + 1) + c + 1)] = run;
        }
    }
    std::vector<float> D(static_cast<size_t>(m) * n);
    for (int r = 0; r < m; ++r) {
        for (int c = 0; c < n; ++c) {
            const int c1 = std::max(0, c - hw);
            const int c2 = std::min(n - 1, c + hw);
            const double s = C[static_cast<size_t>(r * (n + 1) + c2 + 1)] - C[static_cast<size_t>(r * (n + 1) + c1)];
            D[static_cast<size_t>(r * n + c)] = static_cast<float>(s / (c2 - c1 + 1));
        }
    }
    std::vector<double> C2(static_cast<size_t>((m + 1) * n), 0.0);
    for (int c = 0; c < n; ++c) {
        double run = 0;
        C2[static_cast<size_t>(c)] = 0;
        for (int r = 0; r < m; ++r) {
            run += D[static_cast<size_t>(r * n + c)];
            C2[static_cast<size_t>((r + 1) * n + c)] = run;
        }
    }
    L.resize(static_cast<size_t>(m) * n);
    for (int r = 0; r < m; ++r) {
        const int r1 = std::max(0, r - hw);
        const int r2 = std::min(m - 1, r + hw);
        for (int c = 0; c < n; ++c) {
            const double s = C2[static_cast<size_t>((r2 + 1) * n + c)] - C2[static_cast<size_t>(r1 * n + c)];
            L[static_cast<size_t>(r * n + c)] = static_cast<float>(s / (r2 - r1 + 1));
        }
    }
}

struct SeamPath {
    int r0 = 0;
    int dr = 1;
    std::vector<float> v; // 画布列位置，按预览行
};

float pathAtRow(const SeamPath &p, int rowAbs)
{
    if (p.v.empty()) return 0;
    int k = static_cast<int>(std::llround((rowAbs - p.r0) / static_cast<double>(std::max(1, p.dr)))) + 1;
    k = clampv(k, 1, static_cast<int>(p.v.size()));
    return p.v[static_cast<size_t>(k - 1)];
}

void detectValidBox(const std::vector<uint16_t> &img, uint32_t W, uint32_t H,
                    int &c1, int &c2, int &r1, int &r2)
{
    c1 = 0;
    c2 = static_cast<int>(W) - 1;
    r1 = 0;
    r2 = static_cast<int>(H) - 1;
    // 抽稀采样阈值，避免 8K 图全像素扫描过慢
    const int step = std::max(1, static_cast<int>(std::min(W, H) / 1024));
    const double thr = std::max(1.0, percentileSample(img, 5.0, std::max(16, step * 4)) * 0.05);
    auto rowHas = [&](int y) {
        for (uint32_t x = 0; x < W; x += static_cast<uint32_t>(step))
            if (img[static_cast<size_t>(y * W + x)] > thr) return true;
        return false;
    };
    auto colHas = [&](int x) {
        for (uint32_t y = 0; y < H; y += static_cast<uint32_t>(step))
            if (img[static_cast<size_t>(y * W + x)] > thr) return true;
        return false;
    };
    while (r1 < r2 && !rowHas(r1)) r1 += step;
    while (r2 > r1 && !rowHas(r2)) r2 -= step;
    while (c1 < c2 && !colHas(c1)) c1 += step;
    while (c2 > c1 && !colHas(c2)) c2 -= step;
    r1 = clampv(r1, 0, static_cast<int>(H) - 1);
    r2 = clampv(r2, 0, static_cast<int>(H) - 1);
    c1 = clampv(c1, 0, static_cast<int>(W) - 1);
    c2 = clampv(c2, 0, static_cast<int>(W) - 1);
}

double nccOfFlat(const float *x, const float *y, int n)
{
    if (n < 16) return -9;
    double sx = 0, sy = 0;
    for (int i = 0; i < n; ++i) {
        sx += x[i];
        sy += y[i];
    }
    const double mx = sx / n, my = sy / n;
    double num = 0, nx = 0, ny = 0;
    for (int i = 0; i < n; ++i) {
        const double a = x[i] - mx, b = y[i] - my;
        num += a * b;
        nx += a * a;
        ny += b * b;
    }
    if (nx <= 1e-18 || ny <= 1e-18) return -9;
    return num / std::sqrt(nx * ny);
}

/** MATLAB structMapS: 1/4 预览 → |I-boxmean| → 再 /4 → 共 1/16，σ 归一 */
void buildStructMapS(const std::vector<uint16_t> &img, uint32_t W, uint32_t H,
                     std::vector<float> &Sec, int &sw, int &sh, int &scale)
{
    const int f = 4, d = 4;
    scale = f * d; // 16
    int pw = 0, ph = 0;
    std::vector<float> P;
    downsampleU16(img, W, H, f, P, pw, ph);
    // box mean w=9
    std::vector<float> B(P.size(), 0.f);
    const int hw = 4;
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            double s = 0;
            int n = 0;
            for (int dy = -hw; dy <= hw; ++dy) {
                for (int dx = -hw; dx <= hw; ++dx) {
                    const int yy = clampv(y + dy, 0, ph - 1);
                    const int xx = clampv(x + dx, 0, pw - 1);
                    s += P[static_cast<size_t>(yy * pw + xx)];
                    ++n;
                }
            }
            B[static_cast<size_t>(y * pw + x)] = static_cast<float>(s / std::max(1, n));
        }
    }
    const int m2 = ph / d, n2 = pw / d;
    sw = std::max(1, n2);
    sh = std::max(1, m2);
    Sec.assign(static_cast<size_t>(sw * sh), 0.f);
    if (m2 < 4 || n2 < 4) {
        // 退化：直接用预览高通归一
        for (int y = 0; y < sh; ++y)
            for (int x = 0; x < sw; ++x) {
                const int yy = std::min(ph - 1, y * d);
                const int xx = std::min(pw - 1, x * d);
                Sec[static_cast<size_t>(y * sw + x)] =
                    std::abs(P[static_cast<size_t>(yy * pw + xx)] - B[static_cast<size_t>(yy * pw + xx)]);
            }
    } else {
        for (int a = 0; a < m2; ++a) {
            for (int b = 0; b < n2; ++b) {
                double s = 0;
                for (int i = 0; i < d; ++i)
                    for (int j = 0; j < d; ++j) {
                        const int yy = a * d + i, xx = b * d + j;
                        s += std::abs(P[static_cast<size_t>(yy * pw + xx)] - B[static_cast<size_t>(yy * pw + xx)]);
                    }
                Sec[static_cast<size_t>(a * n2 + b)] = static_cast<float>(s / (d * d));
            }
        }
        sw = n2;
        sh = m2;
    }
    double m = 0, s2 = 0;
    for (float v : Sec) m += v;
    m /= std::max<size_t>(1, Sec.size());
    for (float v : Sec) s2 += (v - m) * (v - m);
    const double sd = std::sqrt(s2 / std::max<size_t>(1, Sec.size())) + 1e-9;
    for (float &v : Sec) v = static_cast<float>((v - m) / sd);
}

/** MATLAB scoreShiftS：3×3 分块平均 NCC（结构图尺度位移） */
double scoreShiftS(const std::vector<float> &A, const std::vector<float> &B,
                   int aw, int ah, int bw, int bh, int dr, int dc, int hr, int hc)
{
    const int np = std::min(ah, bh);
    const int mp = std::min(aw, bw);
    const int ovR = np - std::abs(dr);
    const int ovC = mp - std::abs(dc);
    if (ovR < 8 || ovC < 8) return -9;
    double tot = 0;
    int cnt = 0;
    std::vector<float> X, Y;
    X.reserve(static_cast<size_t>((hr / 2 + 1) * (hc / 2 + 1)));
    Y.reserve(X.capacity());
    for (int a = 1; a <= 3; ++a) {
        for (int b = 1; b <= 3; ++b) {
            const int r0 = std::max(0, dr) + static_cast<int>(std::llround(((a - 0.5) / 3.0) * ovR));
            const int c0 = std::max(0, dc) + static_cast<int>(std::llround(((b - 0.5) / 3.0) * ovC));
            const int r1 = r0 - dr;
            const int c1 = c0 - dc;
            if (r0 < 0 || c0 < 0 || r1 < 0 || c1 < 0) continue;
            if (r0 + hr > np || c0 + hc > mp || r1 + hr > np || c1 + hc > mp) continue;
            X.clear();
            Y.clear();
            for (int rr = 0; rr < hr; rr += 2) {
                for (int cc = 0; cc < hc; cc += 2) {
                    X.push_back(A[static_cast<size_t>((r0 + rr) * aw + (c0 + cc))]);
                    Y.push_back(B[static_cast<size_t>((r1 + rr) * bw + (c1 + cc))]);
                }
            }
            const double vv = nccOfFlat(X.data(), Y.data(), static_cast<int>(X.size()));
            if (vv > -8) {
                tot += vv;
                ++cnt;
            }
        }
    }
    if (cnt < 6) return -9;
    return tot / cnt;
}

/** 全分辨率条带精修（内存版 refinePairNCC，±rad） */
void refinePairNccMem(const std::vector<uint16_t> &A, uint32_t Wa, uint32_t Ha,
                      const std::vector<uint16_t> &B, uint32_t Wb, uint32_t Hb,
                      int dR0, int dC0, int rad, int &dR, int &dC, double &vBest)
{
    dR = dR0;
    dC = dC0;
    vBest = -9;
    const int r1 = std::max(0, dR0);
    const int r2 = std::min(static_cast<int>(Ha), static_cast<int>(Hb) + dR0) - 1;
    if (r2 - r1 < 64) return;
    const int bandH = std::min(768, r2 - r1 + 1);
    const int rm = (r1 + r2) / 2;
    int ra0 = clampv(rm - bandH / 2, r1, r2 - bandH + 1);
    const int stepC = std::max(1, static_cast<int>(std::min(Wa, Wb) / 1200));
    const int passes[3][3] = {{-32, 8, 32}, {-8, 2, 8}, {-2, 1, 2}}; // start, step, end abs pattern as -s:step:s
    for (int p = 0; p < 3; ++p) {
        const int span = passes[p][2];
        const int st = passes[p][1];
        const int baseR = dR, baseC = dC;
        for (int dd = -span; dd <= span; dd += st) {
            const int candR = baseR + dd;
            if (candR < dR0 - rad || candR > dR0 + rad) continue;
            for (int cc = -span; cc <= span; cc += st) {
                const int candC = baseC + cc;
                if (candC < dC0 - rad || candC > dC0 + rad) continue;
                const int c1 = std::max(0, candC);
                const int c2 = std::min(static_cast<int>(Wa), static_cast<int>(Wb) + candC) - 1;
                if (c2 - c1 < 256) continue;
                double sa = 0, sb = 0, sab = 0, sa2 = 0, sb2 = 0;
                int n = 0;
                for (int k = 0; k < bandH; ++k) {
                    const int ya = ra0 + k;
                    const int yb = ya - candR;
                    if (ya < 0 || ya >= static_cast<int>(Ha) || yb < 0 || yb >= static_cast<int>(Hb))
                        continue;
                    for (int x = c1; x <= c2; x += stepC) {
                        const int xb = x - candC;
                        if (xb < 0 || xb >= static_cast<int>(Wb)) continue;
                        const double va = A[static_cast<size_t>(ya * Wa + x)];
                        const double vb = B[static_cast<size_t>(yb * Wb + xb)];
                        sa += va;
                        sb += vb;
                        sab += va * vb;
                        sa2 += va * va;
                        sb2 += vb * vb;
                        ++n;
                    }
                }
                if (n < 64) continue;
                const double ma = sa / n, mb = sb / n;
                const double num = sab - n * ma * mb;
                const double den = std::sqrt(std::max(0.0, sa2 - n * ma * ma) * std::max(0.0, sb2 - n * mb * mb));
                if (den < 1e-12) continue;
                const double v = num / den;
                if (v > vBest) {
                    vBest = v;
                    dR = candR;
                    dC = candC;
                }
            }
        }
    }
}

/**
 * 方位向(左右)结构配准，对齐 MATLAB pairShiftAuto + forceDir='col' + refinePairNCC
 * 返回全分辨率 (dR,dC)：后一张相对前一张的放置位移
 */
bool estimatePairOffsetCol(const std::vector<uint16_t> &imgA, uint32_t Wa, uint32_t Ha,
                           const std::vector<uint16_t> &imgB, uint32_t Wb, uint32_t Hb,
                           int &dR, int &dC, double &ncc,
                           std::function<bool()> cancelled)
{
    std::vector<float> SA, SB;
    int aw, ah, bw, bh, scale = 16;
    buildStructMapS(imgA, Wa, Ha, SA, aw, ah, scale);
    buildStructMapS(imgB, Wb, Hb, SB, bw, bh, scale);
    const int np = std::min(ah, bh);
    const int mp = std::min(aw, bw);
    if (np < 16 || mp < 16) {
        dR = 0;
        dC = static_cast<int>(Wa) * 2 / 3;
        ncc = -1;
        return false;
    }
    const int hr = std::max(4, std::min(24, np / 10));
    const int hc = std::max(4, std::min(24, mp / 10));
    // 排除近零虚峰：位移占轴 2%~98%（与 MATLAB pairShiftAuto forceDir=col 一致，只扫正向列）
    const int loC = std::max(2, static_cast<int>(std::llround(0.02 * mp)));
    const int hiC = std::max(loC + 1, mp - 8);

    double vCol = -9;
    int dCw = std::max(loC, mp / 3);
    {
        std::atomic<int> next{loC};
        const int nt = hwThreads();
        std::vector<double> bestV(static_cast<size_t>(nt), -9.0);
        std::vector<int> bestDc(static_cast<size_t>(nt), dCw);
        std::vector<std::thread> pool;
        for (int t = 0; t < nt; ++t) {
            pool.emplace_back([&, t]() {
                for (;;) {
                    if (cancelled && cancelled()) return;
                    const int dc = next.fetch_add(1);
                    if (dc > hiC) break;
                    const double v = scoreShiftS(SA, SB, aw, ah, bw, bh, 0, dc, hr, hc);
                    if (v > bestV[static_cast<size_t>(t)]) {
                        bestV[static_cast<size_t>(t)] = v;
                        bestDc[static_cast<size_t>(t)] = dc;
                    }
                }
            });
        }
        for (auto &th : pool) th.join();
        if (cancelled && cancelled()) return false;
        for (int t = 0; t < nt; ++t) {
            if (bestV[static_cast<size_t>(t)] > vCol) {
                vCol = bestV[static_cast<size_t>(t)];
                dCw = bestDc[static_cast<size_t>(t)];
            }
        }
    }
    // 列向候选上做行向精修 ±8（MATLAB vCol2）
    double vCol2 = -9;
    int dRc = 0, dCc = dCw;
    for (int dr2 = -8; dr2 <= 8; ++dr2) {
        const double v = scoreShiftS(SA, SB, aw, ah, bw, bh, dr2, dCw, hr, hc);
        if (v > vCol2) {
            vCol2 = v;
            dRc = dr2;
            dCc = dCw;
        }
    }
    // 粗值 → 全分辨率（×16）再 refinePairNCC ±48（与 MATLAB 一致：仍返回估计值）
    const int dR0 = dRc * scale;
    const int dC0 = dCc * scale;
    refinePairNccMem(imgA, Wa, Ha, imgB, Wb, Hb, dR0, dC0, 48, dR, dC, ncc);
    if (ncc < -8) {
        dR = dR0;
        dC = dC0;
        ncc = vCol2;
    }
    return ncc > 0.05;
}

struct PrepPack {
    std::vector<uint16_t> lut;          // 65536
    std::vector<float> gainPrev;        // 预览尺寸增益场
    int gw = 0, gh = 0;
    int prevStep = 16;
    double lvlGain = 1.0;
};

void buildPreNormAndFlat(const std::vector<uint16_t> &img, uint32_t W, uint32_t H,
                         int prevStep, int preFlatBlk, double ref50, double ref99,
                         PrepPack &pack)
{
    pack.prevStep = prevStep;
    pack.lut.resize(65536);
    std::vector<float> P;
    int pw = 0, ph = 0;
    downsampleU16(img, W, H, prevStep, P, pw, ph);
    std::vector<double> vv;
    vv.reserve(P.size());
    for (float v : P)
        if (v > 0) vv.push_back(v);
    if (vv.empty()) {
        for (float v : P) vv.push_back(v);
    }
    const double p50 = std::max(percentileSorted(vv, 50), 1.0);
    const double p99 = std::max(percentileSorted(vv, 99), p50 + 1);
    double gm = std::log(std::max(ref99, ref50 + 1) / ref50) / std::max(std::log(p99 / p50), 1e-3);
    gm = clampv(gm, 0.4, 2.5);
    for (int x = 0; x < 65536; ++x) {
        if (x == 0) {
            pack.lut[0] = 0;
            continue;
        }
        const double yv = ref50 * std::pow(static_cast<double>(x) / p50, gm);
        pack.lut[static_cast<size_t>(x)] = static_cast<uint16_t>(clampv(yv, 0.0, 65535.0));
    }

    const int bf = std::max(2, preFlatBlk / prevStep);
    std::vector<float> G;
    int br = 0, bc = 0;
    boxMedianFlat(P, pw, ph, bf, G, br, bc);
    double Gm = 0;
    int gcnt = 0;
    for (float g : G) {
        if (g > 0) {
            Gm += g;
            ++gcnt;
        }
    }
    Gm = (gcnt > 0) ? (Gm / gcnt) : std::max(p50, 1.0);
    std::vector<float> ggrid = G;
    for (float &g : ggrid) {
        g = static_cast<float>(clampv(g / std::max(Gm, 1.0), 0.60, 1.65));
    }
    // 黑区不校正
    const float ref = static_cast<float>(std::max(p50, 1.0));
    for (size_t i = 0; i < ggrid.size(); ++i) {
        if (G[i] < std::max(1.f, 0.05f * ref))
            ggrid[i] = 1.f;
    }
    smoothGrid3(ggrid, br, bc);
    // 插值到预览网格
    pack.gw = pw;
    pack.gh = ph;
    pack.gainPrev.assign(static_cast<size_t>(pw * ph), 1.f);
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            const float xb = (static_cast<float>(x) + 0.5f) / static_cast<float>(bf) - 0.5f;
            const float yb = (static_cast<float>(y) + 0.5f) / static_cast<float>(bf) - 0.5f;
            pack.gainPrev[static_cast<size_t>(y * pw + x)] = sampleBilinear(ggrid, bc, br, xb, yb);
        }
    }
}

float sampleGain(const PrepPack &pack, int fullY, int fullX)
{
    if (pack.gainPrev.empty()) return 1.f;
    const float x = static_cast<float>(fullX) / static_cast<float>(pack.prevStep);
    const float y = static_cast<float>(fullY) / static_cast<float>(pack.prevStep);
    return std::max(sampleBilinear(pack.gainPrev, pack.gw, pack.gh, x, y), 0.2f);
}

uint16_t mapPixel(const PrepPack &pack, uint16_t v, int y, int x, double lvlGainOverride = -1.0)
{
    const uint16_t u = pack.lut.empty() ? v : pack.lut[v];
    const double g = sampleGain(pack, y, x);
    const double lg = (lvlGainOverride > 0.0) ? lvlGainOverride : pack.lvlGain;
    const double out = (static_cast<double>(u) / g) * lg;
    return static_cast<uint16_t>(clampv(out, 0.0, 65535.0));
}

bool findBestSeam(const std::vector<float> &A, const std::vector<float> &B, int mR, int mC,
                  double seamFindBand, std::vector<int> &pth)
{
    if (mR < 2 || mC < 4) return false;
    std::vector<float> cst(static_cast<size_t>(mR * mC));
    std::vector<float> dAB(static_cast<size_t>(mR * mC));
    std::vector<float> gA(static_cast<size_t>(mR * mC), 0.f);
    for (int r = 0; r < mR; ++r) {
        for (int c = 0; c < mC; ++c) {
            const size_t i = static_cast<size_t>(r * mC + c);
            dAB[i] = std::abs(A[i] - B[i]);
            if (c > 0)
                gA[i] = std::abs(A[i] - A[i - 1]);
        }
    }
    std::vector<double> tmp(dAB.begin(), dAB.end());
    const double medD = std::max(percentileSorted(tmp, 50), 1e-6);
    tmp.assign(gA.begin(), gA.end());
    const double medG = std::max(percentileSorted(tmp, 50), 1e-6);
    std::vector<double> mids;
    for (size_t i = 0; i < A.size(); ++i) {
        if (A[i] > 0 && B[i] > 0)
            mids.push_back(0.5 * (A[i] + B[i]));
    }
    const double mRef = mids.empty() ? 1.0 : std::max(percentileSorted(mids, 50), 1.0);
    for (int r = 0; r < mR; ++r) {
        for (int c = 0; c < mC; ++c) {
            const size_t i = static_cast<size_t>(r * mC + c);
            float cost = static_cast<float>(dAB[i] / medD + 0.5 * gA[i] / medG);
            if (!(A[i] > 0) || !(B[i] > 0))
                cost = 1e6f;
            const double mAB = 0.5 * (A[i] + B[i]);
            cost += static_cast<float>(std::max(0.0, 1.0 - std::min(mAB / mRef, 1.0)));
            cst[i] = cost;
        }
    }
    const int bandHalf = std::max(2, static_cast<int>(std::llround(seamFindBand * mC)));
    const int lo = std::max(1, mC / 2 - bandHalf) - 1; // 0-based
    const int hi = std::min(mC, mC / 2 + bandHalf + 1) - 1;
    const int nb = hi - lo + 1;
    if (nb < 2) return false;
    std::vector<double> cum(static_cast<size_t>(mR * nb), 1e300);
    std::vector<int> back(static_cast<size_t>(mR * nb), 0);
    for (int q = 0; q < nb; ++q)
        cum[static_cast<size_t>(q)] = cst[static_cast<size_t>(lo + q)];
    for (int r = 1; r < mR; ++r) {
        for (int q = 0; q < nb; ++q) {
            double best = cum[static_cast<size_t>((r - 1) * nb + q)];
            int bq = q;
            if (q > 0 && cum[static_cast<size_t>((r - 1) * nb + q - 1)] < best) {
                best = cum[static_cast<size_t>((r - 1) * nb + q - 1)];
                bq = q - 1;
            }
            if (q + 1 < nb && cum[static_cast<size_t>((r - 1) * nb + q + 1)] < best) {
                best = cum[static_cast<size_t>((r - 1) * nb + q + 1)];
                bq = q + 1;
            }
            cum[static_cast<size_t>(r * nb + q)] = best + cst[static_cast<size_t>(r * mC + lo + q)];
            back[static_cast<size_t>(r * nb + q)] = bq;
        }
    }
    int q = 0;
    double best = cum[static_cast<size_t>((mR - 1) * nb)];
    for (int i = 1; i < nb; ++i) {
        if (cum[static_cast<size_t>((mR - 1) * nb + i)] < best) {
            best = cum[static_cast<size_t>((mR - 1) * nb + i)];
            q = i;
        }
    }
    pth.resize(static_cast<size_t>(mR));
    for (int r = mR - 1; r >= 0; --r) {
        pth[static_cast<size_t>(r)] = lo + q + 1; // 1-based like MATLAB
        if (r > 0)
            q = back[static_cast<size_t>(r * nb + q)];
    }
    if (mR >= 5) {
        std::vector<int> sm = pth;
        for (int r = 1; r + 1 < mR; ++r)
            sm[static_cast<size_t>(r)] = (pth[static_cast<size_t>(r - 1)] + pth[static_cast<size_t>(r)]
                                         + pth[static_cast<size_t>(r + 1)]) / 3;
        sm[0] = sm[1] = sm[2];
        sm[static_cast<size_t>(mR - 1)] = sm[static_cast<size_t>(mR - 2)] = sm[static_cast<size_t>(mR - 3)];
        const int dp = std::max(1, static_cast<int>(std::llround(0.02 * mC)));
        for (int r = 1; r < mR; ++r)
            sm[static_cast<size_t>(r)] = clampv(sm[static_cast<size_t>(r)], sm[static_cast<size_t>(r - 1)] - dp, sm[static_cast<size_t>(r - 1)] + dp);
        for (int &v : sm) v = clampv(v, 1, mC);
        pth.swap(sm);
    }
    return true;
}

static float seamRamp(double v, float s1, float s2)
{
    if (!(s2 > s1) || !(s1 > 0))
        return 1.f;
    const float t = static_cast<float>((s2 - v) / (s2 - s1));
    return std::min(1.f, std::max(0.f, t));
}

/** 纯几何+羽化导出 MOSAIC_RCS.tif / MOSAIC_L3.txt（不经 preNorm/匀光） */
bool exportPureValue(const std::vector<std::string> &paths, const std::vector<uint32_t> &widths,
                     const std::vector<uint32_t> &heights, const std::vector<int> &rOff,
                     const std::vector<int> &cOff, const std::vector<int> &vR1,
                     const std::vector<int> &vC1, const std::vector<int> &Nr,
                     const std::vector<int> &Na, const std::vector<float> &seamC_L,
                     const std::vector<float> &seamC_R, int canvasH, int canvasW,
                     const std::string &outDir, MosaicResult &result,
                     std::function<void(int, const std::string &)> progress,
                     std::function<bool()> cancelled, std::string *err)
{
    if (!result.writeRcsTif && !result.writeL3)
        return true;
    const int n = static_cast<int>(paths.size());
    if (progress)
        progress(84, "导出 RCS 原值 / 三级数据…");

    // 预读原值图（int32）
    std::vector<std::vector<int32_t>> raw(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (cancelled && cancelled())
            return false;
        bool isSigned = false;
        uint32_t w = 0, h = 0;
        if (!readTiff16AsI32(paths[static_cast<size_t>(i)], raw[static_cast<size_t>(i)], w, h, isSigned,
                             err))
            return false;
        if (w != widths[static_cast<size_t>(i)] || h != heights[static_cast<size_t>(i)]) {
            if (err)
                *err = "导出时尺寸与配准不一致: " + paths[static_cast<size_t>(i)];
            return false;
        }
        (void)isSigned;
    }

    std::vector<float> acc(static_cast<size_t>(canvasH) * canvasW, 0.f);
    std::vector<float> wsum(static_cast<size_t>(canvasH) * canvasW, 0.f);
    for (int i = 0; i < n; ++i) {
        if (cancelled && cancelled())
            return false;
        const int c0 = std::max(0, cOff[static_cast<size_t>(i)]);
        const int c1 = std::min(canvasW, cOff[static_cast<size_t>(i)] + Na[static_cast<size_t>(i)]);
        const int r0 = std::max(0, rOff[static_cast<size_t>(i)]);
        const int r1 = std::min(canvasH, rOff[static_cast<size_t>(i)] + Nr[static_cast<size_t>(i)]);
        if (c0 >= c1 || r0 >= r1)
            continue;
        // MATLAB: w = localRamp(col, seamC(i,左), seamC(i,右)) → 左缝=1、右缝=0
        const float sL = seamC_L[static_cast<size_t>(i)];
        const float sR = seamC_R[static_cast<size_t>(i)];
        const uint32_t wi = widths[static_cast<size_t>(i)];
        for (int y = r0; y < r1; ++y) {
            const int srcY = y - rOff[static_cast<size_t>(i)] + vR1[static_cast<size_t>(i)];
            for (int x = c0; x < c1; ++x) {
                const int srcX = x - cOff[static_cast<size_t>(i)] + vC1[static_cast<size_t>(i)];
                const float w = std::max(1e-4f, seamRamp(static_cast<double>(x) + 1.0, sL, sR));
                const int32_t dn =
                    raw[static_cast<size_t>(i)][static_cast<size_t>(srcY) * wi + static_cast<size_t>(srcX)];
                const size_t idx = static_cast<size_t>(y) * canvasW + static_cast<size_t>(x);
                acc[idx] += static_cast<float>(dn) * w;
                wsum[idx] += w;
            }
        }
    }
    raw.clear();

    std::vector<int16_t> outR(static_cast<size_t>(canvasH) * canvasW, static_cast<int16_t>(-32768));
    std::vector<float> dbsm;
    if (result.writeL3)
        dbsm.assign(outR.size(), std::numeric_limits<float>::quiet_NaN());
    const double scale = (result.l3Scale > 0) ? result.l3Scale : 100.0;
    for (size_t i = 0; i < outR.size(); ++i) {
        if (wsum[i] <= 0)
            continue;
        const float v = acc[i] / wsum[i];
        int iv = static_cast<int>(std::llround(v));
        if (iv < -32767)
            iv = -32767;
        if (iv > 32767)
            iv = 32767;
        outR[i] = static_cast<int16_t>(iv);
        if (result.writeL3)
            dbsm[i] = static_cast<float>(v / scale);
    }
    acc.clear();
    wsum.clear();

    if (result.writeRcsTif) {
        result.rcsTif = pathJoin(outDir, "MOSAIC_RCS.tif");
        // 用 libtiff 写 int16
        TIFF *tif = TIFFOpen(result.rcsTif.c_str(), "w");
        if (!tif) {
            if (err)
                *err = "无法创建 " + result.rcsTif;
            return false;
        }
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(canvasW));
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(canvasH));
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_INT);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, static_cast<uint32_t>(canvasH));
        for (int y = 0; y < canvasH; ++y) {
            if (TIFFWriteScanline(tif, outR.data() + static_cast<size_t>(y) * canvasW, y) < 0) {
                TIFFClose(tif);
                if (err)
                    *err = "写 MOSAIC_RCS.tif 失败";
                return false;
            }
        }
        TIFFClose(tif);
        if (progress)
            progress(90, "已写 " + result.rcsTif);
    }

    if (result.writeL3) {
        result.l3Path = pathJoin(outDir, "MOSAIC_L3.txt");
        Level3Info info = result.l3Header;
        if (info.taskName.empty())
            info.taskName = "SAR二维像RCS拼接";
        if (info.dataSource.empty())
            info.dataSource = "多张定标TIFF拼接";
        if (info.dataType.empty())
            info.dataType = "3级 RCS 二维像(dBsm)";
        if (info.calMode.empty())
            info.calMode = "角反射器能量(积分)法";
        if (info.calName.empty())
            info.calName = "角反射器";
        if (info.tgtName.empty())
            info.tgtName = "MOSAIC";
        if (info.pol.empty())
            info.pol = result.pol;
        {
            std::ostringstream note;
            note << "波段=" << result.band << "; 极化=" << result.pol
                 << "; 单位=dBsm(真值=拼接DN/" << scale
                 << "); 仅几何摆位+羽化加权; 未做匀色/明暗调整";
            if (info.note.empty())
                info.note = note.str();
        }
        // 流式写 5252 头 + DATA
        auto field = [](const std::string &k, const std::string &v) { return k + " = " + v + "\r\n"; };
        std::ostringstream hdr;
        hdr << "[TASK]\r\n" << field("任务名称", info.taskName) << field("任务负责人", info.operatorName)
            << field("任务单位", info.unit) << field("任务代号", info.code) << field("任务来源", info.source)
            << field("数据来源", info.dataSource) << field("数据类型", info.dataType)
            << field("任务执行地点", info.place) << field("任务执行时间", info.time)
            << field("定标形式", info.calMode) << field("定标体名称", info.calName)
            << field("定标体尺寸", info.calSize) << field("不确定度", info.uncertainty) << "[/TASK]\r\n"
            << "[TARGET]\r\n" << field("目标名称", info.tgtName) << field("目标说明", info.tgtDesc)
            << field("目标分类", info.tgtClass) << field("目标长(m)", info.tgtL)
            << field("目标宽(m)", info.tgtW) << field("目标高(m)", info.tgtH) << "[/TARGET]\r\n"
            << "[SYSTEM]\r\n" << field("设备名称", info.devName) << field("设备分类", info.devClass)
            << field("工作体制", info.sysMode) << field("脉冲宽度(μs)", info.pulseWidth)
            << field("脉冲重复频率", info.prf) << field("动态范围(dB)", info.dynRange) << "[/SYSTEM]\r\n"
            << "[ENVIRONMENT]\r\n" << field("温度(℃)", info.temp) << field("湿度(%)", info.humidity)
            << field("气压(hPa)", info.pressure) << field("能见度(km)", info.visibility)
            << field("云况", info.cloud) << "[/ENVIRONMENT]\r\n"
            << "[CONDITIONS]\r\n" << field("测量时刻", info.measureTime)
            << field("频率步长(GHz)", info.fStep) << field("起始频率(GHz)", info.fStart)
            << field("终止频率(GHz)", info.fStop) << field("极化组合", info.pol)
            << field("目标高度(m)", info.tgtHeight) << field("起始入射方位视向角(°)", info.azStart)
            << field("终止入射方位视向角(°)", info.azStop) << field("起始入射俯仰视向角(°)", info.elStart)
            << field("终止入射俯仰视向角(°)", info.elStop) << field("回波采样率", info.fs)
            << field("备注", info.note) << "[/CONDITIONS]\r\n";
        std::string header = hdr.str();
        const int kHdr = 5252;
        if (static_cast<int>(header.size()) < kHdr)
            header.append(static_cast<size_t>(kHdr - static_cast<int>(header.size())), ' ');
        FILE *fp = std::fopen(result.l3Path.c_str(), "wb");
        if (!fp) {
            if (err)
                *err = "无法写 " + result.l3Path;
            return false;
        }
        std::fwrite(header.data(), 1, header.size(), fp);
        char line[128];
        int nw = std::snprintf(line, sizeof(line),
                               "[DATA]\r\nRCS[径向点数（行数%d），横向点数（列数%d）]\r\n", canvasH,
                               canvasW);
        if (nw > 0)
            std::fwrite(line, 1, static_cast<size_t>(nw), fp);
        char num[64];
        if (result.l3Layout == 1) {
            for (int r = 0; r < canvasH; ++r) {
                for (int c = 0; c < canvasW; ++c) {
                    if (c)
                        std::fputc(' ', fp);
                    const float v = dbsm[static_cast<size_t>(r) * canvasW + c];
                    if (v != v)
                        std::fputs("NaN", fp);
                    else {
                        std::snprintf(num, sizeof(num), "%.3f", static_cast<double>(v));
                        std::fputs(num, fp);
                    }
                }
                std::fputs("\r\n", fp);
            }
        } else {
            for (int r = 0; r < canvasH; ++r) {
                for (int c = 0; c < canvasW; ++c) {
                    const float v = dbsm[static_cast<size_t>(r) * canvasW + c];
                    if (v != v)
                        nw = std::snprintf(line, sizeof(line), "%d %d NaN\r\n", r + 1, c + 1);
                    else
                        nw = std::snprintf(line, sizeof(line), "%d %d %.3f\r\n", r + 1, c + 1,
                                           static_cast<double>(v));
                    if (nw > 0)
                        std::fwrite(line, 1, static_cast<size_t>(nw), fp);
                }
            }
        }
        std::fputs("[/DATA]\r\n", fp);
        const bool ok = std::ferror(fp) == 0;
        std::fclose(fp);
        if (!ok) {
            if (err)
                *err = "写三级数据失败";
            return false;
        }
        if (progress)
            progress(93, "已写 " + result.l3Path);
    }
    return true;
}

bool fuseAndWrite(std::vector<std::vector<uint16_t>> &imgs,
                  std::vector<uint32_t> &widths,
                  std::vector<uint32_t> &heights,
                  std::vector<int> offR, std::vector<int> offC,
                  const std::vector<std::string> &paths,
                  const std::string &outDir, bool doFlatten, double medOut,
                  MosaicResult &result,
                  std::function<void(int, const std::string &)> progress,
                  std::function<bool()> cancelled,
                  std::string *err)
{
    const int n = static_cast<int>(imgs.size());
    if (n < 2) {
        if (err) *err = "至少需要2张图";
        return false;
    }

    int maxNr = 0, maxNa = 0;
    for (int i = 0; i < n; ++i) {
        maxNr = std::max(maxNr, static_cast<int>(heights[static_cast<size_t>(i)]));
        maxNa = std::max(maxNa, static_cast<int>(widths[static_cast<size_t>(i)]));
    }
    // 自动推导（与 MATLAB autoTune）
    int prevStep = 1 << clampi(static_cast<int>(std::llround(std::log2(static_cast<double>(std::max(maxNr, maxNa)) / 1024.0))), 4, 5);
    int preFlatBlk = std::max(64, std::min(maxNr, maxNa) / 16);
    // 过渡带先按尺寸给初值；放置后按最小重叠再收窄（见下方 ovlMin）
    int featherPix = 48;
    int featherWide = 512;
    const double seamFindBand = 0.05;
    const double postPctLo = 1.0, postPctHi = 99.5, postGamma = 0.75, postMedTarget = 0.35;

    if (progress) progress(25, "拼前匀光/亮度对齐…");

    // 参考中位 / 99%（并行采样）
    std::vector<double> p50s(static_cast<size_t>(n)), p99s(static_cast<size_t>(n));
    {
        std::atomic<int> next{0};
        const int nt = hwThreads();
        std::vector<std::thread> pool;
        for (int t = 0; t < nt; ++t) {
            pool.emplace_back([&]() {
                for (;;) {
                    const int i = next.fetch_add(1);
                    if (i >= n) break;
                    p50s[static_cast<size_t>(i)] = std::max(percentileSample(imgs[static_cast<size_t>(i)], 50), 1.0);
                    p99s[static_cast<size_t>(i)] = std::max(percentileSample(imgs[static_cast<size_t>(i)], 99),
                                                        p50s[static_cast<size_t>(i)] + 1);
                }
            });
        }
        for (auto &th : pool) th.join();
    }
    std::vector<double> tmp50 = p50s, tmp99 = p99s;
    const double ref50 = percentileSorted(tmp50, 50);
    const double ref99 = percentileSorted(tmp99, 50);

    std::vector<PrepPack> packs(static_cast<size_t>(n));
    {
        std::atomic<int> next{0};
        const int nt = hwThreads();
        std::vector<std::thread> pool;
        for (int t = 0; t < nt; ++t) {
            pool.emplace_back([&]() {
                for (;;) {
                    const int i = next.fetch_add(1);
                    if (i >= n) break;
                    if (cancelled && cancelled()) return;
                    buildPreNormAndFlat(imgs[static_cast<size_t>(i)], widths[static_cast<size_t>(i)],
                                        heights[static_cast<size_t>(i)], prevStep, preFlatBlk, ref50, ref99,
                                        packs[static_cast<size_t>(i)]);
                }
            });
        }
        for (auto &th : pool) th.join();
    }
    if (cancelled && cancelled()) return false;

    // 有效区检测 + inset 裁剪
    std::vector<int> vC1(n), vC2(n), vR1(n), vR2(n);
    for (int i = 0; i < n; ++i) {
        detectValidBox(imgs[static_cast<size_t>(i)], widths[static_cast<size_t>(i)],
                       heights[static_cast<size_t>(i)], vC1[i], vC2[i], vR1[i], vR2[i]);
    }

    int canvasMinR = offR[0] + vR1[0], canvasMinC = offC[0] + vC1[0];
    int canvasMaxR = offR[0] + vR2[0] + 1, canvasMaxC = offC[0] + vC2[0] + 1;
    for (int i = 1; i < n; ++i) {
        canvasMinR = std::min(canvasMinR, offR[i] + vR1[i]);
        canvasMinC = std::min(canvasMinC, offC[i] + vC1[i]);
        canvasMaxR = std::max(canvasMaxR, offR[i] + vR2[i] + 1);
        canvasMaxC = std::max(canvasMaxC, offC[i] + vC2[i] + 1);
    }
    // 公共覆盖（inset）
    int colA = offC[0] + vC1[0], colB = offC[0] + vC2[0];
    int rowA = offR[0] + vR1[0], rowB = offR[0] + vR2[0];
    for (int i = 1; i < n; ++i) {
        colA = std::max(colA, offC[i] + vC1[i]);
        colB = std::min(colB, offC[i] + vC2[i]);
        rowA = std::max(rowA, offR[i] + vR1[i]);
        rowB = std::min(rowB, offR[i] + vR2[i]);
    }
    int canvasW0 = canvasMaxC - canvasMinC;
    int canvasH0 = canvasMaxR - canvasMinR;
    if ((colB - colA + 1) >= static_cast<int>(0.30 * canvasW0)) {
        canvasMinC = colA;
        canvasMaxC = colB + 1;
    }
    if ((rowB - rowA + 1) >= static_cast<int>(0.30 * canvasH0) && (rowA > canvasMinR || rowB + 1 < canvasMaxR)) {
        canvasMinR = rowA;
        canvasMaxR = rowB + 1;
    }

    // 裁到有效子区后的偏移与尺寸
    std::vector<int> rOff(n), cOff(n);
    std::vector<int> Nr(n), Na(n);
    for (int i = 0; i < n; ++i) {
        rOff[i] = offR[i] + vR1[i] - canvasMinR;
        cOff[i] = offC[i] + vC1[i] - canvasMinC;
        Nr[i] = vR2[i] - vR1[i] + 1;
        Na[i] = vC2[i] - vC1[i] + 1;
    }
    int minR2 = rOff[0], minC2 = cOff[0];
    for (int i = 1; i < n; ++i) {
        minR2 = std::min(minR2, rOff[i]);
        minC2 = std::min(minC2, cOff[i]);
    }
    for (int i = 0; i < n; ++i) {
        rOff[i] -= minR2;
        cOff[i] -= minC2;
    }
    int canvasH = 0, canvasW = 0;
    for (int i = 0; i < n; ++i) {
        canvasH = std::max(canvasH, rOff[i] + Nr[i]);
        canvasW = std::max(canvasW, cOff[i] + Na[i]);
    }
    if (canvasH <= 0 || canvasW <= 0) {
        if (err) *err = "画布尺寸异常";
        return false;
    }

    // 过渡带按实际最小重叠自动收窄（对齐 MATLAB autoTune）
    {
        int ovlCmin = std::numeric_limits<int>::max();
        int ovlRmin = std::numeric_limits<int>::max();
        for (int i = 0; i + 1 < n; ++i) {
            const int dC = offC[i + 1] - offC[i];
            const int dR = offR[i + 1] - offR[i];
            const int na = static_cast<int>(std::min(widths[static_cast<size_t>(i)], widths[static_cast<size_t>(i + 1)]));
            const int nr = static_cast<int>(std::min(heights[static_cast<size_t>(i)], heights[static_cast<size_t>(i + 1)]));
            ovlCmin = std::min(ovlCmin, na - std::abs(dC));
            ovlRmin = std::min(ovlRmin, nr - std::abs(dR));
        }
        const int ovlMin = std::max(16, std::min(ovlCmin, ovlRmin));
        featherWide = std::max(24, static_cast<int>(std::llround(0.40 * ovlMin)));
        featherPix = std::max(32, static_cast<int>(std::llround(0.12 * ovlMin)));
        if (progress)
            progress(38, "最小重叠 " + std::to_string(ovlMin) + " px → 低频过渡 " + std::to_string(featherWide) + " / 高频过渡 " + std::to_string(featherPix));
    }

    // 直线缝（列向）
    result.seamCols.clear();
    std::vector<float> seamC_L(static_cast<size_t>(n), NAN), seamC_R(static_cast<size_t>(n), NAN);
    for (int i = 0; i + 1 < n; ++i) {
        const float s = (cOff[i] + Na[i] + cOff[i + 1] + 1) * 0.5f;
        seamC_R[static_cast<size_t>(i)] = s;
        seamC_L[static_cast<size_t>(i + 1)] = s;
        result.seamCols.push_back(static_cast<int>(std::llround(s)));
    }

    if (progress) progress(40, "重叠区电平匹配…");
    // 全局最小二乘电平配平（大图必须抽稀；禁止按像素拷贝 PrepPack）
    std::vector<int> pI, pJ;
    std::vector<double> pR;
    for (int i = 0; i + 1 < n; ++i) {
        if (cancelled && cancelled()) return false;
        const int j = i + 1;
        const int r1 = std::max(rOff[i], rOff[j]);
        const int r2 = std::min(rOff[i] + Nr[i], rOff[j] + Nr[j]);
        const int c1 = std::max(cOff[i], cOff[j]);
        const int c2 = std::min(cOff[i] + Na[i], cOff[j] + Na[j]);
        if ((r2 - r1) < 128 || (c2 - c1) < 128) continue;

        const int rowStep = std::max(4, (r2 - r1) / 400);
        const int colStep = std::max(4, (c2 - c1) / 200);
        const int cEnd = std::min(c2, c1 + 512);
        const PrepPack &packA = packs[static_cast<size_t>(i)];
        const PrepPack &packB = packs[static_cast<size_t>(j)];
        const uint32_t wi = widths[static_cast<size_t>(i)];
        const uint32_t wj = widths[static_cast<size_t>(j)];
        const int hi = static_cast<int>(heights[static_cast<size_t>(i)]);
        const int hj = static_cast<int>(heights[static_cast<size_t>(j)]);

        std::vector<double> aa, bb;
        aa.reserve(20000);
        bb.reserve(20000);
        for (int rr = r1; rr < r2; rr += rowStep) {
            if (cancelled && cancelled()) return false;
            for (int cc = c1; cc < cEnd; cc += colStep) {
                const int yi = rr - rOff[i] + vR1[i];
                const int xi = cc - cOff[i] + vC1[i];
                const int yj = rr - rOff[j] + vR1[j];
                const int xj = cc - cOff[j] + vC1[j];
                if (yi < 0 || xi < 0 || yj < 0 || xj < 0) continue;
                if (xi >= static_cast<int>(wi) || yi >= hi) continue;
                if (xj >= static_cast<int>(wj) || yj >= hj) continue;
                const uint16_t va = imgs[static_cast<size_t>(i)][static_cast<size_t>(yi * wi + xi)];
                const uint16_t vb = imgs[static_cast<size_t>(j)][static_cast<size_t>(yj * wj + xj)];
                if (vb == 0) continue;
                // lvlGain=1：在配平前同口径比较（勿拷贝整个 PrepPack）
                const double A = mapPixel(packA, va, yi, xi, 1.0);
                const double B = mapPixel(packB, vb, yj, xj, 1.0);
                if (B <= 0) continue;
                aa.push_back(A);
                bb.push_back(B);
                if (aa.size() >= 25000) break;
            }
            if (aa.size() >= 25000) break;
        }
        if (aa.size() < 500) continue;
        std::vector<double> aa2 = aa, bb2 = bb;
        const double ma = percentileSorted(aa2, 50);
        const double mb = percentileSorted(bb2, 50);
        if (ma <= 0 || mb <= 0) continue;
        pI.push_back(i);
        pJ.push_back(j);
        pR.push_back(std::log(ma / mb));
        if (progress)
            progress(40 + (i + 1) * 8 / std::max(1, n - 1), "重叠区电平匹配 " + std::to_string(i + 1) + "/" + std::to_string(n - 1));
    }
    if (!pR.empty() && n > 1) {
        const int nVar = n - 1;
        std::vector<double> AtA(static_cast<size_t>(nVar * nVar), 0.0);
        std::vector<double> Atb(static_cast<size_t>(nVar), 0.0);
        for (size_t k = 0; k < pR.size(); ++k) {
            // log g_j - log g_i = pR ; g0 fixed = 0
            const int i = pI[k], j = pJ[k];
            std::vector<double> row(static_cast<size_t>(nVar), 0.0);
            if (j > 0) row[static_cast<size_t>(j - 1)] += 1.0;
            if (i > 0) row[static_cast<size_t>(i - 1)] -= 1.0;
            for (int a = 0; a < nVar; ++a) {
                Atb[static_cast<size_t>(a)] += row[static_cast<size_t>(a)] * pR[k];
                for (int b = 0; b < nVar; ++b)
                    AtA[static_cast<size_t>(a * nVar + b)] += row[static_cast<size_t>(a)] * row[static_cast<size_t>(b)];
            }
        }
        // 高斯消元
        std::vector<double> lg(static_cast<size_t>(nVar), 0.0);
        for (int col = 0; col < nVar; ++col) {
            int piv = col;
            for (int r = col + 1; r < nVar; ++r)
                if (std::abs(AtA[static_cast<size_t>(r * nVar + col)]) > std::abs(AtA[static_cast<size_t>(piv * nVar + col)]))
                    piv = r;
            for (int c = 0; c < nVar; ++c)
                std::swap(AtA[static_cast<size_t>(col * nVar + c)], AtA[static_cast<size_t>(piv * nVar + c)]);
            std::swap(Atb[static_cast<size_t>(col)], Atb[static_cast<size_t>(piv)]);
            const double d = AtA[static_cast<size_t>(col * nVar + col)];
            if (std::abs(d) < 1e-12) continue;
            for (int r = col + 1; r < nVar; ++r) {
                const double f = AtA[static_cast<size_t>(r * nVar + col)] / d;
                for (int c = col; c < nVar; ++c)
                    AtA[static_cast<size_t>(r * nVar + c)] -= f * AtA[static_cast<size_t>(col * nVar + c)];
                Atb[static_cast<size_t>(r)] -= f * Atb[static_cast<size_t>(col)];
            }
        }
        for (int i = nVar - 1; i >= 0; --i) {
            double s = Atb[static_cast<size_t>(i)];
            for (int c = i + 1; c < nVar; ++c)
                s -= AtA[static_cast<size_t>(i * nVar + c)] * lg[static_cast<size_t>(c)];
            const double d = AtA[static_cast<size_t>(i * nVar + i)];
            lg[static_cast<size_t>(i)] = (std::abs(d) < 1e-12) ? 0.0 : (s / d);
        }
        packs[0].lvlGain = 1.0;
        for (int i = 1; i < n; ++i)
            packs[static_cast<size_t>(i)].lvlGain = clampv(std::exp(lg[static_cast<size_t>(i - 1)]), 0.5, 2.0);
    }

    if (progress) progress(50, "搜索最佳接缝…");
    std::vector<SeamPath> pathL(static_cast<size_t>(n)), pathR(static_cast<size_t>(n));
    std::vector<char> hasPathL(static_cast<size_t>(n), 0), hasPathR(static_cast<size_t>(n), 0);

    // 预览图（映射后）用于接缝
    std::vector<std::vector<float>> Pv(static_cast<size_t>(n));
    std::vector<int> Pw(n), Ph(n);
    {
        std::atomic<int> next{0};
        std::vector<std::thread> pool;
        for (int t = 0; t < hwThreads(); ++t) {
            pool.emplace_back([&]() {
                for (;;) {
                    const int i = next.fetch_add(1);
                    if (i >= n) break;
                    downsampleU16(imgs[static_cast<size_t>(i)], widths[static_cast<size_t>(i)],
                                  heights[static_cast<size_t>(i)], prevStep, Pv[static_cast<size_t>(i)],
                                  Pw[i], Ph[i]);
                    // 在预览上近似应用 LUT/增益（抽稀采样）
                    for (int y = 0; y < Ph[i]; ++y) {
                        for (int x = 0; x < Pw[i]; ++x) {
                            const int fy = std::min(static_cast<int>(heights[static_cast<size_t>(i)]) - 1, y * prevStep);
                            const int fx = std::min(static_cast<int>(widths[static_cast<size_t>(i)]) - 1, x * prevStep);
                            const uint16_t raw = imgs[static_cast<size_t>(i)][static_cast<size_t>(fy * widths[static_cast<size_t>(i)] + fx)];
                            Pv[static_cast<size_t>(i)][static_cast<size_t>(y * Pw[i] + x)] =
                                static_cast<float>(mapPixel(packs[static_cast<size_t>(i)], raw, fy, fx));
                        }
                    }
                }
            });
        }
        for (auto &th : pool) th.join();
    }

    for (int i = 0; i + 1 < n; ++i) {
        if (cancelled && cancelled()) return false;
        const int j = i + 1;
        const int r1 = std::max(rOff[i], rOff[j]) + 1;
        const int r2 = std::min(rOff[i] + Nr[i], rOff[j] + Nr[j]);
        const int c1 = std::max(cOff[i], cOff[j]) + 1;
        const int c2 = std::min(cOff[i] + Na[i], cOff[j] + Na[j]);
        if ((r2 - r1) < 32 || (c2 - c1) < 32) continue;
        std::vector<int> rr, cc;
        for (int r = r1; r <= r2; r += prevStep) rr.push_back(r);
        for (int c = c1; c <= c2; c += prevStep) cc.push_back(c);
        const int mR = static_cast<int>(rr.size());
        const int mC = static_cast<int>(cc.size());
        std::vector<float> A(static_cast<size_t>(mR * mC)), B(static_cast<size_t>(mR * mC));
        for (int a = 0; a < mR; ++a) {
            for (int b = 0; b < mC; ++b) {
                const int ri = clampv((rr[static_cast<size_t>(a)] - rOff[i] - 1) / prevStep, 0, Ph[i] - 1);
                const int ci = clampv((cc[static_cast<size_t>(b)] - cOff[i] - 1) / prevStep, 0, Pw[i] - 1);
                const int rj = clampv((rr[static_cast<size_t>(a)] - rOff[j] - 1) / prevStep, 0, Ph[j] - 1);
                const int cj = clampv((cc[static_cast<size_t>(b)] - cOff[j] - 1) / prevStep, 0, Pw[j] - 1);
                // 有效子区相对原图：预览是整图抽稀，需加 vR1/vC1
                const int ri2 = clampv((rr[static_cast<size_t>(a)] - rOff[i] + vR1[i]) / prevStep, 0, Ph[i] - 1);
                const int ci2 = clampv((cc[static_cast<size_t>(b)] - cOff[i] + vC1[i]) / prevStep, 0, Pw[i] - 1);
                const int rj2 = clampv((rr[static_cast<size_t>(a)] - rOff[j] + vR1[j]) / prevStep, 0, Ph[j] - 1);
                const int cj2 = clampv((cc[static_cast<size_t>(b)] - cOff[j] + vC1[j]) / prevStep, 0, Pw[j] - 1);
                (void)ri;
                (void)ci;
                (void)rj;
                (void)cj;
                A[static_cast<size_t>(a * mC + b)] = Pv[static_cast<size_t>(i)][static_cast<size_t>(ri2 * Pw[i] + ci2)];
                B[static_cast<size_t>(a * mC + b)] = Pv[static_cast<size_t>(j)][static_cast<size_t>(rj2 * Pw[j] + cj2)];
            }
        }
        std::vector<int> pth;
        if (!findBestSeam(A, B, mR, mC, seamFindBand, pth)) continue;
        SeamPath sp;
        sp.r0 = r1;
        sp.dr = prevStep;
        sp.v.resize(pth.size());
        double maxDev = 0;
        const double mid = 0.5 * (c1 + c2);
        for (size_t k = 0; k < pth.size(); ++k) {
            sp.v[k] = static_cast<float>(c1 + (pth[k] - 1) * prevStep + prevStep / 2.0);
            maxDev = std::max(maxDev, std::abs(sp.v[k] - mid));
        }
        if (maxDev > 0.55 * (c2 - c1 + 1)) continue;
        pathR[static_cast<size_t>(i)] = sp;
        pathL[static_cast<size_t>(j)] = sp;
        hasPathR[static_cast<size_t>(i)] = 1;
        hasPathL[static_cast<size_t>(j)] = 1;
    }
    Pv.clear();

    if (progress) progress(60, "多带融合写出…");
    // 过渡带已按重叠定宽，不再按画布宽度二次压窄（与 MATLAB autoTune 一致）
    featherWide = std::min(featherWide, std::max(24, canvasW / 2));
    featherPix = std::min(featherPix, featherWide);
    int stripH = std::max(128, std::min(1024, static_cast<int>(64e6 / (std::max(canvasW, 1) * 4))));
    stripH = std::min(stripH, canvasH);

    std::vector<uint16_t> outPix(static_cast<size_t>(canvasH) * canvasW, 0);
    std::vector<uint16_t> sampAll;
    sampAll.reserve(static_cast<size_t>(canvasH / 8 + 1) * (canvasW / 8 + 1));

    const int nStrip = (canvasH + stripH - 1) / stripH;
    std::atomic<int> nextStrip{0};
    std::atomic<bool> failFlag{false};
    const int nt = hwThreads();
    std::vector<std::thread> pool;
    for (int t = 0; t < nt; ++t) {
        pool.emplace_back([&]() {
            while (!failFlag.load()) {
                if (cancelled && cancelled()) {
                    failFlag = true;
                    return;
                }
                const int s = nextStrip.fetch_add(1);
                if (s >= nStrip) return;
                const int rowA = s * stripH;
                const int rowB = std::min(canvasH, rowA + stripH);
                const int nR = rowB - rowA;
                std::vector<float> accL(static_cast<size_t>(nR) * canvasW, 0.f);
                std::vector<float> wL(static_cast<size_t>(nR) * canvasW, 0.f);
                std::vector<float> accH(static_cast<size_t>(nR) * canvasW, 0.f);
                std::vector<float> wH(static_cast<size_t>(nR) * canvasW, 0.f);

                for (int i = 0; i < n; ++i) {
                    const int imgR0 = rOff[i];
                    const int imgR1 = rOff[i] + Nr[i];
                    const int ov0 = std::max(rowA, imgR0);
                    const int ov1 = std::min(rowB, imgR1);
                    if (ov0 >= ov1) continue;
                    const int c0 = std::max(0, cOff[i]);
                    const int c1 = std::min(canvasW, cOff[i] + Na[i]);
                    if (c0 >= c1) continue;

                    const int blkH = ov1 - ov0;
                    const int blkW = c1 - c0;
                    std::vector<float> blk(static_cast<size_t>(blkH * blkW));
                    for (int y = 0; y < blkH; ++y) {
                        const int canvasY = ov0 + y;
                        const int srcY = canvasY - rOff[i] + vR1[i];
                        for (int x = 0; x < blkW; ++x) {
                            const int canvasX = c0 + x;
                            const int srcX = canvasX - cOff[i] + vC1[i];
                            const uint16_t raw =
                                imgs[static_cast<size_t>(i)][static_cast<size_t>(srcY * widths[static_cast<size_t>(i)] + srcX)];
                            blk[static_cast<size_t>(y * blkW + x)] =
                                static_cast<float>(mapPixel(packs[static_cast<size_t>(i)], raw, srcY, srcX));
                        }
                    }

                    // 权重
                    std::vector<float> wN(static_cast<size_t>(blkH * blkW), 1.f);
                    std::vector<float> wW(static_cast<size_t>(blkH * blkW), 1.f);
                    for (int y = 0; y < blkH; ++y) {
                        const int rowAbs = ov0 + y + 1; // 1-based 友好
                        float sL = seamC_L[static_cast<size_t>(i)];
                        float sR = seamC_R[static_cast<size_t>(i)];
                        if (hasPathL[static_cast<size_t>(i)])
                            sL = pathAtRow(pathL[static_cast<size_t>(i)], rowAbs);
                        if (hasPathR[static_cast<size_t>(i)])
                            sR = pathAtRow(pathR[static_cast<size_t>(i)], rowAbs);
                        for (int x = 0; x < blkW; ++x) {
                            const float pos = static_cast<float>(c0 + x + 1);
                            float wn = 1.f, ww = 1.f;
                            if (std::isfinite(sL)) {
                                wn *= ramp01((pos - sL) / featherPix + 0.5f);
                                ww *= ramp01((pos - sL) / featherWide + 0.5f);
                            }
                            if (std::isfinite(sR)) {
                                wn *= ramp01((sR - pos) / featherPix + 0.5f);
                                ww *= ramp01((sR - pos) / featherWide + 0.5f);
                            }
                            wn = std::max(wn, 1e-4f);
                            ww = std::max(ww, 1e-4f);
                            if (blk[static_cast<size_t>(y * blkW + x)] <= 0) {
                                wn = 0;
                                ww = 0;
                            }
                            wN[static_cast<size_t>(y * blkW + x)] = wn;
                            wW[static_cast<size_t>(y * blkW + x)] = ww;
                        }
                    }

                    // 低频带
                    std::vector<float> lowB;
                    lowBand(blk, blkH, blkW, featherWide, lowB);
                    for (int y = 0; y < blkH; ++y) {
                        const int ia = ov0 - rowA + y;
                        for (int x = 0; x < blkW; ++x) {
                            const int xx = c0 + x;
                            const size_t dst = static_cast<size_t>(ia * canvasW + xx);
                            const size_t src = static_cast<size_t>(y * blkW + x);
                            const float lo = lowB[src];
                            const float hi = blk[src] - lo;
                            accL[dst] += lo * wW[src];
                            wL[dst] += wW[src];
                            accH[dst] += hi * wN[src];
                            wH[dst] += wN[src];
                        }
                    }
                }

                for (int y = 0; y < nR; ++y) {
                    int first = -1, last = -1;
                    for (int x = 0; x < canvasW; ++x) {
                        const size_t idx = static_cast<size_t>(y * canvasW + x);
                        const bool m = (wL[idx] > 0) || (wH[idx] > 0);
                        if (!m) continue;
                        if (first < 0) first = x;
                        last = x;
                        const double v = static_cast<double>(accL[idx] / std::max(wL[idx], 1e-6f)
                                                             + accH[idx] / std::max(wH[idx], 1e-6f));
                        outPix[static_cast<size_t>((rowA + y) * canvasW + x)] =
                            static_cast<uint16_t>(clampv(std::round(v), 0.0, 65535.0));
                    }
                    // fillEmpty：行内两侧空洞用最近有效像素补齐
                    if (first >= 0) {
                        for (int x = 0; x < first; ++x)
                            outPix[static_cast<size_t>((rowA + y) * canvasW + x)] =
                                outPix[static_cast<size_t>((rowA + y) * canvasW + first)];
                        for (int x = last + 1; x < canvasW; ++x)
                            outPix[static_cast<size_t>((rowA + y) * canvasW + x)] =
                                outPix[static_cast<size_t>((rowA + y) * canvasW + last)];
                    }
                }
                if (progress && (s % 4 == 0 || s + 1 == nStrip))
                    progress(60 + 25 * (s + 1) / nStrip, "多带融合条带 " + std::to_string(s + 1) + "/" + std::to_string(nStrip));
            }
        });
    }
    for (auto &th : pool) th.join();
    if (failFlag.load() || (cancelled && cancelled())) return false;

    for (int y = 0; y < canvasH; y += 8)
        for (int x = 0; x < canvasW; x += 8)
            sampAll.push_back(outPix[static_cast<size_t>(y * canvasW + x)]);

    result.mosaicTif = pathJoin(outDir, "MOSAIC.tif");
    if (!writeTiffU16(result.mosaicTif, static_cast<uint32_t>(canvasW), static_cast<uint32_t>(canvasH),
                      outPix.data(), err))
        return false;

    // ★纯值支路：MOSAIC_RCS.tif + MOSAIC_L3.txt（不受显示链/匀色影响）
    if (!exportPureValue(paths, widths, heights, rOff, cOff, vR1, vC1, Nr, Na, seamC_L, seamC_R,
                         canvasH, canvasW, outDir, result, progress, cancelled, err))
        return false;

    // 拼后显示版 MOSAIC_显示.tif（原值不动）
    if (progress) progress(88, "写出显示版…");
    {
        std::vector<double> vv;
        vv.reserve(sampAll.size());
        for (uint16_t v : sampAll)
            if (v > 0) vv.push_back(v);
        double lo = 0, hi = 65535, med = 0;
        if (!vv.empty()) {
            std::vector<double> a = vv, b = vv, c = vv;
            lo = percentileSorted(a, postPctLo);
            hi = percentileSorted(b, postPctHi);
            med = percentileSorted(c, 50);
        }
        if (hi <= lo) hi = lo + 1;
        const double xm = clampv((med - lo) / (hi - lo), 1e-6, 1.0);
        const double gGain = (postMedTarget > 0) ? (postMedTarget / std::max(std::pow(xm, postGamma), 1e-6)) : 1.0;
        std::vector<uint16_t> lut(65536);
        for (int x = 0; x < 65536; ++x) {
            const double t = clampv((x - lo) / (hi - lo), 0.0, 1.0);
            lut[static_cast<size_t>(x)] =
                static_cast<uint16_t>(clampv(65535.0 * gGain * std::pow(t, postGamma), 0.0, 65535.0));
        }
        std::vector<uint16_t> disp = outPix;
        {
            std::atomic<size_t> next{0};
            std::vector<std::thread> pool2;
            for (int t = 0; t < hwThreads(); ++t) {
                pool2.emplace_back([&]() {
                    for (;;) {
                        const size_t i = next.fetch_add(4096);
                        if (i >= disp.size()) return;
                        const size_t end = std::min(disp.size(), i + 4096);
                        for (size_t k = i; k < end; ++k)
                            disp[k] = lut[disp[k]];
                    }
                });
            }
            for (auto &th : pool2) th.join();
        }
        result.displayTif = pathJoin(outDir, "MOSAIC_显示.tif");
        if (!writeTiffU16(result.displayTif, static_cast<uint32_t>(canvasW), static_cast<uint32_t>(canvasH),
                          disp.data(), err))
            return false;
    }

    if (doFlatten) {
        if (progress) progress(92, "匀光…");
        result.flattenTif = pathJoin(outDir, "MOSAIC_匀光.tif");
        if (!MosaicAlgorithm::flattenTiff(result.mosaicTif, result.flattenTif, medOut, result.seamCols, progress, err))
            return false;
    }
    if (progress) progress(100, "拼接完成");
    return true;
}


} // namespace


std::vector<std::string> MosaicAlgorithm::listTiffFiles(const std::string &folder)
{
    return listTiffs(folder);
}

bool MosaicAlgorithm::prepareInputFiles(const std::string &inFolder, const std::string &outFolder,
                                        std::vector<std::string> &tiffPaths,
                                        std::function<void(int, const std::string &)> progress,
                                        std::function<bool()> cancelled,
                                        std::string *err,
                                        int rawNrHint, int rawNaHint)
{
    tiffPaths.clear();
    if (!isDir(inFolder)) {
        if (err) *err = std::string("找不到文件夹: ") + inFolder;
        return false;
    }
    std::vector<std::string> tifs, raws;
    listInputs(inFolder, tifs, raws);

    if (!tifs.empty()) {
        tiffPaths = tifs;
        if (progress) {
            progress(2, "输入 TIFF " + std::to_string(tifs.size()) + " 张 → 直接拼接");
            if (!raws.empty())
                progress(2, "（同目录还有 " + std::to_string(raws.size()) + " 个 RAW，已忽略）");
        }
        return true;
    }
    if (raws.empty()) {
        if (err) *err = std::string("文件夹里既没有 TIFF 也没有 RAW: ") + inFolder;
        return false;
    }

    const std::string outDir = resolveOutDir(outFolder, inFolder, mosaicResultTag());
    const std::string workFolder = pathJoin(outDir, "raw2tif");
    makeDirs(workFolder);
    if (progress)
        progress(1, "仅有 RAW " + std::to_string(raws.size()) + " 个 → 自动转 TIFF（" + workFolder + "）");

    int nrPrefer = 0;
    tiffPaths.reserve(raws.size());
    for (int i = 0; i < raws.size(); ++i) {
        if (cancelled && cancelled()) return false;
        int Nr = 0, Na = 0;
        std::string how;
        if (!detectRawDims(raws[i], rawNrHint, rawNaHint, nrPrefer, Nr, Na, how, err))
            return false;
        if (i == 0) {
            nrPrefer = Nr;
            if (progress)
                progress(2, "RAW 尺寸: 行=" + std::to_string(Nr) + " 列=" + std::to_string(Na) + " [" + how + "]");
        }
        const std::string base = fileStem(raws[static_cast<size_t>(i)]);
        const std::string outTif = pathJoin(workFolder, base + ".tif");
        const int64_t needBytes = static_cast<int64_t>(Nr) * Na * 2;
        bool okReuse = false;
        const int64_t existingSize = fileSizeBytes(outTif);
        const bool existingExists = existingSize >= 0 && pathExists(outTif);
        if (existingExists && existingSize >= needBytes && tiffSizeMatches(outTif, Nr, Na))
            okReuse = true;
        if (okReuse) {
            if (progress)
                progress(3 + i * 10 / std::max(1, static_cast<int>(raws.size())), "[" + std::to_string(i + 1) + "/" + std::to_string(raws.size()) + "] " + base + " → 复用已有 TIFF");
        } else {
            if (progress)
                progress(3 + i * 10 / std::max(1, static_cast<int>(raws.size())), "[" + std::to_string(i + 1) + "/" + std::to_string(raws.size()) + "] " + base + " (" + std::to_string(Nr) + "×" + std::to_string(Na) + ") → 转 TIFF…");
            if (!rawToTiffBe(raws[i], Nr, Na, outTif, err))
                return false;
        }
        tiffPaths.push_back(outTif);
    }
    return true;
}

bool MosaicAlgorithm::loadGrayPreview(const std::string &path, int step, GrayPreview &preview,
                                      std::string *err)
{
    std::vector<int32_t> pix;
    uint32_t w = 0, h = 0;
    bool isSigned = false;
    if (!readTiff16AsI32(path, pix, w, h, isSigned, err))
        return false;
    preview.fullW = static_cast<int>(w);
    preview.fullH = static_cast<int>(h);
    step = std::max(1, step);
    const int ow = (preview.fullW + step - 1) / step;
    const int oh = (preview.fullH + step - 1) / step;
    preview.width = ow;
    preview.height = oh;
    preview.pixels.assign(static_cast<size_t>(ow * oh), 0);

    // 定标 int16：先按 2% 分位拉黑场（与 MATLAB readPreview）
    std::vector<double> sample;
    sample.reserve(std::min(200000, static_cast<int>(pix.size())));
    const int stride = std::max(1, static_cast<int>(pix.size() / 100000));
    for (size_t i = 0; i < pix.size(); i += static_cast<size_t>(stride))
        sample.push_back(static_cast<double>(pix[i]));
    std::sort(sample.begin(), sample.end());
    double ped = 0;
    if (isSigned || (!sample.empty() && sample.front() < 0)) {
        ped = sample[static_cast<size_t>(std::max(0, static_cast<int>(0.02 * (sample.size() - 1))))];
        for (double &v : sample)
            v = std::max(v - ped, 0.0);
    }
    const double lo =
        sample.empty() ? 0.0
                       : sample[static_cast<size_t>(static_cast<int>(0.005 * (sample.size() - 1)))];
    const double hi =
        sample.empty() ? 1.0
                       : sample[static_cast<size_t>(static_cast<int>(0.995 * (sample.size() - 1)))];
    const double span = std::max(hi - lo, 1.0);
    for (int y = 0; y < oh; ++y) {
        for (int x = 0; x < ow; ++x) {
            double v = static_cast<double>(pix[static_cast<size_t>((y * step) * w + (x * step))]) - ped;
            if (v < 0)
                v = 0;
            preview.pixels[static_cast<size_t>(y * ow + x)] =
                static_cast<uint8_t>(clampi(static_cast<int>((v - lo) / span * 255.0), 0, 255));
        }
    }
    return true;
}

bool MosaicAlgorithm::run(const std::string &inFolder, const std::string &outFolder,
                          bool doFlatten, double medOut,
                          MosaicResult &result,
                          std::function<void(int, const std::string &)> progress,
                          std::function<bool()> cancelled,
                          std::string *err,
                          int rawNrHint, int rawNaHint)
{
    std::vector<std::string> paths;
    if (!prepareInputFiles(inFolder, outFolder, paths, progress, cancelled, err, rawNrHint, rawNaHint))
        return false;
    if (paths.size() < 2) {
        if (err) *err = "至少需要2张图像（TIFF 或 RAW）";
        return false;
    }
    std::string outDir = resolveOutDir(outFolder, inFolder, mosaicResultTag());
    makeDirs(outDir);

    const int n = static_cast<int>(paths.size());
    std::vector<std::vector<uint16_t>> imgs;
    std::vector<uint32_t> widths, heights;
    double dbLo = 0;
    if (!loadDisplayImgs(paths, imgs, widths, heights, dbLo, progress, cancelled, err))
        return false;
    (void)dbLo;

    std::vector<int> offC(static_cast<size_t>(n), 0), offR(static_cast<size_t>(n), 0);
    for (int i = 1; i < n; ++i) {
        if (cancelled && cancelled()) return false;
        if (progress) progress(18 + i * 12 / n, "结构配准 " + std::to_string(i) + "/" + std::to_string(n - 1) + "…");
        int dR = 0, dC = 0;
        double ncc = -1;
        estimatePairOffsetCol(
            imgs[static_cast<size_t>(i - 1)], widths[static_cast<size_t>(i - 1)], heights[static_cast<size_t>(i - 1)],
            imgs[static_cast<size_t>(i)], widths[static_cast<size_t>(i)], heights[static_cast<size_t>(i)],
            dR, dC, ncc, cancelled);
        if (cancelled && cancelled()) return false;
        // 与 MATLAB nccThresh=0.12：相关偏低仍按估计值放置，只提示可疑
        const double nccThresh = 0.12;
        if (progress) {
            const int wa = static_cast<int>(widths[static_cast<size_t>(i - 1)]);
            const int ha = static_cast<int>(heights[static_cast<size_t>(i - 1)]);
            const double ovlR = 100.0 * (ha - std::abs(dR)) / std::max(ha, 1);
            const double ovlC = 100.0 * (wa - std::abs(dC)) / std::max(wa, 1);
            std::ostringstream oss;
            oss << "第" << i << "→" << (i + 1) << ": Δ行=" << dR << " Δ列=" << dC << " NCC="
                << std::fixed << std::setprecision(3) << ncc << " (重叠 行" << std::setprecision(0) << ovlR
                << "% 列" << ovlC << "%)";
            std::string msg = oss.str();
            if (ncc < nccThresh)
                msg += " ⚠可信度低，仍按估计值放置";
            if (ovlR < 25.0 || ovlC < 25.0)
                msg += " ⚠重叠偏低";
            progress(18 + i * 12 / n, msg);
        }
        offR[i] = offR[i - 1] + dR;
        offC[i] = offC[i - 1] + dC;
    }

    return fuseAndWrite(imgs, widths, heights, offR, offC, paths, outDir, doFlatten, medOut,
                        result, progress, cancelled, err);
}

bool MosaicAlgorithm::runWithOffsets(const std::string &inFolder, const std::string &outFolder,
                                     const std::vector<int> &offR, const std::vector<int> &offC,
                                     bool doFlatten, double medOut,
                                     MosaicResult &result,
                                     std::function<void(int, const std::string &)> progress,
                                     std::function<bool()> cancelled,
                                     std::string *err,
                                     int rawNrHint, int rawNaHint)
{
    std::vector<std::string> paths;
    if (!prepareInputFiles(inFolder, outFolder, paths, progress, cancelled, err, rawNrHint, rawNaHint))
        return false;
    if (paths.size() < 2) {
        if (err) *err = "至少需要2张图像（TIFF 或 RAW）";
        return false;
    }
    if (offR.size() != paths.size() || offC.size() != paths.size()) {
        if (err) *err = "手动偏移数量与图像数不一致";
        return false;
    }
    std::string outDir = resolveOutDir(outFolder, inFolder, mosaicResultTag());
    makeDirs(outDir);

    std::vector<std::vector<uint16_t>> imgs;
    std::vector<uint32_t> widths, heights;
    double dbLo = 0;
    if (!loadDisplayImgs(paths, imgs, widths, heights, dbLo, progress, cancelled, err))
        return false;
    (void)dbLo;
    return fuseAndWrite(imgs, widths, heights, offR, offC, paths, outDir, doFlatten, medOut,
                        result, progress, cancelled, err);
}

bool MosaicAlgorithm::flattenTiff(const std::string &inTif, const std::string &outTif, double medOut,
                                  const std::vector<int> & /*seamCols*/,
                                  std::function<void(int, const std::string &)> progress,
                                  std::string *err)
{
    std::vector<uint16_t> img;
    uint32_t W = 0, H = 0;
    if (!readTiffU16(inTif, img, W, H, err))
        return false;
    if (progress) progress(92, "块中值仿射匀光…");

    const int blk = 128;
    const int gw = (static_cast<int>(W) + blk - 1) / blk;
    const int gh = (static_cast<int>(H) + blk - 1) / blk;
    std::vector<double> med(static_cast<size_t>(gw * gh), 0.0);
    {
        std::atomic<int> next{0};
        std::vector<std::thread> pool;
        for (int t = 0; t < hwThreads(); ++t) {
            pool.emplace_back([&]() {
                for (;;) {
                    const int by = next.fetch_add(1);
                    if (by >= gh) return;
                    for (int bx = 0; bx < gw; ++bx) {
                        std::vector<double> vals;
                        for (int y = by * blk; y < std::min((by + 1) * blk, static_cast<int>(H)); ++y)
                            for (int x = bx * blk; x < std::min((bx + 1) * blk, static_cast<int>(W)); ++x)
                                vals.push_back(img[static_cast<size_t>(y * W + x)]);
                        if (vals.empty()) continue;
                        std::nth_element(vals.begin(), vals.begin() + vals.size() / 2, vals.end());
                        med[static_cast<size_t>(by * gw + bx)] = vals[vals.size() / 2];
                    }
                }
            });
        }
        for (auto &th : pool) th.join();
    }
    double medRef = 0;
    for (double v : med) medRef += v;
    medRef /= std::max<size_t>(1, med.size());
    if (medRef < 1) medRef = 1;

    std::vector<uint16_t> out(img.size());
    std::vector<double> all;
    all.reserve(static_cast<int>(img.size() / 16));
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            const int bx = std::min(gw - 1, static_cast<int>(x) / blk);
            const int by = std::min(gh - 1, static_cast<int>(y) / blk);
            const double m = med[static_cast<size_t>(by * gw + bx)];
            const double a = medRef / std::max(m, 1.0);
            double v = img[static_cast<size_t>(y * W + x)] * a;
            out[static_cast<size_t>(y * W + x)] = static_cast<uint16_t>(clampv(v, 0.0, 65535.0));
            if (((x + y) & 15) == 0) all.push_back(v);
        }
    }
    std::sort(all.begin(), all.end());
    const double lo = all[static_cast<size_t>(static_cast<int>(0.005 * (all.size() - 1)))];
    const double hi = all[static_cast<size_t>(static_cast<int>(0.995 * (all.size() - 1)))];
    const double span = std::max(hi - lo, 1.0);
    std::vector<double> mapped;
    mapped.reserve(all.size());
    for (double v : all) mapped.push_back(clampv( (v - lo) / span, 0.0, 1.0));
    std::sort(mapped.begin(), mapped.end());
    const double x50 = std::max(mapped[mapped.size() / 2], 1e-6);
    const double gamma = std::log(clampv(medOut, 0.02, 0.98)) / std::log(x50);

    {
        std::atomic<size_t> next{0};
        std::vector<std::thread> pool;
        for (int t = 0; t < hwThreads(); ++t) {
            pool.emplace_back([&]() {
                for (;;) {
                    const size_t i = next.fetch_add(4096);
                    if (i >= out.size()) return;
                    const size_t end = std::min(out.size(), i + 4096);
                    for (size_t k = i; k < end; ++k) {
                        double tt = clampv( (out[k] - lo) / span, 0.0, 1.0);
                        tt = std::pow(tt, gamma);
                        out[k] = static_cast<uint16_t>(clampv(tt * 65535.0, 0.0, 65535.0));
                    }
                }
            });
        }
        for (auto &th : pool) th.join();
    }
    return writeTiffU16(outTif, W, H, out.data(), err);
}
