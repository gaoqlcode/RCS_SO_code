#include "Level2Pdat.h"
#include "BinaryIo.h"
#include "Util.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

static void patchCalHeader(std::vector<uint8_t> &ohdr, double Rcorner, double sigmaDb, int rcsScale)
{
    if (ohdr.size() < 512)
        ohdr.resize(512, 0);
    writeU16LE(ohdr.data() + 320, 1);
    writeU32LE(ohdr.data() + 322, static_cast<uint32_t>(std::llround(Rcorner)));
    writeU32LE(ohdr.data() + 326, static_cast<uint32_t>(std::llround(sigmaDb * rcsScale)));
    writeU32LE(ohdr.data() + 330, static_cast<uint32_t>(std::llround(sigmaDb * rcsScale)));
}

static bool writeBytes(FILE *f, const void *p, size_t n)
{
    return fwrite(p, 1, n, f) == n;
}

bool Level2Pdat::save2D(const std::string &outPath, const ComplexMatrix &focused,
                        const std::vector<double> &R, double sigmaCalLin, double Rcorner, double Pcorner,
                        const FileHeaderInfo &hdr, const FrameMeta &fm,
                        const std::vector<uint16_t> &azList, int rcsScale, std::string *err)
{
    FILE *f = fopenUtf8(outPath, "wb");
    if (!f) {
        if (err)
            *err = "无法创建: " + outPath;
        return false;
    }
    std::vector<uint8_t> ohdr = hdr.rawHeader;
    if (ohdr.size() < 512)
        ohdr.resize(512, 0);
    const double sigmaDb = 10.0 * std::log10(imax(sigmaCalLin, 1e-30));
    patchCalHeader(ohdr, Rcorner, sigmaDb, rcsScale);
    if (!writeBytes(f, ohdr.data(), 512)) {
        fclose(f);
        if (err)
            *err = "写入失败: " + outPath;
        return false;
    }

    const int N = focused.nr;
    const int Np = focused.np;
    const double twoPi = 2.0 * 3.14159265358979323846;
    std::vector<uint8_t> fh(256, 0);
    std::vector<uint8_t> buf(static_cast<size_t>(N) * 6);

    for (int p = 0; p < Np; ++p) {
        uint8_t *h = fh.data();
        std::memset(h, 0, 256);
        for (int i = 0; i < 4; ++i)
            writeU16LE(h + i * 2, fm.sync[i]);
        writeU32LE(h + 8, static_cast<uint32_t>(p));
        writeU32LE(h + 16, static_cast<uint32_t>(N));
        writeU32LE(h + 20, fm.PRT);
        writeU32LE(h + 24, fm.tauN);
        writeU32LE(h + 28, fm.dlyN);
        uint16_t az = (p < static_cast<int>(azList.size())) ? azList[static_cast<size_t>(p)] : 0;
        writeU16LE(h + 32, az);
        if (!writeBytes(f, fh.data(), 256)) {
            fclose(f);
            if (err)
                *err = "写入失败: " + outPath;
            return false;
        }

        uint8_t *b = buf.data();
        for (int g = 0; g < N; ++g) {
            const auto &c = focused.at(g, p);
            const double Pg = std::norm(c);
            const double Rr = (g < static_cast<int>(R.size())) ? R[static_cast<size_t>(g)] : Rcorner;
            double sig =
                sigmaCalLin * (Pg / imax(Pcorner, 1e-30)) * std::pow(Rr / Rcorner, 4.0);
            float sigDB = static_cast<float>(
                10.0 * std::log10(imax(sig, static_cast<double>(std::numeric_limits<double>::min()))));
            double ang = std::arg(c);
            if (ang < 0)
                ang += twoPi;
            uint16_t phU =
                static_cast<uint16_t>(std::llround(std::fmod(ang, twoPi) / twoPi * 65535.0));
            writeF32LE(b + g * 6, sigDB);
            writeU16LE(b + g * 6 + 4, phU);
        }
        if (!writeBytes(f, buf.data(), buf.size())) {
            fclose(f);
            if (err)
                *err = "写入失败: " + outPath;
            return false;
        }
    }
    fclose(f);
    return true;
}

