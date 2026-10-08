#include "MergeDat.h"
#include "BinaryIo.h"
#include "Util.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <regex>

static int firstMultiDigit(const std::string &name)
{
    // 取文件名里【第一个】至少两位的数字段（避免把 L0 的 0 当序号）
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

static std::string fmtTimeCompact(const std::vector<int> &t)
{
    if (t.size() < 6)
        return std::string();
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d", t[0], t[1], t[2], t[3], t[4], t[5]);
    return buf;
}

static std::string fmtTimePretty(const std::vector<int> &t)
{
    if (t.size() < 6)
        return std::string();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", t[0], t[1], t[2], t[3], t[4],
                  t[5]);
    return buf;
}

std::string mergeTimeSpanTag(const MergeSidecar &side)
{
    if (side.fileTimes.empty())
        return std::string();
    const std::string a = fmtTimeCompact(side.fileTimes.front());
    const std::string b = fmtTimeCompact(side.fileTimes.back());
    if (a.empty())
        return b;
    if (b.empty() || a == b)
        return a;
    return a + b;
}

std::string mergeTimeSpanPretty(const MergeSidecar &side)
{
    if (side.fileTimes.empty())
        return std::string();
    const std::string a = fmtTimePretty(side.fileTimes.front());
    const std::string b = fmtTimePretty(side.fileTimes.back());
    if (a.empty())
        return b;
    if (b.empty() || a == b)
        return a;
    return a + " ~ " + b;
}

double mergeStartTimeMs(const MergeSidecar &side)
{
    if (side.fileTimes.empty() || side.fileTimes.front().size() < 7)
        return 0;
    const std::vector<int> &t = side.fileTimes.front();
    return (t[3] * 3600 + t[4] * 60 + t[5]) * 1000.0 + t[6];
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

bool MergeDat::mergeFolder(const std::string &folder, const std::string &polSel,
                           std::string &outMergedPath, MergeSidecar *side, std::string *err,
                           const std::string &mergedDir, std::function<bool()> cancelled)
{
    if (!isDir(folder)) {
        if (err)
            *err = "数据文件夹不存在或无法访问（路径含中文时请使用本版本库）: " + folder;
        return false;
    }

    std::vector<std::string> files = listFileNames(folder);
    std::vector<std::string> datLike;
    for (size_t i = 0; i < files.size(); ++i) {
        if (endsWithIgnoreCase(files[i], ".dat") || endsWithIgnoreCase(files[i], ".pdat"))
            datLike.push_back(files[i]);
    }
    if (datLike.empty()) {
        if (err)
            *err = "文件夹里没有 .dat/.pdat";
        return false;
    }

    const std::string pol = toUpperStr(trimStr(polSel));
    std::vector<std::string> kept;
    for (size_t i = 0; i < datLike.size(); ++i) {
        const std::string &n = datLike[i];
        if (n.size() >= 7 && toUpperStr(n.substr(0, 7)) == "_MERGED")
            continue;
        if (n.find("处理结果_") == 0)
            continue;
        if (pol.empty() || toUpperStr(n).find(pol) != std::string::npos)
            kept.push_back(n);
    }
    if (kept.empty()) {
        if (err)
            *err = std::string("没有 ") + pol + " 极化的 dat";
        return false;
    }
    std::sort(kept.begin(), kept.end(), [](const std::string &a, const std::string &b) {
        return firstMultiDigit(a) < firstMultiDigit(b);
    });

    const std::string writeDir = trimStr(mergedDir).empty() ? folder : trimStr(mergedDir);
    if (!makeDirs(writeDir)) {
        if (err)
            *err = "无法创建输出目录: " + writeDir;
        return false;
    }
    outMergedPath = pathJoin(writeDir, "_merged_CR.dat");

    FILE *fo = fopenUtf8(outMergedPath, "wb");
    if (!fo) {
        if (err)
            *err = "无法创建合并文件: " + outMergedPath;
        return false;
    }

    if (side) {
        side->usedFiles = kept;
        side->fileTimes.clear();
        side->fileRadarAz.clear();
        side->fileRadarRoll.clear();
        side->frameFileIdx.clear();
    }

    bool headerWritten = false;
    int fileIndex = 0;
    for (size_t ki = 0; ki < kept.size(); ++ki) {
        if (cancelled && cancelled()) {
            if (err)
                *err = "已取消";
            fclose(fo);
            removeFile(outMergedPath);
            return false;
        }
        ++fileIndex;
        const std::string name = kept[ki];
        std::vector<uint8_t> all;
        if (!readFileAll(pathJoin(folder, name), all))
            continue;
        if (all.size() < 512)
            continue;

        const uint8_t *hd = &all[0];
        uint16_t lFH = readU16LE(hd + 2);
        uint16_t lFr = readU16LE(hd + 4);
        if (lFH == 0 || lFH > 512)
            lFH = 512;
        if (lFr == 0 || lFr > 512)
            lFr = 256;

        if (side) {
            std::vector<int> t(7);
            t[0] = readU16LE(hd + 256);
            t[1] = readU16LE(hd + 258);
            t[2] = readU16LE(hd + 260);
            t[3] = readU16LE(hd + 266);
            t[4] = readU16LE(hd + 268);
            t[5] = readU16LE(hd + 270);
            t[6] = readU16LE(hd + 274);
            side->fileTimes.push_back(t);
        }

        if (!headerWritten) {
            if (fwrite(&all[0], 1, static_cast<size_t>(lFH), fo) != static_cast<size_t>(lFH)) {
                fclose(fo);
                if (err)
                    *err = "写入合并文件失败";
                return false;
            }
            headerWritten = true;
        }

        int64_t off = lFH;
        bool firstFrame = true;
        while (off + lFr <= static_cast<int64_t>(all.size())) {
            const uint8_t *fh = &all[0] + off;
            uint32_t Nf = readU32LE(fh + 16);
            if (Nf == 0)
                break;
            const int64_t need = lFr + static_cast<int64_t>(Nf) * 4;
            if (off + need > static_cast<int64_t>(all.size()))
                break;
            if (side && firstFrame) {
                side->fileRadarAz.push_back(readU16LE(fh + 32));
                side->fileRadarRoll.push_back(readI16LE(fh + 36));
                firstFrame = false;
            }
            if (fwrite(&all[0] + off, 1, static_cast<size_t>(need), fo) != static_cast<size_t>(need)) {
                fclose(fo);
                if (err)
                    *err = "写入合并文件失败";
                return false;
            }
            if (side)
                side->frameFileIdx.push_back(fileIndex);
            off += need;
        }
        if (side && firstFrame) {
            side->fileRadarAz.push_back(0);
            side->fileRadarRoll.push_back(0);
        }
    }
    fclose(fo);
    if (!headerWritten) {
        if (err)
            *err = "合并失败";
        return false;
    }
    return true;
}
