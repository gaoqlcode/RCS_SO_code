#ifndef LEVEL2PDAT_H
#define LEVEL2PDAT_H

#include "rcs/types.hpp"
#include <string>
#include <vector>

class Level2Pdat {
public:
    static bool save2D(const std::string &outPath, const ComplexMatrix &focused,
                       const std::vector<double> &R, double sigmaCalLin, double Rcorner, double Pcorner,
                       const FileHeaderInfo &hdr, const FrameMeta &fm,
                       const std::vector<uint16_t> &azList, int rcsScale = 100,
                       std::string *err = nullptr);

    static bool save1D(const std::string &outPath, const std::vector<double> &rcsDbsm,
                       const std::vector<double> &phaseDeg, double Rcorner, double sigmaDb,
                       const FileHeaderInfo &hdr, const FrameMeta &fm, int rcsScale = 100,
                       std::string *err = nullptr);
};

#endif
