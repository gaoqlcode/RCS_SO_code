#include "BinaryIo.h"
#include "Level0Dat.h"
#include "OutPath.h"
#include "Util.hpp"
#include "rcs/rcs_api.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <regex>
#include <set>

namespace {

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

bool matchL0Pol(const std::string &name, const std::string &pol)
{
    const std::string u = toUpperStr(name);
    const std::string needle = std::string("_L0_") + toUpperStr(trimStr(pol)) + ".DAT";
    if (u.size() < needle.size())
        return false;
    return u.compare(u.size() - needle.size(), needle.size(), needle) == 0;
}

std::string baseNameOf(const std::string &path)
{
    const size_t p1 = path.find_last_of('/');
    const size_t p2 = path.find_last_of('\\');
    size_t p = std::string::npos;
    if (p1 == std::string::npos)
        p = p2;
    else if (p2 == std::string::npos)
        p = p1;
    else
        p = (p1 > p2) ? p1 : p2;
    return (p == std::string::npos) ? path : path.substr(p + 1);
}

bool loadRawU16Preview(const std::string &path, int nr, int na, int step, GrayImage &g,
                       std::string *err)
{
    if (nr <= 0 || na <= 0) {
        if (err)
            *err = "nr/na 无效";
        return false;
    }
    if (step < 1)
        step = 8;
    const int64_t need = static_cast<int64_t>(nr) * na * 2;
    std::vector<uint8_t> bytes;
    if (!readFileAll(path, bytes) || static_cast<int64_t>(bytes.size()) < need) {
        if (err)
            *err = "无法读取 RAW 或尺寸不符: " + path;
        return false;
    }
    // 磁盘行主序 [Nr][Na]，与 processSigma0 / MATLAB fread([Na,Nr])' 一致
    std::vector<uint16_t> img(static_cast<size_t>(nr) * static_cast<size_t>(na));
    for (size_t i = 0; i < img.size(); ++i) {
        const size_t off = i * 2;
        img[i] = static_cast<uint16_t>(bytes[off] | (bytes[off + 1] << 8));
    }
    g.height = (nr + step - 1) / step;
    g.width = (na + step - 1) / step;
    const size_t nPix = static_cast<size_t>(g.width) * static_cast<size_t>(g.height);
    g.pixels.assign(nPix, 0);
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
        const size_t i = static_cast<size_t>(
            std::max(0.0, std::min(p / 100.0 * (sorted.size() - 1),
                                   static_cast<double>(sorted.size() - 1))));
        return sorted[i];
    };
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
        if (t < 0.0)
            t = 0.0;
        if (t > 1.0)
            t = 1.0;
        g.pixels[i] = static_cast<uint8_t>(
            std::max(0, std::min(255, static_cast<int>(t * 255.0 + 0.5))));
    }
    return true;
}

} // namespace

bool listL0Files(const std::string &folder, const std::string &pol,
                 std::vector<std::string> &outNames, std::string *err)
{
    outNames.clear();
    if (!isDir(folder)) {
        if (err)
            *err = "文件夹无效";
        return false;
    }
    const std::string p = toUpperStr(trimStr(pol));
    std::vector<std::string> names = listFileNames(folder);
    for (size_t i = 0; i < names.size(); ++i) {
        if (matchL0Pol(names[i], p))
            outNames.push_back(names[i]);
    }
    std::sort(outNames.begin(), outNames.end(), [](const std::string &a, const std::string &b) {
        return firstMultiDigit(a) < firstMultiDigit(b);
    });
    if (outNames.empty()) {
        if (err)
            *err = "未找到 *_L0_" + p + ".dat";
        return false;
    }
    return true;
}

bool listAvailablePolarizations(const std::string &folder, std::vector<std::string> &outPols,
                                std::string *err)
{
    outPols.clear();
    if (!isDir(folder)) {
        if (err)
            *err = "文件夹无效";
        return false;
    }
    const char *cands[] = {"HH", "HV", "VV", "VH"};
    std::set<std::string> found;
    std::vector<std::string> names = listFileNames(folder);
    for (size_t i = 0; i < names.size(); ++i) {
        for (int k = 0; k < 4; ++k) {
            if (matchL0Pol(names[i], cands[k]))
                found.insert(cands[k]);
        }
    }
    for (int k = 0; k < 4; ++k) {
        if (found.count(cands[k]))
            outPols.push_back(cands[k]);
    }
    if (outPols.empty()) {
        if (err)
            *err = "目录中未发现 *_L0_*.dat";
        return false;
    }
    return true;
}

bool probeL0File(const std::string &path, FileHeaderInfo &hdr, FrameMeta &frame0, std::string *err)
{
    hdr = FileHeaderInfo();
    frame0 = FrameMeta();
    std::vector<uint8_t> all;
    if (!readFileAll(path, all) || all.size() < 512) {
        if (err)
            *err = "无法读取文件或头不足 512B";
        return false;
    }
    std::vector<uint8_t> hdr512(all.begin(), all.begin() + 512);
    if (!Level0Dat::parseHeader(hdr512, hdr, err))
        return false;
    const int lenFileH = hdr.lenFileH > 0 ? hdr.lenFileH : 512;
    const int lenFrmH = hdr.lenFrmH > 0 ? hdr.lenFrmH : 256;
    if (static_cast<int64_t>(all.size()) < lenFileH + lenFrmH) {
        if (err)
            *err = "文件太短，读不到帧头";
        return false;
    }
    const uint8_t *fh = all.data() + lenFileH;
    frame0.N = readU32LE(fh + 16);
    frame0.PRT = readU32LE(fh + 20);
    frame0.tauN = readU32LE(fh + 24);
    frame0.dlyN = readU32LE(fh + 28);
    frame0.azRaw = readU16LE(fh + 32);
    frame0.rollRaw = readI16LE(fh + 36);
    return true;
}

