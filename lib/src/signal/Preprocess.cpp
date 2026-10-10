#include "Preprocess.h"
#include "Util.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

void Preprocess::removeDc(ComplexMatrix &data)
{
    for (int g = 0; g < data.nr; ++g) {
        std::complex<double> m{0, 0};
        for (int p = 0; p < data.np; ++p)
            m += data.at(g, p);
        m /= static_cast<double>(imax(1, data.np));
        for (int p = 0; p < data.np; ++p)
            data.at(g, p) -= m;
    }
}

void Preprocess::removeDcPerPulse(ComplexMatrix &data)
{
    // MATLAB RCS_DianPin: z = z - mean(z); 每脉冲沿距离向
    for (int p = 0; p < data.np; ++p) {
        std::complex<double> m{0, 0};
        for (int g = 0; g < data.nr; ++g)
            m += data.at(g, p);
        m /= static_cast<double>(imax(1, data.nr));
        for (int g = 0; g < data.nr; ++g)
            data.at(g, p) -= m;
    }
}

void Preprocess::removeDirectWave(ComplexMatrix &data, double pulseWidthUs, double fs)
{
    const int Nr = data.nr;
    const int Np = data.np;
    const int N_pulse_gate = static_cast<int>(std::ceil(pulseWidthUs * 1e-6 * fs));
    const int search_max = imin(100, Nr);
    std::vector<double> profile(static_cast<size_t>(search_max));
    for (int g = 0; g < search_max; ++g) {
        double s = 0;
        for (int p = 0; p < Np; ++p)
            s += std::norm(data.at(g, p));
        profile[static_cast<size_t>(g)] = s / imax(1, Np);
    }
    std::vector<double> sorted = profile;
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2),
                     sorted.end());
    const double noise = sorted[sorted.size() / 2];
    int direct_idx = 0;
    for (int g = 0; g < search_max; ++g) {
        if (profile[static_cast<size_t>(g)] > 3.0 * noise) {
            direct_idx = g;
            break;
        }
    }
    const int end_idx = imin(direct_idx + N_pulse_gate, Nr - 1);
    for (int g = 0; g <= end_idx; ++g) {
        std::complex<double> m{0, 0};
        for (int p = 0; p < Np; ++p)
            m += data.at(g, p);
        m /= static_cast<double>(imax(1, Np));
        for (int p = 0; p < Np; ++p)
            data.at(g, p) -= m;
    }
}
