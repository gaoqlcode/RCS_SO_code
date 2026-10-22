#include "Level2Pdat.h"
#include "Util.hpp"
#include "rcs/rcs_api.h"

#include <iconv.h>

#include <cstdint>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <queue>
#include <regex>
#include <sstream>
#include <vector>

namespace {

const double kC = 299792458.0;
const int kL3HdrBytes = 5252;

bool cancelled(Callbacks &cb) { return cb.isCancelled && cb.isCancelled(); }
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

double beamWidthDeg(const std::string &band)
{
    const std::string b = toUpperStr(trimStr(band));
    if (b == "W")
        return 7.0;
    return 10.0;
}

bool parseNrNa(const std::string &name, int &nr, int &na)
{
    static const std::regex re("Nr(\\d+)Na(\\d+)", std::regex::icase);
    std::smatch m;
    if (!std::regex_search(name, m, re) || m.size() < 3)
        return false;
    nr = std::stoi(m[1].str());
    na = std::stoi(m[2].str());
    return nr > 0 && na > 0;
}

std::string baseName(const std::string &path)
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
    std::string n = (p == std::string::npos) ? path : path.substr(p + 1);
    const size_t dot = n.find_last_of('.');
    if (dot != std::string::npos)
        n = n.substr(0, dot);
    return n;
}

bool loadRawU16LE(const std::string &path, int nr, int na, std::vector<uint16_t> &out, std::string *err)
{
    const int64_t need = static_cast<int64_t>(nr) * na * 2;
    const int64_t sz = fileSizeBytes(path);
    if (sz < need) {
        if (err)
            *err = "RAW 太小或尺寸不符: " + path;
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!readFileAll(path, bytes) || static_cast<int64_t>(bytes.size()) < need) {
        if (err)
            *err = "无法读取 RAW: " + path;
        return false;
    }
    out.resize(static_cast<size_t>(nr) * static_cast<size_t>(na));
    for (size_t i = 0; i < out.size(); ++i) {
        const size_t off = i * 2;
        out[i] = static_cast<uint16_t>(bytes[off] | (bytes[off + 1] << 8));
    }
    return true;
}

GrayImage makePreview(const std::vector<uint16_t> &img, int nr, int na, int step)
{
    GrayImage g;
    if (step < 1)
        step = 8;
    g.height = (nr + step - 1) / step;
    g.width = (na + step - 1) / step;
    g.pixels.assign(static_cast<size_t>(g.width) * g.height, 0);
    std::vector<uint16_t> samp;
    samp.reserve(static_cast<size_t>(g.width) * g.height);
    for (int y = 0; y < nr; y += step)
        for (int x = 0; x < na; x += step)
            samp.push_back(img[static_cast<size_t>(y) * na + x]);
    if (samp.empty())
        return g;
    std::vector<uint16_t> sorted = samp;
    std::sort(sorted.begin(), sorted.end());
    const uint16_t lo = sorted[sorted.size() / 100];
    const uint16_t hi = sorted[std::min(sorted.size() - 1, sorted.size() * 999 / 1000)];
    const double span = std::max(1.0, static_cast<double>(hi) - lo);
    for (size_t i = 0; i < samp.size(); ++i) {
        double t = (static_cast<double>(samp[i]) - lo) / span;
        if (t < 0)
            t = 0;
        if (t > 1)
            t = 1;
        g.pixels[i] = static_cast<uint8_t>(t * 255.0 + 0.5);
    }
    return g;
}

bool clampRoi(RectRoi &r, int nr, int na)
{
    if (r.w <= 0 || r.h <= 0)
        return false;
    if (r.x < 0)
        r.x = 0;
    if (r.y < 0)
        r.y = 0;
    if (r.x >= na || r.y >= nr)
        return false;
    if (r.x + r.w > na)
        r.w = na - r.x;
    if (r.y + r.h > nr)
        r.h = nr - r.y;
    return r.w >= 3 && r.h >= 3;
}

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
    if (NA <= 0 || NB < 10) {
        if (err)
            *err = "角反连通域或背景像素不足，请重选缓冲区";
        return false;
    }
    eps = sumPA - (static_cast<double>(NA) / NB) * sumPB;
    if (eps <= 0) {
        if (err)
            *err = "角反净能量<=0，请重选缓冲区";
        return false;
    }
    return true;
}

bool computeRNear(const RcsCalTifRequest &req, int nr, double Rbin, double bwDeg, double &R_near,
                  std::string *err)
{
    if (req.anchorMode == rcs::RcsCalCenter) {
        const double R_center = req.hFlight / std::cos(req.incAngleDeg * 3.14159265358979323846 / 180.0);
        R_near = R_center - nr * Rbin / 2.0;
        return true;
    }
    if (req.anchorMode == rcs::RcsCalCrKnown) {
        if (req.rCalKnown <= 0 || req.rowCrHint <= 0) {
            if (err)
                *err = "crKnown 需要 rCalKnown 与 rowCrHint";
            return false;
        }
        R_near = req.rCalKnown - (nr - req.rowCrHint) * Rbin;
        return true;
    }
    // lastRow
    const double theta_last = req.incAngleDeg - bwDeg / 2.0;
    R_near = req.hFlight / std::cos(theta_last * 3.14159265358979323846 / 180.0);
    return true;
}

