#include "Util.hpp"
#include "rcs/rcs_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
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

std::string fieldLine(const std::string &k, const std::string &v)
{
    return k + " = " + v + "\r\n";
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

std::string uniqTxtPath(const std::string &dir, const std::string &stem, const std::string &fallback)
{
    std::string p = pathJoin(dir, stem + ".txt");
    if (!pathExists(p))
        return p;
    p = pathJoin(dir, stem + "_" + fallback + ".txt");
    int n = 1;
    while (pathExists(p)) {
        ++n;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", n);
        p = pathJoin(dir, stem + "_" + fallback + "_" + buf + ".txt");
    }
    return p;
}

bool nameHasToken(const std::string &name, const std::string &tok)
{
    if (tok.empty())
        return true;
    const std::string u = toUpperStr(name);
    const std::string t = toUpperStr(tok);
    return u.find(t) != std::string::npos;
}

bool writeL3Txt(const std::string &path, int nr, int na, const std::vector<float> &dbsm, int layout,
                const Level3Info &info, std::string *err)
{
    std::ostringstream hdr;
    hdr << "[TASK]\r\n"
        << fieldLine("任务名称", info.taskName.empty() ? "SAR二维像RCS定标" : info.taskName)
        << fieldLine("任务负责人", info.operatorName) << fieldLine("任务单位", info.unit)
        << fieldLine("任务代号", info.code) << fieldLine("任务来源", info.source)
        << fieldLine("数据来源", info.dataSource.empty() ? "实测原始回波(.raw)辐射定标" : info.dataSource)
        << fieldLine("数据类型", info.dataType.empty() ? "3级 RCS 二维像(dBsm)" : info.dataType)
        << fieldLine("任务执行地点", info.place)
        << fieldLine("任务执行时间", info.time.empty() ? nowPretty() : info.time)
        << fieldLine("定标形式", info.calMode.empty() ? "角反射器能量(积分)法" : info.calMode)
        << fieldLine("定标体名称", info.calName.empty() ? "角反射器" : info.calName)
        << fieldLine("定标体尺寸", info.calSize) << fieldLine("不确定度", info.uncertainty)
        << "[/TASK]\r\n"
        << "[TARGET]\r\n"
        << fieldLine("目标名称", info.tgtName) << fieldLine("目标说明", info.tgtDesc)
        << fieldLine("目标分类", info.tgtClass) << fieldLine("目标长(m)", info.tgtL)
        << fieldLine("目标宽(m)", info.tgtW) << fieldLine("目标高(m)", info.tgtH)
        << "[/TARGET]\r\n"
        << "[SYSTEM]\r\n"
        << fieldLine("设备名称", info.devName) << fieldLine("设备分类", info.devClass)
        << fieldLine("工作体制", info.sysMode) << fieldLine("脉冲宽度(μs)", info.pulseWidth)
        << fieldLine("脉冲重复频率", info.prf) << fieldLine("动态范围(dB)", info.dynRange)
        << "[/SYSTEM]\r\n"
        << "[ENVIRONMENT]\r\n"
        << fieldLine("温度(℃)", info.temp) << fieldLine("湿度(%)", info.humidity)
        << fieldLine("气压(hPa)", info.pressure) << fieldLine("能见度(km)", info.visibility)
        << fieldLine("云况", info.cloud) << "[/ENVIRONMENT]\r\n"
        << "[CONDITIONS]\r\n"
        << fieldLine("测量时刻", info.measureTime) << fieldLine("频率步长(GHz)", info.fStep)
        << fieldLine("起始频率(GHz)", info.fStart) << fieldLine("终止频率(GHz)", info.fStop)
        << fieldLine("极化组合", info.pol) << fieldLine("目标高度(m)", info.tgtHeight)
        << fieldLine("起始入射方位视向角(°)", info.azStart)
        << fieldLine("终止入射方位视向角(°)", info.azStop)
        << fieldLine("起始入射俯仰视向角(°)", info.elStart)
        << fieldLine("终止入射俯仰视向角(°)", info.elStop)
        << fieldLine("回波采样率", info.fs) << fieldLine("备注", info.note) << "[/CONDITIONS]\r\n";

    std::string header = hdr.str();
    if (static_cast<int>(header.size()) > kL3HdrBytes) {
        // 超长仍写，不再硬失败（与 MATLAB 警告一致）
    } else {
        header.append(static_cast<size_t>(kL3HdrBytes - static_cast<int>(header.size())), ' ');
    }

    std::ostringstream data;
    data << "[DATA]\r\n";
    data << "RCS[" << nr << "，" << na << "]\r\n";
    char num[64];
    if (layout == rcs::RcsCalL3Row) {
        for (int r = 0; r < nr; ++r) {
            for (int c = 0; c < na; ++c) {
                if (c)
                    data << ' ';
                std::snprintf(num, sizeof(num), "%.3f",
                              dbsm[static_cast<size_t>(r) * na + c]);
                data << num;
            }
            data << "\r\n";
        }
    } else {
        for (int r = 0; r < nr; ++r) {
            for (int c = 0; c < na; ++c) {
                std::snprintf(num, sizeof(num), "%d %d %.3f\r\n", r + 1, c + 1,
                              dbsm[static_cast<size_t>(r) * na + c]);
                data << num;
            }
        }
    }
    data << "[/DATA]\r\n";

    std::string all = header + data.str();
    if (!writeFileAll(path, all.data(), all.size())) {
        if (err)
            *err = "无法写三级 txt: " + path;
        return false;
    }
    return true;
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
            Level3Info info = req.header;
            if (info.tgtName.empty())
                info.tgtName = req.tgtName.empty() ? "目标" : req.tgtName;
            if (info.pol.empty())
                info.pol = req.pol;
            if (info.fs.empty()) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.6g Hz", req.fsHz);
                info.fs = buf;
            }
            if (info.measureTime.empty())
                info.measureTime = nowPretty();
            if (info.time.empty())
                info.time = nowPretty();
            {
                char note[512];
                std::snprintf(note, sizeof(note),
                              "波段=%s; 极化=%s; K=%.6e; R_near=%.2f; Rbin=%.6f; sigma_cal=%.4f m2(%.3f "
                              "dBsm); 第1行=远端",
                              req.band.c_str(), info.pol.c_str(), K, R_near, RbinUse, sigma_cal,
                              req.sigmaTheoryDb);
                if (info.note.empty())
                    info.note = note;
            }
            const std::string stamp = fileTimeStamp(fPath);
            const std::string stemL3 = info.tgtName.empty() ? stem : info.tgtName;
            fr.txtPath = uniqTxtPath(outDir, stemL3 + stamp, stem);
            if (!writeL3Txt(fr.txtPath, nr, na, dbsm, req.l3Layout, info, &localErr)) {
                fr.note = (fr.note.empty() ? "" : fr.note + "; ") + localErr;
                fr.txtPath.clear();
            }
        }
        resp.files.push_back(fr);
    }

    progress(cb, 100, "RCS 像定标完成");
    return true;
}
