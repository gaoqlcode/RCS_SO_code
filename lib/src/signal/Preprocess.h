#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "rcs/types.hpp"

class Preprocess {
public:
    static void removeDc(ComplexMatrix &data);
    static void removeDirectWave(ComplexMatrix &data, double pulseWidthUs, double fs);
};

#endif
