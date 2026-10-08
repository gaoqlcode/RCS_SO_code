#include "Level0Dat.h"
#include "BinaryIo.h"
#include "Util.hpp"
#include <cmath>

bool Level0Dat::parseHeader(const std::vector<uint8_t> &hdr512, FileHeaderInfo &out, std::string *err)
{
    if (hdr512.size() < 512) {
        if (err)
            *err = "文件头不足512字节";
        return false;
    }
    const uint8_t *h = hdr512.data();
    out.rawHeader = hdr512;
    out.flag = readU16LE(h + 0);
    out.lenFileH = readU16LE(h + 2);
    out.lenFrmH = readU16LE(h + 4);
    if (out.lenFileH == 0 || out.lenFileH > 512)
        out.lenFileH = 512;
    if (out.lenFrmH == 0 || out.lenFrmH > 512)
        out.lenFrmH = 256;
    out.devName.assign(reinterpret_cast<const char *>(h + 6), 4);
    while (!out.devName.empty() && (out.devName.back() == ' ' || out.devName.back() == '\0'))
        out.devName.pop_back();
    out.devId = readU16LE(h + 10);
    out.workMode = readU16LE(h + 12);
    out.waveform = readU16LE(h + 14);
    out.bandCode = readU16LE(h + 16);
    out.fcGHz = readF32LE(h + 22);
    out.fStartG = readF32LE(h + 38);
    out.fStopG = readF32LE(h + 42);
    out.fStepMHz = readU16LE(h + 46);
    out.bwMHz = readU16LE(h + 48);
    out.polComb = readU16LE(h + 134);
    out.year = readU16LE(h + 256);
    out.month = readU16LE(h + 258);
    out.day = readU16LE(h + 260);
    out.hour = readU16LE(h + 266);
    out.minute = readU16LE(h + 268);
    out.second = readU16LE(h + 270);
    out.msec = readU16LE(h + 274);
    return true;
}

bool Level0Dat::readFile(const std::string &path, FileHeaderInfo &hdr, FrameMeta &frame0,
                         ComplexMatrix &data, std::vector<uint16_t> &azList, int phaseMode,
                         int maxFrame, std::function<void(int)> progress, std::string *err,
                         std::function<bool()> cancelled)
{
    std::vector<uint8_t> all;
    if (!readBinFile(path, all)) {
        if (err)
            *err = "无法打开: " + path;
        return false;
    }
    if (all.size() < 512) {
        if (err)
            *err = "文件过小";
        return false;
    }
    std::vector<uint8_t> hdrBytes(all.begin(), all.begin() + 512);
    if (!parseHeader(hdrBytes, hdr, err))
        return false;

    const int lenFileH = hdr.lenFileH;
    const int lenFrmH = hdr.lenFrmH;
    if (static_cast<int>(all.size()) < lenFileH + lenFrmH) {
        if (err)
            *err = "读不到第1帧帧头";
        return false;
    }
    const uint8_t *fh1 = all.data() + lenFileH;
    for (int i = 0; i < 4; ++i)
        frame0.sync[i] = readU16LE(fh1 + i * 2);
    frame0.N = readU32LE(fh1 + 16);
    frame0.PRT = readU32LE(fh1 + 20);
    frame0.tauN = readU32LE(fh1 + 24);
    frame0.dlyN = readU32LE(fh1 + 28);
    frame0.azRaw = readU16LE(fh1 + 32);
    frame0.rollRaw = readI16LE(fh1 + 36);
    if (frame0.N == 0 || frame0.N > 10000000u) {
        if (err)
            *err = "帧长度N异常";
        return false;
    }

    const int64_t frameBytes = static_cast<int64_t>(lenFrmH) + static_cast<int64_t>(frame0.N) * 4;
    const int64_t nEst = (static_cast<int64_t>(all.size()) - lenFileH) / frameBytes;
    const int nMax = (maxFrame > 0) ? maxFrame : static_cast<int>(nEst);
    data.resize(static_cast<int>(frame0.N), imax(1, nMax));
    azList.clear();
    azList.reserve(static_cast<size_t>(nMax));

    int nFrame = 0;
    int64_t off = lenFileH;
    while (off + lenFrmH <= static_cast<int64_t>(all.size()) && nFrame < nMax) {
        if (cancelled && cancelled()) {
            if (err)
                *err = "已取消";
            return false;
        }
        const uint8_t *fh = all.data() + off;
        uint32_t Nf = readU32LE(fh + 16);
        if (Nf != frame0.N)
            break;
        uint16_t az = readU16LE(fh + 32);
        off += lenFrmH;
        if (off + static_cast<int64_t>(frame0.N) * 4 > static_cast<int64_t>(all.size()))
            break;
        const uint8_t *raw = all.data() + off;
        for (uint32_t g = 0; g < frame0.N; ++g) {
            uint16_t A = readU16LE(raw + g * 4);
            uint16_t PH = readU16LE(raw + g * 4 + 2);
            double phi;
            if (phaseMode == 0)
                phi = PH / 65536.0 * 2.0 * 3.14159265358979323846;
            else
                phi = (PH - 32768.0) / 32768.0 * 3.14159265358979323846;
            data.at(static_cast<int>(g), nFrame) = std::polar(static_cast<double>(A), phi);
        }
        off += static_cast<int64_t>(frame0.N) * 4;
        azList.push_back(az);
        ++nFrame;
        if (progress && (nFrame % 64 == 0 || nFrame == nMax))
            progress(static_cast<int>(100.0 * nFrame / imax(1, nMax)));
    }
    data.np = nFrame;
    data.data.resize(static_cast<size_t>(data.nr) * data.np);
    if (nFrame == 0) {
        if (err)
            *err = "未读到有效帧";
        return false;
    }
    return true;
}

void Level0Dat::buildRangeAxis(const FrameMeta &fm, std::vector<double> &R)
{
    const double fs = kFsForced;
    const double t0 = fm.dlyN / fs;
    const double dr = kC / (2.0 * fs);
    R.resize(static_cast<size_t>(fm.N));
    for (size_t i = 0; i < R.size(); ++i)
        R[i] = (kC * t0) / 2.0 + static_cast<double>(i) * dr;
}
