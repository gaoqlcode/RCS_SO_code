#ifndef LEVEL3TEXT_H
#define LEVEL3TEXT_H

#include "rcs/types.hpp"
#include <string>
#include <vector>

class Level3Text {
public:
    static bool saveHrrp(const std::string &path, const std::vector<double> &R,
                         const std::vector<double> &rcsDbsm, const std::vector<double> &phaseDeg,
                         const Level3Info &info, std::string *err = nullptr);

    // rangeSpec：点目标写数值字符串；扩展目标写 "起~止"
    static bool saveRcs(const std::string &path, const std::vector<double> &rcsDbsm,
                        const std::vector<double> &azView, const std::vector<std::string> &rangeSpec,
                        double timeMs, double radarAz, double radarRoll, const Level3Info &info,
                        std::string *err = 0);

    // 点频：第1列频率(GHz)
    static bool saveCwRcs(const std::string &path, const std::vector<double> &freqGHz,
                          const std::vector<double> &rcsDbsm, const std::vector<double> &tmsMs,
                          const std::vector<double> &azDeg, const std::vector<double> &rollDeg,
                          const std::string &rangeCol, const Level3Info &info, std::string *err = 0);

    // 后向散射 .cs：[DATA] 时间(ms) 散射系数(dB)，每目标一行
    static bool saveCs(const std::string &path, const std::vector<double> &timeMs,
                       const std::vector<double> &sigma0Db, const Level3Info &info,
                       std::string *err = 0);
};

#endif