bool Level2Pdat::save1D(const std::string &outPath, const std::vector<double> &rcsDbsm,
                        const std::vector<double> &phaseDeg, double Rcorner, double sigmaDb,
                        const FileHeaderInfo &hdr, const FrameMeta &fm, int rcsScale,
                        std::string *err)
{
    FILE *f = fopenUtf8(outPath, "wb");
    if (!f) {
        if (err)
            *err = "无法创建: " + outPath;
        return false;
    }
    std::vector<uint8_t> ohdr = hdr.rawHeader;
    if (ohdr.size() < 512)
        ohdr.resize(512, 0);
    patchCalHeader(ohdr, Rcorner, sigmaDb, rcsScale);
    if (!writeBytes(f, ohdr.data(), 512)) {
        fclose(f);
        if (err)
            *err = "写入失败: " + outPath;
        return false;
    }

    const int N = static_cast<int>(rcsDbsm.size());
    std::vector<uint8_t> fh(256, 0);
    uint8_t *h = fh.data();
    for (int i = 0; i < 4; ++i)
        writeU16LE(h + i * 2, fm.sync[i]);
    writeU32LE(h + 16, static_cast<uint32_t>(N));
    writeU32LE(h + 20, fm.PRT);
    writeU32LE(h + 24, fm.tauN);
    writeU32LE(h + 28, fm.dlyN);
    if (!writeBytes(f, fh.data(), 256)) {
        fclose(f);
        if (err)
            *err = "写入失败: " + outPath;
        return false;
    }

    const double twoPi = 2.0 * 3.14159265358979323846;
    std::vector<uint8_t> buf(static_cast<size_t>(N) * 6);
    uint8_t *b = buf.data();
    for (int g = 0; g < N; ++g) {
        float sigF = static_cast<float>(rcsDbsm[static_cast<size_t>(g)]);
        double ph = 0;
        if (g < static_cast<int>(phaseDeg.size()))
            ph = phaseDeg[static_cast<size_t>(g)] * 3.14159265358979323846 / 180.0;
        ph = std::fmod(ph, twoPi);
        if (ph < 0)
            ph += twoPi;
        uint16_t phU = static_cast<uint16_t>(std::llround(ph / twoPi * 65535.0));
        writeF32LE(b + g * 6, sigF);
        writeU16LE(b + g * 6 + 4, phU);
    }
    const bool ok = writeBytes(f, buf.data(), buf.size());
    fclose(f);
    if (!ok && err)
        *err = "写入失败: " + outPath;
    return ok;
}

bool Level2Pdat::saveCalDbsm(const std::string &outPath, const std::vector<uint8_t> &fileHeader,
                             const std::vector<uint8_t> &frame0, int nr, int na,
                             const std::vector<float> &dbsm, double RcalM, double sigmaDb,
                             int rcsScale, std::string *err)
{
    if (nr <= 0 || na <= 0 || dbsm.size() < static_cast<size_t>(nr) * static_cast<size_t>(na)) {
        if (err)
            *err = "二级数据尺寸无效";
        return false;
    }
    if (fileHeader.size() < 512 || frame0.size() < 256) {
        if (err)
            *err = "0 级文件头不足 512+256 字节";
        return false;
    }

    FILE *f = fopenUtf8(outPath, "wb");
    if (!f) {
        if (err)
            *err = "无法创建: " + outPath;
        return false;
    }

    std::vector<uint8_t> ohdr(fileHeader.begin(), fileHeader.begin() + 512);
    patchCalHeader(ohdr, RcalM, sigmaDb, rcsScale);
    if (!writeBytes(f, ohdr.data(), 512)) {
        fclose(f);
        if (err)
            *err = "写入失败: " + outPath;
        return false;
    }

    std::vector<uint8_t> fh(256, 0);
    std::vector<uint8_t> buf(static_cast<size_t>(nr) * 6);
    for (int c = 0; c < na; ++c) {
        std::memcpy(fh.data(), frame0.data(), 256);
        writeU32LE(fh.data() + 8, static_cast<uint32_t>(c));
        writeU32LE(fh.data() + 16, static_cast<uint32_t>(nr));
        if (!writeBytes(f, fh.data(), 256)) {
            fclose(f);
            if (err)
                *err = "写入失败: " + outPath;
            return false;
        }
        uint8_t *b = buf.data();
        for (int r = 0; r < nr; ++r) {
            const float db = dbsm[static_cast<size_t>(r) * static_cast<size_t>(na) + static_cast<size_t>(c)];
            writeF32LE(b + r * 6, db);
            writeU16LE(b + r * 6 + 4, 0);
        }
        if (!writeBytes(f, buf.data(), buf.size())) {
            fclose(f);
            if (err)
                *err = "写入失败: " + outPath;
            return false;
        }
    }
    fclose(f);
    return true;
}