void putU16(std::vector<uint8_t> &b, uint16_t v)
{
    b.push_back(static_cast<uint8_t>(v & 0xff));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
}
void putU32(std::vector<uint8_t> &b, uint32_t v)
{
    putU16(b, static_cast<uint16_t>(v & 0xffff));
    putU16(b, static_cast<uint16_t>((v >> 16) & 0xffff));
}
void putEntry(std::vector<uint8_t> &b, uint16_t tag, uint16_t type, uint32_t count, uint32_t valOrOff)
{
    putU16(b, tag);
    putU16(b, type);
    putU32(b, count);
    putU32(b, valOrOff);
}

// Classic TIFF：灰度 int16、无压缩、整幅一条 strip
bool writeTiffInt16(const std::string &path, int w, int h, const std::vector<int16_t> &pix,
                    const std::string &desc, std::string *err)
{
    if (w <= 0 || h <= 0 || static_cast<size_t>(w) * h != pix.size()) {
        if (err)
            *err = "TIFF 尺寸与像素数不符";
        return false;
    }
    const uint32_t stripBytes = static_cast<uint32_t>(w) * h * 2;
    // IFD after header(8) + image data
    const uint32_t dataOff = 8;
    const uint32_t ifdOff = dataOff + stripBytes;

    std::vector<uint8_t> file;
    file.reserve(static_cast<size_t>(ifdOff) + 256 + desc.size());
    // header
    putU16(file, 0x4949); // II
    putU16(file, 42);
    putU32(file, ifdOff);
    // pixels little-endian int16
    for (size_t i = 0; i < pix.size(); ++i) {
        const uint16_t u = static_cast<uint16_t>(pix[i]);
        putU16(file, u);
    }
    // IFD: 11 entries
    const uint16_t nEnt = 11;
    putU16(file, nEnt);
    putEntry(file, 256, 3, 1, static_cast<uint32_t>(w));           // ImageWidth SHORT
    putEntry(file, 257, 3, 1, static_cast<uint32_t>(h));           // ImageLength
    putEntry(file, 258, 3, 1, 16);                                 // BitsPerSample
    putEntry(file, 259, 3, 1, 1);                                  // Compression none
    putEntry(file, 262, 3, 1, 1);                                  // Photometric MinIsBlack
    putEntry(file, 273, 4, 1, dataOff);                            // StripOffsets LONG
    putEntry(file, 277, 3, 1, 1);                                  // SamplesPerPixel
    putEntry(file, 278, 3, 1, static_cast<uint32_t>(h));           // RowsPerStrip
    putEntry(file, 279, 4, 1, stripBytes);                         // StripByteCounts
    putEntry(file, 339, 3, 1, 2);                                  // SampleFormat = signed int
    // ImageDescription: type ASCII, count=len+1, value=offset if >4
    const uint32_t descOff = ifdOff + 2 + nEnt * 12 + 4;
    const uint32_t descLen = static_cast<uint32_t>(desc.size() + 1);
    putEntry(file, 270, 2, descLen, descOff);
    putU32(file, 0); // next IFD
    // description bytes
    file.insert(file.end(), desc.begin(), desc.end());
    file.push_back(0);

    if (!writeFileAll(path, file.empty() ? 0 : &file[0], file.size())) {
        if (err)
            *err = "无法写 TIFF: " + path;
        return false;
    }
    return true;
}

std::string nowPretty()
{
    const time_t t = time(0);
    struct tm tmbuf;
#if defined(_WIN32)
    localtime_s(&tmbuf, &t);
#else
    localtime_r(&t, &tmbuf);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", tmbuf.tm_year + 1900,
                  tmbuf.tm_mon + 1, tmbuf.tm_mday, tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec);
    return buf;
}

std::string fileTimeStamp(const std::string &path)
{
    // 无可靠 mtime 时用当前时间压缩戳
    (void)path;
    const time_t t = time(0);
    struct tm tmbuf;
#if defined(_WIN32)
    localtime_s(&tmbuf, &t);
#else
    localtime_r(&t, &tmbuf);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d", tmbuf.tm_year + 1900, tmbuf.tm_mon + 1,
                  tmbuf.tm_mday, tmbuf.tm_hour, tmbuf.tm_min);
    return buf;
}

bool nameHasToken(const std::string &name, const std::string &tok)
{
    if (tok.empty())
        return true;
    const std::string u = toUpperStr(name);
    const std::string t = toUpperStr(tok);
    return u.find(t) != std::string::npos;
}

