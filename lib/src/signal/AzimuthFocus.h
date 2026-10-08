#ifndef AZIMUTHFOCUS_H
#define AZIMUTHFOCUS_H

#include "rcs/types.hpp"
#include <functional>
#include <vector>

class AzimuthFocus {
public:
    static void apply(ComplexMatrix &data, const std::vector<double> &R, int mode, double vSar,
                      double lambda, double prf, const std::function<void(int)> &progress = nullptr,
                      const std::function<bool()> &cancelled = nullptr);
};

#endif
