#ifndef MERGEDAT_H
#define MERGEDAT_H

#include <functional>
#include <string>
#include <vector>

struct MergeSidecar {
    std::vector<int> frameFileIdx;
    std::vector<std::vector<int> > fileTimes; // 每文件 [年,月,日,时,分,秒,毫秒]
    std::vector<int> fileRadarAz;
    std::vector<int> fileRadarRoll;
    std::vector<std::string> usedFiles;
};

// 从 sidecar 首末文件头时间拼 timeSpanTag（同则单戳，异则首尾拼接无分隔符）
std::string mergeTimeSpanTag(const MergeSidecar &side);
// 可读区间：YYYY-MM-DD HH:MM:SS 或 起 ~ 止
std::string mergeTimeSpanPretty(const MergeSidecar &side);
// 首文件时刻对应的当日毫秒
double mergeStartTimeMs(const MergeSidecar &side);

class MergeDat {
public:
    static bool mergeFolder(const std::string &folder, const std::string &polSel,
                            std::string &outMergedPath, MergeSidecar *side = 0,
                            std::string *err = 0, const std::string &mergedDir = std::string(),
                            std::function<bool()> cancelled = std::function<bool()>());
};

#endif
