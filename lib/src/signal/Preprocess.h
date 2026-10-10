#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "rcs/types.hpp"

class Preprocess {
public:
    /** RCS/HRRP：每个距离门，对脉冲维去均值 */
    static void removeDc(ComplexMatrix &data);
    /** 点频（对齐 RCS_DianPin.m）：每个脉冲，沿距离向去均值 z=z-mean(z) */
    static void removeDcPerPulse(ComplexMatrix &data);
    static void removeDirectWave(ComplexMatrix &data, double pulseWidthUs, double fs);
};

#endif