bool endsWithI(const std::string &s, const std::string &suf)
{
    if (s.size() < suf.size())
        return false;
    for (size_t i = 0; i < suf.size(); ++i) {
        unsigned char a = static_cast<unsigned char>(s[s.size() - suf.size() + i]);
        unsigned char b = static_cast<unsigned char>(suf[i]);
        if (std::tolower(a) != std::tolower(b))
            return false;
    }
    return true;
}

std::string polTokenOf(const std::string &name, const std::string &reqPol)
{
    const std::string req = toUpperStr(trimStr(reqPol));
    if (!req.empty())
        return req;
    const std::string u = toUpperStr(name);
    const char *pols[] = {"HH", "HV", "VH", "VV"};
    for (int i = 0; i < 4; ++i) {
        if (u.find(std::string("_") + pols[i]) != std::string::npos)
            return pols[i];
    }
    return std::string();
}

std::string findL0HeaderFile(const std::string &folder, const std::string &pol)
{
    std::vector<std::string> names = listFileNames(folder);
    std::sort(names.begin(), names.end());
    const std::string polU = toUpperStr(trimStr(pol));
    std::string fallback;
    for (size_t i = 0; i < names.size(); ++i) {
        if (!endsWithI(names[i], ".dat"))
            continue;
        const std::string u = toUpperStr(names[i]);
        if (!polU.empty() && u.find("_" + polU) == std::string::npos)
            continue;
        const std::string full = pathJoin(folder, names[i]);
        if (u.find("_L0_") != std::string::npos)
            return full;
        if (fallback.empty())
            fallback = full;
    }
    return fallback;
}

bool loadL0Heads(const std::string &path, std::vector<uint8_t> &fileHdr, std::vector<uint8_t> &frame0,
                 std::string *err)
{
    std::vector<uint8_t> all;
    if (!readFileAll(path, all) || all.size() < 768) {
        if (err)
            *err = "无法读取 0 级头（需要至少 768 字节）: " + path;
        return false;
    }
    fileHdr.assign(all.begin(), all.begin() + 512);
    frame0.assign(all.begin() + 512, all.begin() + 768);
    return true;
}

std::string pdatPathForTxt(const std::string &txt)
{
    if (endsWithI(txt, ".txt"))
        return txt.substr(0, txt.size() - 4) + ".pdat";
    return txt + ".pdat";
}

void appendNote(std::string &note, const std::string &extra)
{
    if (extra.empty())
        return;
    if (!note.empty())
        note += "; ";
    note += extra;
}

int utf8DispWidth(const std::string &s)
{
    int w = 0;
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::uint32_t cp = c;
        std::size_t n = 1;
        if ((c & 0x80) == 0) {
            n = 1;
            cp = c;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            n = 2;
            cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            n = 3;
            cp = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6)
                 | (static_cast<unsigned char>(s[i + 2]) & 0x3F);
        } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            n = 4;
            cp = ((c & 0x07) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12)
                 | ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6)
                 | (static_cast<unsigned char>(s[i + 3]) & 0x3F);
        }
        w += (cp >= 0x80) ? 2 : 1;
        i += n;
    }
    return w;
}

std::string padRightDisp(const std::string &s, int width)
{
    const int w = utf8DispWidth(s);
    if (w >= width)
        return s;
    return s + std::string(static_cast<std::size_t>(width - w), ' ');
}

std::string padLeftAscii(const std::string &s, int width)
{
    if (static_cast<int>(s.size()) >= width)
        return s;
    return std::string(static_cast<std::size_t>(width - static_cast<int>(s.size())), ' ') + s;
}

bool utf8ToGbk(const std::string &in, std::string &out, std::string *err)
{
    iconv_t cd = ::iconv_open("GBK", "UTF-8");
    if (cd == reinterpret_cast<iconv_t>(-1)) {
        if (err)
            *err = "系统无法把文本转为 GBK";
        return false;
    }
    std::string src = in;
    std::size_t inleft = src.size();
    const std::size_t cap = src.size() * 4 + 32;
    out.assign(cap, '\0');
    char *ip = src.empty() ? 0 : &src[0];
    char *op = &out[0];
    std::size_t oleft = cap;
    const std::size_t rc = ::iconv(cd, &ip, &inleft, &op, &oleft);
    ::iconv_close(cd);
    if (rc == static_cast<std::size_t>(-1)) {
        if (err)
            *err = "三级文本含有 GBK 无法表示的字符";
        return false;
    }
    out.resize(cap - oleft);
    return true;
}

std::string dsarField(const std::string &name, const std::string &value)
{
    return std::string("\t\t") + name + "=" + value + "\r\n";
}

bool clampRoi(const RectRoi &roi, int nr, int na, int &x, int &y, int &w, int &h)
{
    if (!roi.valid() || nr <= 0 || na <= 0)
        return false;
    x = roi.x;
    y = roi.y;
    w = roi.w;
    h = roi.h;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= na || y >= nr)
        return false;
    if (x + w > na)
        w = na - x;
    if (y + h > nr)
        h = nr - y;
    return w > 0 && h > 0;
}

