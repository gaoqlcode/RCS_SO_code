#include "FftBackend.h"
#include "pocketfft_hdronly.h"
#include <cstddef>

void FftBackend::fft(std::vector<std::complex<double> > &x, bool inverse)
{
    fftLength(x, static_cast<int>(x.size()), inverse);
}

void FftBackend::fftLength(std::vector<std::complex<double> > &x, int nfft, bool inverse)
{
    if (nfft <= 0)
        return;
    if (static_cast<int>(x.size()) < nfft)
        x.resize(static_cast<size_t>(nfft), {0.0, 0.0});
    else if (static_cast<int>(x.size()) > nfft)
        x.resize(static_cast<size_t>(nfft));

    const pocketfft::shape_t shape{static_cast<size_t>(nfft)};
    const std::ptrdiff_t strideBytes = static_cast<std::ptrdiff_t>(sizeof(std::complex<double>));
    const pocketfft::stride_t stride_in{strideBytes};
    const pocketfft::stride_t stride_out{strideBytes};
    const pocketfft::shape_t axes{0};

    std::vector<std::complex<double> > out(static_cast<size_t>(nfft));
    const bool forward = !inverse;
    const double fct = inverse ? (1.0 / static_cast<double>(nfft)) : 1.0;

    pocketfft::c2c(shape, stride_in, stride_out, axes, forward, x.data(), out.data(), fct, 1u);
    x.swap(out);
}
