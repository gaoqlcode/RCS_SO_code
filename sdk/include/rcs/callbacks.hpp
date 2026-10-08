#ifndef RCS_CALLBACKS_HPP
#define RCS_CALLBACKS_HPP

#include "rcs/types.hpp"
#include <functional>
#include <string>
#include <vector>

// 进度、取消、选点。做界面时：库跑在工作线程，选点回调里切回 UI 线程弹窗，
// 等用户点完再把结果写进 InteractSelection，然后 return true。
// 批处理或不想交互：请求里 autoPick=true，下面选点函数可以不填。

struct Callbacks {
    std::function<void(int, const std::string &)> onProgress; // 进度 0~100，附带一句说明
    std::function<bool()> isCancelled;                        // 返回 true 就尽快停
    std::function<void(const std::string &)> onMessage;
    std::function<void(const std::string &)> onError;

    // HRRP：核对角反峰。rangeM / powerDb 是曲线，peakIdx 是自动峰下标。
    // 可把 sel.cornerRangeOverrideM 改成确认后的距离（米），<0 表示用自动峰。
    std::function<bool(const std::vector<double> &, const std::vector<double> &, int,
                       InteractSelection &)>
        onCornerConfirm;

    // RCS：在预览图上选角反。gray 是预览灰度；fullNr/fullNp 是全分辨率尺寸。
    // 点坐标请按全分辨率下标写入 sel.corners。
    std::function<bool(const std::vector<double> &, const GrayImage &, int, int, InteractSelection &)>
        onCornerPick;

    // RCS / 点频：选目标。点目标填 sel.targets；扩展目标设 targetMode=2，填 extendedRanges（米）。
    std::function<bool(const std::vector<double> &, const GrayImage &, int, int,
                       const std::vector<double> &, int, InteractSelection &)>
        onTargetPick;
};

#endif
