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

    /**
     * 由定标 dBsm 图写二级 .pdat。
     * fileHeader 为 0 级 512 字节文件头，frame0 为首帧 256 字节帧头。
     * dbsm 行主序，长度 nr*na；每个方位列一帧，相位填 0。
     */
    static bool saveCalDbsm(const std::string &outPath, const std::vector<uint8_t> &fileHeader,
                            const std::vector<uint8_t> &frame0, int nr, int na,
                            const std::vector<float> &dbsm, double RcalM, double sigmaDb,
                            int rcsScale = 100, std::string *err = nullptr);
};

#endif