bool probeDataFolder(const std::string &folder, const std::string &pol, L0FolderProbe &out,
                     std::string *err)
{
    out = L0FolderProbe();
    out.pol = toUpperStr(trimStr(pol));
    std::vector<std::string> names;
    if (!listL0Files(folder, out.pol, names, err))
        return false;
    out.fileCount = static_cast<int>(names.size());
    out.firstFile = names.front();
    out.lastFile = names.back();
    FrameMeta fm;
    const std::string path = pathJoin(folder, out.firstFile);
    if (probeL0File(path, out.firstHeader, fm, err))
        out.hasHeader = true;
    else {
        // 列表成功但头失败：仍返回列表信息
        if (err)
            err->clear();
        out.hasHeader = false;
    }
    return true;
}

bool parseImagedRawSize(const std::string &pathOrName, int &nr, int &na, std::string *err)
{
    nr = 0;
    na = 0;
    const std::string name = baseNameOf(pathOrName);
    // Image_Nr2048Na4602... 或 ...Nr2048Na4602...
    static const std::regex re("Nr(\\d+)Na(\\d+)", std::regex::icase);
    std::smatch m;
    if (!std::regex_search(name, m, re) || m.size() < 3) {
        if (err)
            *err = "文件名中未找到 Nr/Na（例 Image_Nr2048Na4602....raw）";
        return false;
    }
    nr = std::stoi(m[1].str());
    na = std::stoi(m[2].str());
    if (nr <= 0 || na <= 0) {
        if (err)
            *err = "解析到的 nr/na 无效";
        return false;
    }
    return true;
}

bool defaultOutDir(const std::string &dataFolder, const std::string &tag, std::string &outDir,
                   std::string *err)
{
    outDir.clear();
    if (trimStr(dataFolder).empty() || trimStr(tag).empty()) {
        if (err)
            *err = "dataFolder/tag 不能为空";
        return false;
    }
    outDir = defaultResultDir(dataFolder, tag);
    return !outDir.empty();
}

bool loadImagedRawPreview(const std::string &rawPath, int nr, int na, int step, GrayImage &preview,
                          std::string *err)
{
    preview = GrayImage();
    return loadRawU16Preview(rawPath, nr, na, step, preview, err);
}

bool mapPreviewRoiToFull(int previewW, int previewH, int fullNr, int fullNa,
                         const RectRoi &previewRoi, RectRoi &fullRoi, std::string *err)
{
    if (previewW <= 0 || previewH <= 0 || fullNr <= 0 || fullNa <= 0 || !previewRoi.valid()) {
        if (err)
            *err = "预览/全分辨率尺寸或 ROI 无效";
        return false;
    }
    const double sx = static_cast<double>(fullNa) / previewW;
    const double sy = static_cast<double>(fullNr) / previewH;
    fullRoi.x = static_cast<int>(previewRoi.x * sx);
    fullRoi.y = static_cast<int>(previewRoi.y * sy);
    fullRoi.w = std::max(1, static_cast<int>(previewRoi.w * sx));
    fullRoi.h = std::max(1, static_cast<int>(previewRoi.h * sy));
    if (fullRoi.x < 0)
        fullRoi.x = 0;
    if (fullRoi.y < 0)
        fullRoi.y = 0;
    if (fullRoi.x + fullRoi.w > fullNa)
        fullRoi.w = fullNa - fullRoi.x;
    if (fullRoi.y + fullRoi.h > fullNr)
        fullRoi.h = fullNr - fullRoi.y;
    return fullRoi.valid();
}

bool mapFullPointToPreview(int previewW, int previewH, int fullNr, int fullNa, int fullAz,
                           int fullRg, int &prevX, int &prevY, std::string *err)
{
    if (previewW <= 0 || previewH <= 0 || fullNr <= 0 || fullNa <= 0) {
        if (err)
            *err = "尺寸无效";
        return false;
    }
    prevX = static_cast<int>(fullAz * (static_cast<double>(previewW) / fullNa));
    prevY = static_cast<int>(fullRg * (static_cast<double>(previewH) / fullNr));
    prevX = ibound(0, prevX, previewW - 1);
    prevY = ibound(0, prevY, previewH - 1);
    return true;
}

bool listImagedRawFiles(const std::string &folder, std::vector<std::string> &outNames,
                        std::string *err)
{
    outNames.clear();
    if (!isDir(folder)) {
        if (err)
            *err = "文件夹无效";
        return false;
    }
    std::vector<std::string> names = listFileNames(folder);
    for (size_t i = 0; i < names.size(); ++i) {
        const std::string u = toUpperStr(names[i]);
        if (u.size() < 4 || u.compare(u.size() - 4, 4, ".RAW") != 0)
            continue;
        int nr = 0, na = 0;
        if (!parseImagedRawSize(names[i], nr, na, 0))
            continue;
        outNames.push_back(names[i]);
    }
    std::sort(outNames.begin(), outNames.end());
    if (outNames.empty()) {
        if (err)
            *err = "未找到可解析 Nr/Na 的 .raw";
        return false;
    }
    return true;
}