std::string siblingCplx(const std::string &rawPath)
{
    const std::string::size_type slash = rawPath.find_last_of('/');
    const std::string::size_type dot = rawPath.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return std::string();
    const std::string stem = rawPath.substr(0, dot);
    const char *exts[] = {".cplx", ".CPLX"};
    for (int i = 0; i < 2; ++i) {
        const std::string p = stem + exts[i];
        std::ifstream in(p.c_str(), std::ios::binary);
        if (in)
            return p;
    }
    return std::string();
}

bool loadMergeDistances(const std::string &path, int nPts, std::vector<double> &xy, std::string *note)
{
    xy.clear();
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in) {
        if (note)
            *note = "打不开距离文件: " + path;
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '\t' || line[i] == ',')
                line[i] = ' ';
        }
        std::istringstream iss(line);
        double a = 0, b = 0;
        if (!(iss >> a >> b))
            continue;
        xy.push_back(a);
        xy.push_back(b);
    }
    if (static_cast<int>(xy.size() / 2) != nPts) {
        if (note)
            *note = "距离文件行数 " + std::to_string(static_cast<int>(xy.size() / 2)) + " 与点数 "
                    + std::to_string(nPts) + " 不一致，横向/径向留空";
        xy.clear();
        return false;
    }
    return true;
}

bool writeDsar(const std::string &path, int nRow, int nCol, const std::vector<float> &grid,
               const std::vector<double> *xy, const std::string &tgtName, const std::string &pol,
               const std::string &date, const std::string &measureTime, double fsHz, bool sized,
               double stepM, std::string *err)
{
    if (nRow <= 0 || nCol <= 0
        || grid.size() < static_cast<std::size_t>(nRow) * static_cast<std::size_t>(nCol)) {
        if (err)
            *err = "三级数据尺寸无效";
        return false;
    }
    const char *task[] = {"任务名称",     "任务负责人", "任务单位", "任务来源", "数据来源",
                          "数据类型",     "任务执行地点", "任务执行日期", "定标形式", "定标体名称",
                          "定标体尺寸",   "不确定度",   "备注"};
    const char *taskVal[] = {"", "", "", "", "实测原始回波(.raw)辐射定标", "SAR", "", "",
                             "角反射器能量(积分)法", "角反射器", "", "", ""};
    const char *target[] = {"目标名称", "目标说明", "目标分类", "目标长(m)", "目标宽(m)", "目标高(m)",
                            "目标发动机说明", "目标结构特点说明", "目标电磁隐身措施说明",
                            "目标红外隐身措施说明", "备注"};
    const char *system[] = {"设备名称", "设备分类", "工作机制", "发射机峰值功率(W)", "脉冲宽度(μs)",
                            "脉冲重复频率", "天线形式", "天线H面宽(°)", "天线E面宽(°)", "天线增益(dB)",
                            "天线的极化隔离度(dB)", "动态范围(dB)", "接收通道稳定性(dB)",
                            "接收通道非线性度(dB)", "瞬时带宽(GHz)", "角跟踪或随动精度(mrad)",
                            "测距精度(m)", "备注"};
    const char *env[] = {"温度(℃)", "湿度(%)", "气压(hPa)", "能见度(km)", "云况", "备注"};
    const char *cond[] = {"测量时刻", "极化组合", "成像带宽(GHz)", "目标飞行高度(m)", "目标飞行马赫数",
                          "目标飞行动作", "目标发动机工况", "目标真航向(°)", "目标俯仰角(°)",
                          "目标横滚角(°)", "雷达距离(m)", "起始方位视向角(°)", "终止方位视向角(°)",
                          "起始俯仰视向角(°)", "终止俯仰视向角(°)", "脉冲积累个数", "脉冲步进间隔",
                          "横向像素点数", "径向像素点数", "横向尺寸(m)", "径向尺寸(m)", "中心经度(°)",
                          "中心纬度(°)", "目标姿态采样率", "雷达回波采样率", "雷达跟踪采样率", "备注"};

    std::ostringstream hdr;
    hdr << "#DSAR\r\n// 动态SAR数据\r\n// (Dynamic Synthetic Aperture Radar Data Of Target)\r\n";
    hdr << "[TASK]\r\n";
    for (int i = 0; i < 13; ++i) {
        std::string v = taskVal[i];
        if (std::string(task[i]) == "任务执行日期")
            v = date;
        hdr << dsarField(task[i], v);
    }
    hdr << "[/TASK]\r\n[TARGET]\r\n";
    for (int i = 0; i < 11; ++i) {
        std::string v;
        if (std::string(target[i]) == "目标名称")
            v = tgtName;
        hdr << dsarField(target[i], v);
    }
    hdr << "[/TARGET]\r\n[SYSTEM]\r\n";
    for (int i = 0; i < 18; ++i)
        hdr << dsarField(system[i], "");
    hdr << "[/SYSTEM]\r\n[ENVIRONMENT]\r\n";
    for (int i = 0; i < 6; ++i)
        hdr << dsarField(env[i], "");
    hdr << "[/ENVIRONMENT]\r\n[CONDITIONS]\r\n";
    char num[64];
    for (int i = 0; i < 27; ++i) {
        const std::string key = cond[i];
        std::string v;
        if (key == "测量时刻")
            v = measureTime;
        else if (key == "极化组合")
            v = pol;
        else if (key == "横向像素点数")
            v = std::to_string(nCol);
        else if (key == "径向像素点数")
            v = std::to_string(nRow);
        else if (key == "横向尺寸(m)" && sized && nCol > 1) {
            std::snprintf(num, sizeof(num), "%.6f", (nCol - 1) * stepM);
            v = num;
        } else if (key == "径向尺寸(m)" && sized && nRow > 1) {
            std::snprintf(num, sizeof(num), "%.6f", (nRow - 1) * stepM);
            v = num;
        } else if (key == "雷达回波采样率" && fsHz > 0) {
            std::snprintf(num, sizeof(num), "%.6g", fsHz);
            v = num;
        }
        hdr << dsarField(key, v);
    }
    hdr << "[/CONDITIONS]\r\n";

    std::string headerGbk;
    if (!utf8ToGbk(hdr.str(), headerGbk, err))
        return false;
    if (headerGbk.size() < static_cast<std::size_t>(kL3HdrBytes))
        headerGbk.append(static_cast<std::size_t>(kL3HdrBytes) - headerGbk.size(), ' ');

    std::ostringstream data;
    data << "[DATA]\r\n\t\t" << padRightDisp("横向距离(m)", 13) << "\t\t" << padRightDisp("径向距离(m)", 13)
         << "\t\t" << padRightDisp("IMG(dBsm)", 13) << "\t\t" << padRightDisp("相位(°)", 9) << "\r\n";
    const std::string blank13(13, ' ');
    const std::string phase = padLeftAscii("0", 9);
    const bool useXy = xy && xy->size() >= static_cast<std::size_t>(nRow) * nCol * 2;
    for (int c = 0; c < nCol; ++c) {
        for (int r = 0; r < nRow; ++r) {
            const float db = grid[static_cast<std::size_t>(r) * nCol + c];
            std::snprintf(num, sizeof(num), "%.6f", db);
            std::string xs = blank13;
            std::string ys = blank13;
            if (useXy) {
                const std::size_t k = static_cast<std::size_t>(c) * nRow + r;
                char xb[64], yb[64];
                std::snprintf(xb, sizeof(xb), "%.6f", (*xy)[k * 2]);
                std::snprintf(yb, sizeof(yb), "%.6f", (*xy)[k * 2 + 1]);
                xs = padLeftAscii(xb, 13);
                ys = padLeftAscii(yb, 13);
            }
            data << "\t\t" << xs << "\t\t" << ys << "\t\t" << padLeftAscii(num, 13) << "\t\t" << phase
                 << "\r\n";
        }
    }
    data << "[/DATA]\r\n";
    std::string dataGbk;
    if (!utf8ToGbk(data.str(), dataGbk, err))
        return false;
    const std::string all = headerGbk + dataGbk;
    if (!writeFileAll(path, all.data(), all.size())) {
        if (err)
            *err = "无法写三级 txt: " + path;
        return false;
    }
    return true;
}

