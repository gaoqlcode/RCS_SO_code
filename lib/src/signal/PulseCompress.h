#ifndef PULSECOMPRESS_H
#define PULSECOMPRESS_H

#include "rcs/types.hpp"
#include <functional>

class PulseCompress {
public:
    static void apply(ComplexMatrix &data, double bandwidthMHz, double pulseWidthUs, double fs,
                      const std::function<void(int)> &progress = nullptr,
                      const std::function<bool()> &cancelled = nullptr);
};

#endif
