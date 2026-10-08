#ifndef LEVEL0DAT_H
#define LEVEL0DAT_H

#include "rcs/types.hpp"
#include <functional>
#include <string>
#include <vector>

class Level0Dat {
public:
    static constexpr double kFsForced = 1.25e9;
    static constexpr double kC = 3e8;

    static bool parseHeader(const std::vector<uint8_t> &hdr512, FileHeaderInfo &out,
                            std::string *err = nullptr);
    static bool readFile(const std::string &path, FileHeaderInfo &hdr, FrameMeta &frame0,
                         ComplexMatrix &data, std::vector<uint16_t> &azList, int phaseMode = 0,
                         int maxFrame = -1, std::function<void(int)> progress = nullptr,
                         std::string *err = nullptr, std::function<bool()> cancelled = nullptr);

    static void buildRangeAxis(const FrameMeta &fm, std::vector<double> &R);
};

#endif