bool writePositionFile(const std::string &path, const std::string &rawName, int nr, int na, int x,
                       int y, int w, int h, const std::string &l3Name)
{
    const int cx = x + (w - 1) / 2;
    const int cy = y + (h - 1) / 2;
    std::ostringstream os;
    os << "#目标位置\r\n"
       << "中心列=" << cx << "\r\n"
       << "中心行=" << cy << "\r\n"
       << "横向宽度=" << w << "\r\n"
       << "径向高度=" << h << "\r\n"
       << "roi=" << cx << " " << cy << " " << w << " " << h << "\r\n"
       << "原点=左上角，列是方位向，行是距离向，从 0 开始\r\n"
       << "图像文件=" << rawName << "\r\n"
       << "图像尺寸=径向行数" << nr << ", 横向列数" << na << "\r\n"
       << "选框范围=径向行" << y << "~" << (y + h - 1) << ", 横向列" << x << "~" << (x + w - 1) << "\r\n"
       << "对应3级数据文件=" << l3Name << "\r\n";
    std::string gbk;
    if (!utf8ToGbk(os.str(), gbk, 0))
        gbk = os.str();
    return writeFileAll(path, gbk.data(), gbk.size());
}

} // namespace

bool processRcsCalTif(const RcsCalTifRequest &req0, RcsCalTifResponse &resp, Callbacks &cb,
                      std::string *err)
{
    RcsCalTifRequest req = req0;
    // 始终写出 *_RCS_dBsm.tif（DN=dBsm×100）与三级 .txt

    resp = RcsCalTifResponse();
    if (trimStr(req.rawFolder).empty()) {
        if (err)
            *err = "rawFolder 为空";
        return false;
    }
    if (!isDir(req.rawFolder)) {
        if (err)
            *err = "raw 文件夹不存在: " + req.rawFolder;
        return false;
    }
    const std::string outDir =
        trimStr(req.outDir).empty() ? trimStr(req.rawFolder) : trimStr(req.outDir);
    if (!makeDirs(outDir)) {
        if (err)
            *err = "无法创建输出目录: " + outDir;
        return false;
    }

    const double Rbin = kC / (2.0 * (req.fsHz > 0 ? req.fsHz : 1.25e9));
    const double bwDeg = beamWidthDeg(req.band);
    double K = req.K, R_near = req.R_near, RbinUse = req.Rbin > 0 ? req.Rbin : Rbin;
    double R_cal = 0, eps = 0, sigma_cal = std::pow(10.0, req.sigmaTheoryDb / 10.0);
    int peakRow = 0, peakCol = 0;

    if (req.doCalib) {
        progress(cb, 5, "读定标 RAW…");
        if (trimStr(req.calibRawPath).empty()) {
            if (err)
                *err = "calibRawPath 为空";
            return false;
        }
        int nrC = 0, naC = 0;
        if (!parseNrNa(req.calibRawPath, nrC, naC)) {
            if (err)
                *err = "定标 RAW 文件名缺少 Nr…Na…";
            return false;
        }
        std::vector<uint16_t> img;
        std::string localErr;
        if (!loadRawU16LE(req.calibRawPath, nrC, naC, img, &localErr)) {
            if (err)
                *err = localErr;
            return false;
        }
        if (!computeRNear(req, nrC, RbinUse, bwDeg, R_near, &localErr)) {
            if (err)
                *err = localErr;
            return false;
        }
        resp.preview = makePreview(img, nrC, naC, 8);

        RectRoi calRoi = req.calBufRoi;
        if (!req.autoPick) {
            if (!cb.onRectRoi) {
                if (err)
                    *err = "需要 onRectRoi 框选角反缓冲区";
                return false;
            }
            if (!cb.onRectRoi(resp.preview, nrC, naC, "cal", calRoi) || !calRoi.valid()) {
                if (err)
                    *err = cancelled(cb) ? "已取消" : "未框选角反缓冲区";
                return false;
            }
        }
        if (!calEpsilon(img, nrC, naC, calRoi, eps, peakRow, peakCol, &localErr)) {
            if (err)
                *err = localErr;
            return false;
        }
        // MATLAB: R_axis(yc)=R_near+(HC-yc)*Rbin，yc 1-based；peakRow 0-based
        R_cal = R_near + (nrC - peakRow - 1) * RbinUse;
        K = eps * std::pow(R_cal, 4.0) / sigma_cal;
        {
            std::ostringstream os;
            os << "K=" << K << "  R_cal=" << R_cal << " m  ε=" << eps;
            message(cb, os.str());
        }
        if (cancelled(cb)) {
            if (err)
                *err = "已取消";
            return false;
        }
    } else {
        if (!(K > 0 && R_near > 0 && RbinUse > 0)) {
            if (err)
                *err = "doCalib=false 时需要有效的 K、R_near、Rbin";
            return false;
        }
        message(cb, "使用预填定标系数 K");
    }

    resp.K = K;
    resp.R_near = R_near;
    resp.Rbin = RbinUse;
    resp.R_cal = R_cal;
    resp.epsilon_cal = eps;
    resp.sigma_cal = sigma_cal;
    resp.peakRow = peakRow;
    resp.peakCol = peakCol;

    struct L0Heads {
        std::vector<uint8_t> fileHdr;
        std::vector<uint8_t> frame0;
    };
    std::map<std::string, L0Heads> l0Cache;
    L0Heads fixedHeads;
    const bool haveFixed = !trimStr(req.l0HeaderPath).empty();
    if (haveFixed) {
        std::string localErr;
        if (!loadL0Heads(req.l0HeaderPath, fixedHeads.fileHdr, fixedHeads.frame0, &localErr)) {
            if (err)
                *err = localErr;
            return false;
        }
    }

    // 列目录
    progress(cb, 20, "扫描 RAW…");
    std::vector<std::string> names = listFileNames(req.rawFolder);
    std::vector<std::string> raws;
    for (size_t i = 0; i < names.size(); ++i) {
        const std::string &n = names[i];
        const std::string u = toUpperStr(n);
        if (u.size() < 4 || u.substr(u.size() - 4) != ".RAW")
            continue;
        if (!nameHasToken(n, req.band))
            continue;
        if (!req.pol.empty() && !nameHasToken(n, req.pol))
            continue;
        int nr = 0, na = 0;
        if (!parseNrNa(n, nr, na))
            continue;
        raws.push_back(n);
    }
    std::sort(raws.begin(), raws.end());
    if (raws.empty()) {
        if (err)
            *err = "未找到匹配波段/极化且带 Nr/Na 的 .raw";
        return false;
    }
    int nF = static_cast<int>(raws.size());
    if (req.maxFiles > 0 && req.maxFiles < nF)
        nF = req.maxFiles;

    RectRoi targetRoi = req.targetRoi;
    if (!targetRoi.valid() && cb.onRectRoi && nF > 0) {
        const std::string firstPath = pathJoin(req.rawFolder, raws[0]);
        int nr0 = 0, na0 = 0;
        std::vector<uint16_t> firstImg;
        std::string localErr;
        if (parseNrNa(raws[0], nr0, na0) && loadRawU16LE(firstPath, nr0, na0, firstImg, &localErr)) {
            const GrayImage preview = makePreview(firstImg, nr0, na0, 8);
            RectRoi picked;
            if (cb.onRectRoi(preview, nr0, na0, "target", picked) && picked.valid())
                targetRoi = picked;
            else if (cancelled(cb)) {
                if (err)
                    *err = "已取消";
                return false;
            } else {
                message(cb, "未框选目标，三级数据写全图");
            }
        }
    }

    for (int i = 0; i < nF; ++i) {
        if (cancelled(cb)) {
            if (err)
                *err = "已取消";
            return false;
        }
        const std::string name = raws[static_cast<size_t>(i)];
        const std::string fPath = pathJoin(req.rawFolder, name);
        progress(cb, 20 + 70 * (i + 1) / nF, "定标 " + name);

        RcsCalTifFileResult fr;
        fr.rawName = name;
        int nr = 0, na = 0;
        if (!parseNrNa(name, nr, na)) {
            fr.note = "尺寸不明";
            resp.files.push_back(fr);
            continue;
        }
        fr.nr = nr;
        fr.na = na;
        const int64_t need = static_cast<int64_t>(nr) * na * 2;
        if (fileSizeBytes(fPath) < need) {
            fr.note = "字节数不符";
            resp.files.push_back(fr);
            continue;
        }
        std::vector<uint16_t> raw;
        std::string localErr;
        if (!loadRawU16LE(fPath, nr, na, raw, &localErr)) {
            fr.note = localErr;
            resp.files.push_back(fr);
            continue;
        }

        std::vector<float> dbsm(static_cast<size_t>(nr) * na);
        double vmin = 1e300, vmax = -1e300;
        for (int r = 0; r < nr; ++r) {
            const double R = R_near + (nr - r - 1) * RbinUse;
            const double R4 = std::pow(R, 4.0);
            const double floorSig = R4 / K; // DN=0 接地
            for (int c = 0; c < na; ++c) {
                const double dn = raw[static_cast<size_t>(r) * na + c];
                double sig = dn * dn * R4 / K;
                if (sig < floorSig)
                    sig = floorSig;
                const float db = static_cast<float>(10.0 * std::log10(sig));
                dbsm[static_cast<size_t>(r) * na + c] = db;
                if (db < vmin)
                    vmin = db;
                if (db > vmax)
                    vmax = db;
            }
        }
        fr.dbsmMin = vmin;
        fr.dbsmMax = vmax;
        raw.clear();

        const std::string stem = baseName(name);
        {
            const int scale = 100; // 与拼接三级 dBsm=DN/100 固定配对
            std::vector<int16_t> pix(dbsm.size());
            for (size_t k = 0; k < dbsm.size(); ++k) {
                double v = std::floor(static_cast<double>(dbsm[k]) * scale + 0.5);
                if (v > 32767)
                    v = 32767;
                if (v < -32768)
                    v = -32768;
                pix[k] = static_cast<int16_t>(v);
            }
            fr.tifPath = pathJoin(outDir, stem + "_RCS_dBsm.tif");
            char desc[512];
            std::snprintf(desc, sizeof(desc),
                          "RCS image; unit=dBsm x %d; restore dBsm=DN/%d; K=%.6e; Rbin=%.6f; "
                          "R_near=%.2f; size=%dx%d; source=%s",
                          scale, scale, K, RbinUse, R_near, nr, na, name.c_str());
            if (!writeTiffInt16(fr.tifPath, na, nr, pix, desc, &localErr)) {
                fr.note = localErr;
                fr.tifPath.clear();
            }
        }

        {
            const std::string tgtName = req.tgtName.empty() ? "目标" : req.tgtName;
            std::string polName = req.pol;
            if (polName.empty())
                polName = polTokenOf(name, req.pol);
            const std::string stamp = fileTimeStamp(fPath);
            const std::string l3File = tgtName + stamp + ".txt";
            fr.txtPath = pathJoin(outDir, l3File);

            int bx = 0, by = 0, bw = 0, bh = 0;
            const bool boxed = clampRoi(targetRoi, nr, na, bx, by, bw, bh);
            const int outNr = boxed ? bh : nr;
            const int outNa = boxed ? bw : na;
            std::vector<float> grid(static_cast<std::size_t>(outNr) * outNa);
            if (!boxed) {
                grid = dbsm;
                appendNote(fr.note, "未框选目标，三级与二级写全图");
            } else {
                for (int r = 0; r < outNr; ++r) {
                    for (int c = 0; c < outNa; ++c)
                        grid[static_cast<std::size_t>(r) * outNa + c] =
                            dbsm[static_cast<std::size_t>(by + r) * na + (bx + c)];
                }
                fr.positionPath = pathJoin(outDir, tgtName + stamp + "_目标位置.txt");
                if (!writePositionFile(fr.positionPath, name, nr, na, bx, by, bw, bh, l3File))
                    appendNote(fr.note, "目标位置文件未写出");
            }

            std::string distPath = trimStr(req.mergeTxtPath);
#if defined(RCS_ENABLE_TOZERODATA) && RCS_ENABLE_TOZERODATA
            if (distPath.empty()) {
                if (!boxed) {
                    if (err)
                        *err = "未框选目标，无法生成距离文件，三级数据不完整";
                    return false;
                }
                const std::string cplx = siblingCplx(fPath);
                if (cplx.empty()) {
                    if (err)
                        *err = "未找到同名 .cplx，无法生成距离文件: " + fPath;
                    return false;
                }
                SliceTxtRequest sreq;
                sreq.imagePath = cplx;
                sreq.roiX = bx + (bw - 1) / 2;
                sreq.roiY = by + (bh - 1) / 2;
                sreq.roiW = bw;
                sreq.roiH = bh;
                sreq.outPath = pathJoin(outDir, stem + "_横向纵向.txt");
                sreq.targetName = tgtName;
                SliceTxtResponse sresp;
                std::string serr;
                Callbacks sliceCb = cb;
                sliceCb.onProgress = std::function<void(int, const std::string &)>();
                if (!exportSliceTxt(sreq, sresp, sliceCb, &serr)) {
                    if (cancelled(cb) || serr == "已取消") {
                        if (err)
                            *err = "已取消";
                        return false;
                    }
                    if (err)
                        *err = serr.empty() ? "距离文件生成失败，三级数据不完整" : serr;
                    return false;
                }
                distPath = sresp.txtPath.empty() ? sreq.outPath : sresp.txtPath;
            }
#else
            if (distPath.empty()) {
                if (err)
                    *err = "当前库不能调用 SarTxtExporter，无法生成距离文件";
                return false;
            }
#endif
            std::vector<double> xy;
            const std::vector<double> *xyPtr = 0;
            std::string oneNote;
            if (!loadMergeDistances(distPath, outNr * outNa, xy, &oneNote)) {
                if (err)
                    *err = oneNote.empty() ? "距离文件无法并入三级数据: " + distPath : oneNote;
                return false;
            }
            xyPtr = &xy;
            appendNote(fr.note, "距离文件: " + distPath);

            struct tm tmbuf;
            const time_t now = time(0);
#if defined(_WIN32)
            localtime_s(&tmbuf, &now);
#else
            localtime_r(&now, &tmbuf);
#endif
            char dateBuf[32], timeBuf[40];
            std::snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d", tmbuf.tm_year + 1900,
                          tmbuf.tm_mon + 1, tmbuf.tm_mday);
            std::snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
                          tmbuf.tm_year + 1900, tmbuf.tm_mon + 1, tmbuf.tm_mday, tmbuf.tm_hour,
                          tmbuf.tm_min, tmbuf.tm_sec);
            if (!writeDsar(fr.txtPath, outNr, outNa, grid, xyPtr, tgtName, polName, dateBuf, timeBuf,
                           req.fsHz, boxed, RbinUse, &localErr)) {
                appendNote(fr.note, localErr);
                fr.txtPath.clear();
            }

            std::string headerPath = trimStr(req.l0HeaderPath);
            if (headerPath.empty())
                headerPath = findL0HeaderFile(req.rawFolder, polTokenOf(name, req.pol));
            if (headerPath.empty()) {
                appendNote(fr.note, "未找到匹配极化的 0 级 DAT，未写二级数据");
            } else {
                L0Heads heads;
                if (haveFixed) {
                    heads = fixedHeads;
                } else {
                    std::map<std::string, L0Heads>::const_iterator it = l0Cache.find(headerPath);
                    if (it == l0Cache.end()) {
                        L0Heads loaded;
                        if (!loadL0Heads(headerPath, loaded.fileHdr, loaded.frame0, &localErr)) {
                            appendNote(fr.note, localErr);
                            headerPath.clear();
                        } else {
                            l0Cache[headerPath] = loaded;
                            heads = loaded;
                        }
                    } else {
                        heads = it->second;
                    }
                }
                if (!headerPath.empty()) {
                    fr.pdatPath = fr.txtPath.empty() ? pathJoin(outDir, tgtName + stamp + ".pdat")
                                                     : pdatPathForTxt(fr.txtPath);
                    if (!Level2Pdat::saveCalDbsm(fr.pdatPath, heads.fileHdr, heads.frame0, outNr, outNa,
                                                 grid, R_cal, req.sigmaTheoryDb, 100, &localErr)) {
                        appendNote(fr.note, localErr);
                        fr.pdatPath.clear();
                    }
                }
            }
        }
        resp.files.push_back(fr);
    }

    progress(cb, 100, "RCS 像定标完成");
    return true;
}
