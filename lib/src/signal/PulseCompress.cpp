#include "PulseCompress.h"
#include "FftBackend.h"
#include "Util.hpp"
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

void PulseCompress::apply(ComplexMatrix &data, double bandwidthMHz, double pulseWidthUs, double fs,
                          const std::function<void(int)> &progress,
                          const std::function<bool()> &cancelled)
{
    const double tau = pulseWidthUs * 1e-6;
    const double B = bandwidthMHz * 1e6;
    if (tau <= 0 || B <= 0 || data.nr <= 0 || data.np <= 0)
        return;
    const double K = B / tau;
    const int Ns = imax(1, static_cast<int>(std::llround(tau * fs)));
    std::vector<std::complex<double> > ref(static_cast<size_t>(Ns));
    double norm2 = 0;
    for (int i = 0; i < Ns; ++i) {
        const double t = -tau / 2.0 + (Ns == 1 ? 0.0 : tau * i / (Ns - 1));
        const double w =
            0.54 - 0.46 * std::cos(2.0 * 3.14159265358979323846 * i / imax(1, Ns - 1));
        const std::complex<double> v =
            std::exp(std::complex<double>(0, 3.14159265358979323846 * K * t * t)) * w;
        ref[static_cast<size_t>(i)] = v;
        norm2 += std::norm(v);
    }
    const double nrm = std::sqrt(imax(norm2, 1e-30));
    for (auto &v : ref)
        v /= nrm;

    const int N = data.nr;
    std::vector<std::complex<double> > Ref = ref;
    FftBackend::fftLength(Ref, N, false);
    for (auto &v : Ref)
        v = std::conj(v);

    const int np = data.np;
    unsigned hc = std::thread::hardware_concurrency();
    const int nt = static_cast<int>(hc == 0 ? 2 : imin(hc, 8u));
    // 分块并行：progress 只在调用线程（GUI Worker）上触发，避免 Windows 上从池线程 emit 闪退
    const int chunk = imax(nt * 16, 128);

    for (int p0 = 0; p0 < np;) {
        if (cancelled && cancelled())
            return;
        const int p1 = imin(np, p0 + chunk);
        std::atomic<int> next{p0};
        std::atomic<bool> stop{false};
        std::vector<std::thread> pool;
        pool.reserve(static_cast<size_t>(nt));
        for (int t = 0; t < nt; ++t) {
            pool.emplace_back([&]() {
                std::vector<std::complex<double> > col(static_cast<size_t>(N));
                for (;;) {
                    if (cancelled && cancelled()) {
                        stop = true;
                        return;
                    }
                    if (stop.load())
                        return;
                    const int p = next.fetch_add(1);
                    if (p >= p1)
                        return;
                    for (int g = 0; g < N; ++g)
                        col[static_cast<size_t>(g)] = data.at(g, p);
                    FftBackend::fftLength(col, N, false);
                    for (int g = 0; g < N; ++g)
                        col[static_cast<size_t>(g)] *= Ref[static_cast<size_t>(g)];
                    FftBackend::fftLength(col, N, true);
                    for (int g = 0; g < N; ++g)
                        data.at(g, p) = col[static_cast<size_t>(g)];
                }
            });
        }
        for (auto &th : pool)
            th.join();
        if (stop.load() || (cancelled && cancelled()))
            return;
        p0 = p1;
        if (progress)
            progress(p0 * 100 / imax(1, np));
    }
}
