#ifndef FFTBACKEND_H
#define FFTBACKEND_H

#include <complex>
#include <vector>

class FftBackend {
public:
    static void fft(std::vector<std::complex<double> > &x, bool inverse);
    static void fftLength(std::vector<std::complex<double> > &x, int nfft, bool inverse);
};

#endif
