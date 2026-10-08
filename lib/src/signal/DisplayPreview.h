#ifndef DISPLAYPREVIEW_H
#define DISPLAYPREVIEW_H

#include "Util.hpp"
#include "rcs/types.hpp"
#include <cmath>
#include <vector>

inline GrayImage makeAmpPreview(const ComplexMatrix &data, int maxW = 1400, int maxH = 900)
{
    GrayImage img;
    if (data.nr <= 0 || data.np <= 0)
        return img;
    const int stepAz = imax(1, (data.np + maxW - 1) / maxW);
    const int stepRg = imax(1, (data.nr + maxH - 1) / maxH);
    const int w = (data.np + stepAz - 1) / stepAz;
    const int h = (data.nr + stepRg - 1) / stepRg;
    img.width = w;
    img.height = h;
    img.pixels.assign(static_cast<size_t>(w) * h, 0);
    float mx = 0.f;
    std::vector<float> amp(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        const int g = imin(data.nr - 1, y * stepRg);
        for (int x = 0; x < w; ++x) {
            const int p = imin(data.np - 1, x * stepAz);
            const float a = static_cast<float>(std::abs(data.at(g, p)));
            amp[static_cast<size_t>(y) * w + x] = a;
            mx = imax(mx, a);
        }
    }
    if (mx < 1e-12f)
        mx = 1.f;
    for (int y = 0; y < h; ++y) {
        uint8_t *line = img.pixels.data() + static_cast<size_t>(h - 1 - y) * w;
        for (int x = 0; x < w; ++x)
            line[x] = static_cast<uint8_t>(
                ibound(0, int(amp[static_cast<size_t>(y) * w + x] / mx * 255.f), 255));
    }
    return img;
}

inline std::vector<double> meanPowerDb(const ComplexMatrix &data)
{
    std::vector<double> out(static_cast<size_t>(data.nr));
    for (int g = 0; g < data.nr; ++g) {
        double s = 0;
        for (int p = 0; p < data.np; ++p)
            s += std::norm(data.at(g, p));
        out[static_cast<size_t>(g)] =
            10.0 * std::log10(imax(s / imax(1, data.np), 1e-30));
    }
    return out;
}

#endif
