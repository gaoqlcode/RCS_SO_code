#ifndef RCS_CALLBACKS_HPP
#define RCS_CALLBACKS_HPP

#include "rcs/types.hpp"
#include <functional>
#include <string>
#include <vector>

// 进度、取消、以及 RCS/HRRP 选点回调。
// 不需要交互时：autoPick=true，或干脆不设下面几个选点函数。

struct Callbacks {
    std::function<void(int, const std::string &)> onProgress;
    std::function<bool()> isCancelled;
    std::function<void(const std::string &)> onMessage;
    std::function<void(const std::string &)> onError;

    // HRRP：核对角反峰值。可改 sel.cornerRangeOverrideM（米）
    std::function<bool(const std::vector<double> &, const std::vector<double> &, int, InteractSelection &)>
        onCornerConfirm;

    // RCS：在预览图上选角反点，写入 sel.corners（az/rg 用全分辨率下标）
    std::function<bool(const std::vector<double> &, const GrayImage &, int, int, InteractSelection &)>
        onCornerPick;

    // RCS：选目标。点目标填 targets；扩展目标设 targetMode=2 并填 extendedRanges
    std::function<bool(const std::vector<double> &, const GrayImage &, int, int,
                       const std::vector<double> &, int, InteractSelection &)>
        onTargetPick;
};

#endif
