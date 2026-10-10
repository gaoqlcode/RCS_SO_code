#ifndef RCS_CALLBACKS_HPP
#define RCS_CALLBACKS_HPP

#include "rcs/types.hpp"
#include <functional>
#include <string>
#include <vector>

// =============================================================================
// Callbacks — 进度、取消、人机交互选点（SDK 交付面向界面）
//
// 库在工作线程里同步调用这些函数。做界面时：
//   - onProgress / onMessage：切回 UI 线程刷控件
//   - 选点回调：切回 UI 弹窗，阻塞等到用户确定/取消，再写结果并 return
//   - 返回 true 继续；false 表示用户取消（库侧一般按失败退出）
// 请求默认 autoPick=false，下列选点成员需按业务实现。
// =============================================================================

struct Callbacks {
    /** 进度 0～100，附带当前步骤说明 */
    std::function<void(int, const std::string &)> onProgress;

    /** 返回 true 时库尽快停止；「取消」按钮置位后在此读取 */
    std::function<bool()> isCancelled;

    /** 一般日志（非致命） */
    std::function<void(const std::string &)> onMessage;

    /** 错误日志；最终失败仍以函数返回 false + *err 为准 */
    std::function<void(const std::string &)> onError;

    /**
     * HRRP / 点频：核对角反功率峰。
     * 参数：rangeM、powerDb 曲线、自动峰下标 peakIdx、输出 sel。
     * 可把 sel.cornerRangeOverrideM 设为确认后的距离（米）；<0 表示沿用自动峰。
     */
    std::function<bool(const std::vector<double> &, const std::vector<double> &, int, InteractSelection &)>
        onCornerConfirm;

    /**
     * RCS：在预览灰度图上选角反点。
     * 参数：rangeM、preview、fullNr(距离)、fullNp(方位)、输出 sel。
     * 点坐标请按全分辨率写入 sel.corners（az=方位, rg=距离门），不要用预览像素硬填。
     */
    std::function<bool(const std::vector<double> &, const GrayImage &, int, int, InteractSelection &)>
        onCornerPick;

    /**
     * RCS / 点频：选目标。
     * 参数：rangeM、preview、fullNr、fullNp、meanPowerDb、modeHint、输出 sel。
     * 点目标：sel.targetMode=1，填 sel.targets；
     * 扩展目标：sel.targetMode=2，填 extendedRanges（距离起止，米）。
     */
    std::function<bool(const std::vector<double> &, const GrayImage &, int, int,
                       const std::vector<double> &, int, InteractSelection &)>
        onTargetPick;

    /**
     * 后向散射：框选一个矩形。
     * @param stage "noise"（可选）/ "cal"（角反缓冲，必选）/ "bg"（背景）
     * @param fullNr 距离行数  @param fullNa 方位列数
     * preview 为降采样图；写出的 RectRoi 必须是全分辨率坐标（可用 mapPreviewRoiToFull）。
     */
    std::function<bool(const GrayImage &, int, int, const std::string &stage, RectRoi &out)>
        onRectRoi;

    /**
     * 后向散射：框选一个或多个地物目标 ROI（全分辨率坐标）。
     */
    std::function<bool(const GrayImage &, int, int, std::vector<RectRoi> &outs)> onTargetRois;
};

#endif
