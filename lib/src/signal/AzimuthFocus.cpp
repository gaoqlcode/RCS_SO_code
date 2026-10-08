#include "AzimuthFocus.h"
#include "FftBackend.h"
#include "Util.hpp"
#include <atomic>
#include <cmath>
#include <thread>
#include <utility>
#include <vector>

static void fftshiftInPlace(std::vector<std::complex<double> > &x)
{
    const int n = static_cast<int>(x.size());
    const int h = n / 2;
    for (int i = 0; i < h; ++i)
        std::swap(x[static_cast<size_t>(i)], x[static_cast<size_t>(i + (n + 1) / 2)]);
}

void AzimuthFocus::apply(ComplexMatrix &data, const std::vector<double> &R, int mode, double vSar,
                         double lambda, double prf, const std::function<void(int)> &progress,
                         const std::function<bool()> &cancelled)
{
    const int Nr = data.nr;
    const int Np = data.np;
    if (Nr <= 0 || Np <= 0)
        return;

    unsigned hc = std::thread::hardware_concurrency();
    const int nt = static_cast<int>(hc == 0 ? 2 : imin(hc, 8u));
    std::atomic<int> next{0};
    std::atomic<int> done{0};
    std::atomic<bool> stop{false};
    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(nt));

    if (mode == 1) {
        std::vector<double> fEta(static_cast<size_t>(Np));
        for (int k = 0; k < Np; ++k)
            fEta[static_cast<size_t>(k)] = (k - Np / 2.0) / Np * prf;
        for (int t = 0; t < nt; ++t) {
            pool.emplace_back([&]() {
                std::vector<std::complex<double> > row(static_cast<size_t>(Np));
                for (;;) {
                    if (cancelled && cancelled()) {
                        stop = true;
                        return;
                    }
                    if (stop.load())
                        return;
                    const int g = next.fetch_add(1);
                    if (g >= Nr)
                        return;
                    const double Rg =
                        (g < static_cast<int>(R.size())) ? R[static_cast<size_t>(g)] : R.back();
                    const double Ka = 2.0 * vSar * vSar / (lambda * imax(Rg, 1e-6));
                    for (int p = 0; p < Np; ++p)
                        row[static_cast<size_t>(p)] = data.at(g, p);
                    FftBackend::fft(row, false);
                    fftshiftInPlace(row);
                    for (int p = 0; p < Np; ++p) {
                        const double f = fEta[static_cast<size_t>(p)];
                        row[static_cast<size_t>(p)] *=
                            std::exp(std::complex<double>(0, 3.14159265358979323846 * f * f / Ka));
                    }
                    fftshiftInPlace(row);
                    FftBackend::fft(row, true);
                    for (int p = 0; p < Np; ++p)
                        data.at(g, p) = row[static_cast<size_t>(p)];
                    const int d = done.fetch_add(1) + 1;
                    if (progress && (d % 64 == 0 || d == Nr))
                        progress(d * 100 / Nr);
                }
            });
        }
    } else {
        const double scale = 1.0 / std::sqrt(static_cast<double>(Np));
        for (int t = 0; t < nt; ++t) {
            pool.emplace_back([&]() {
                std::vector<std::complex<double> > row(static_cast<size_t>(Np));
                for (;;) {
                    if (cancelled && cancelled()) {
                        stop = true;
                        return;
                    }
                    if (stop.load())
                        return;
                    const int g = next.fetch_add(1);
                    if (g >= Nr)
                        return;
                    for (int p = 0; p < Np; ++p)
                        row[static_cast<size_t>(p)] = data.at(g, p);
                    FftBackend::fft(row, false);
                    fftshiftInPlace(row);
                    for (int p = 0; p < Np; ++p)
                        data.at(g, p) = row[static_cast<size_t>(p)] * scale;
                    const int d = done.fetch_add(1) + 1;
                    if (progress && (d % 64 == 0 || d == Nr))
                        progress(d * 100 / Nr);
                }
            });
        }
    }
    for (auto &th : pool)
        th.join();
}
