#include "Calibration.h"
#include "Util.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

int Calibration::argMax(const std::vector<double> &v)
{
    int idx = 0;
    double best = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < static_cast<int>(v.size()); ++i) {
        if (v[static_cast<size_t>(i)] > best) {
            best = v[static_cast<size_t>(i)];
            idx = i;
        }
    }
    return idx;
}

void Calibration::meanPowerProfile(const ComplexMatrix &data, std::vector<double> &profile)
{
    profile.resize(static_cast<size_t>(data.nr));
    for (int g = 0; g < data.nr; ++g) {
        double s = 0;
        for (int p = 0; p < data.np; ++p)
            s += std::norm(data.at(g, p));
        profile[static_cast<size_t>(g)] = s / imax(1, data.np);
    }
}

void Calibration::cropAroundPeak(ComplexMatrix &data, std::vector<double> &profile,
                                 std::vector<double> &R, int peak, int cropN)
{
    if (data.nr <= cropN)
        return;
    const int half = cropN / 2;
    int startG = peak - half;
    startG = ibound(0, startG, data.nr - cropN);
    ComplexMatrix cropped;
    cropped.resize(cropN, data.np);
    std::vector<double> p2(static_cast<size_t>(cropN)), r2(static_cast<size_t>(cropN));
    for (int g = 0; g < cropN; ++g) {
        p2[static_cast<size_t>(g)] = profile[static_cast<size_t>(startG + g)];
        r2[static_cast<size_t>(g)] = R[static_cast<size_t>(startG + g)];
        for (int p = 0; p < data.np; ++p)
            cropped.at(g, p) = data.at(startG + g, p);
    }
    data = std::move(cropped);
    profile = std::move(p2);
    R = std::move(r2);
}

void Calibration::gateWindowFromPeak(const std::vector<double> &prof, int gp, double critDb, double fac,
                                     int N, double riseTolDb, int &g1, int &g2, int &bw)
{
    gp = ibound(0, gp, N - 1);
    const double pk = prof[static_cast<size_t>(gp)];
    const double thr = pk * std::pow(10.0, -critDb / 10.0);
    const double riseFac = std::pow(10.0, riseTolDb / 10.0);
    int l = gp;
    double mnl = pk;
    while (l > 0 && prof[static_cast<size_t>(l - 1)] >= thr) {
        const double v = prof[static_cast<size_t>(l - 1)];
        if (v < mnl)
            mnl = v;
        if (v > mnl * riseFac)
            break;
        --l;
    }
    int r = gp;
    double mnr = pk;
    while (r < N - 1 && prof[static_cast<size_t>(r + 1)] >= thr) {
        const double v = prof[static_cast<size_t>(r + 1)];
        if (v < mnr)
            mnr = v;
        if (v > mnr * riseFac)
            break;
        ++r;
    }
    bw = r - l + 1;
    const int half = imax(1, static_cast<int>(std::llround(bw * fac / 2.0)));
    g1 = imax(0, gp - half);
    g2 = imin(N - 1, gp + half);
}

void Calibration::gateNetPower(const std::vector<double> &prof, int g1, int g2, int N, double &Pnet,
                               double &bgPer)
{
    const int Nw = g2 - g1 + 1;
    std::vector<double> bgseg;
    for (int i = imax(0, g1 - Nw); i <= imax(0, g1 - 1); ++i)
        bgseg.push_back(prof[static_cast<size_t>(i)]);
    for (int i = imin(N - 1, g2 + 1); i <= imin(N - 1, g2 + Nw); ++i)
        bgseg.push_back(prof[static_cast<size_t>(i)]);
    if (bgseg.empty())
        bgPer = 0;
    else {
        std::nth_element(bgseg.begin(), bgseg.begin() + static_cast<std::ptrdiff_t>(bgseg.size() / 2),
                         bgseg.end());
        bgPer = bgseg[bgseg.size() / 2];
    }
    double sum = 0;
    for (int i = g1; i <= g2; ++i)
        sum += prof[static_cast<size_t>(i)];
    Pnet = imax(sum - bgPer * Nw, 0.0);
}

int Calibration::snapPeak1D(const std::vector<double> &prof, int gp, int win, int N)
{
    int a = imax(0, gp - win);
    int b = imin(N - 1, gp + win);
    int best = gp;
    double bv = prof[static_cast<size_t>(gp)];
    for (int i = a; i <= b; ++i) {
        if (prof[static_cast<size_t>(i)] > bv) {
            bv = prof[static_cast<size_t>(i)];
            best = i;
        }
    }
    return best;
}

double Calibration::cornerEnergy2D(const ComplexMatrix &focused, int pkR, int g1c, int g2c)
{
    const int Np = focused.np;
    std::vector<double> azProf(static_cast<size_t>(Np));
    for (int p = 0; p < Np; ++p)
        azProf[static_cast<size_t>(p)] = std::norm(focused.at(pkR, p));
    const int pkAz = argMax(azProf);
    const double thr3 = azProf[static_cast<size_t>(pkAz)] / 2.0;
    int a1 = pkAz, a2 = pkAz;
    while (a1 > 0 && azProf[static_cast<size_t>(a1 - 1)] >= thr3)
        --a1;
    while (a2 < Np - 1 && azProf[static_cast<size_t>(a2 + 1)] >= thr3)
        ++a2;
    const int halfA = imax(2, static_cast<int>(std::ceil((a2 - a1 + 1) * 1.5 / 2.0)));
    const int a1m = imax(0, pkAz - halfA);
    const int a2m = imin(Np - 1, pkAz + halfA);
    const int Nw = g2c - g1c + 1;
    const int Na = a2m - a1m + 1;
    double gross = 0;
    for (int g = g1c; g <= g2c; ++g)
        for (int p = a1m; p <= a2m; ++p)
            gross += std::norm(focused.at(g, p));
    std::vector<double> bgseg;
    for (int g = imax(0, g1c - Nw); g <= imax(0, g1c - 1); ++g)
        for (int p = a1m; p <= a2m; ++p)
            bgseg.push_back(std::norm(focused.at(g, p)));
    for (int g = imin(focused.nr - 1, g2c + 1);
         g <= imin(focused.nr - 1, g2c + Nw); ++g)
        for (int p = a1m; p <= a2m; ++p)
            bgseg.push_back(std::norm(focused.at(g, p)));
    double bgPer = 0;
    if (!bgseg.empty()) {
        std::nth_element(bgseg.begin(), bgseg.begin() + static_cast<std::ptrdiff_t>(bgseg.size() / 2),
                         bgseg.end());
        bgPer = bgseg[bgseg.size() / 2];
    }
    return imax(gross - bgPer * (Nw * Na), 1e-30);
}

void Calibration::toRcsProfile(const std::vector<double> &profile, const std::vector<double> &R,
                               double sigmaCalLin, double Pcorner, double Rcorner,
                               std::vector<double> &rcsDbsm)
{
    rcsDbsm.resize(profile.size());
    for (size_t i = 0; i < profile.size(); ++i) {
        const double sig = sigmaCalLin * (profile[i] / imax(Pcorner, 1e-30))
                           * std::pow(R[i] / Rcorner, 4.0);
        rcsDbsm[i] =
            10.0 * std::log10(imax(sig, static_cast<double>(std::numeric_limits<double>::min())));
    }
}
