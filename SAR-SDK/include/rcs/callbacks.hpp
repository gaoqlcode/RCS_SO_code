#ifndef RCS_CALLBACKS_HPP
#define RCS_CALLBACKS_HPP

#include "rcs/types.hpp"
#include <functional>
#include <string>
#include <vector>

// =============================================================================
// Callbacks（顺序勿改）
// SDK 实际会用到：onProgress / isCancelled / onMessage / onError / onRectRoi
// 其余成员为 ABI 占位，本交付不实现。
// =============================================================================

struct Callbacks {
    /**
     * 进度回调（建议实现）。
     * @param percent  0～100
     * @param message  当前步骤说明（UTF-8）
     */
    std::function<void(int percent, const std::string &message)> onProgress;

    /**
     * 取消查询（建议实现）。
     * @return true 表示用户请求中止，库尽快返回 false 且 *err 可能为「已取消」
     */
    std::function<bool()> isCancelled;

    /**
     * 一般信息日志（可选）。
     * @param message  UTF-8 文本
     */
    std::function<void(const std::string &message)> onMessage;

    /**
     * 错误日志（可选；部分路径可能只写 *err 而不调此回调）。
     * @param message  UTF-8 文本
     */
    std::function<void(const std::string &message)> onError;
/////////////////////////////////////////中间部分不用实现//////////////////////////////////////////////
    /**
     * 【本交付可不实现】HRRP/点频角点确认（ABI 占位）。
     */
    std::function<bool(const std::vector<double> &, const std::vector<double> &, int, InteractSelection &)>
        onCornerConfirm;

    /**
     * 【本交付可不实现】RCS 测量角点框选（ABI 占位）。
     */
    std::function<bool(const std::vector<double> &, const GrayImage &, int, int, InteractSelection &)>
        onCornerPick;

    /**
     * 【本交付可不实现】RCS 测量目标点选（ABI 占位）。
     */
    std::function<bool(const std::vector<double> &, const GrayImage &, int, int,
                       const std::vector<double> &, int, InteractSelection &)>
        onTargetPick;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * 辐射定标：框选角反缓冲区。
     * 当 processRcsCalTif 且 doCalib=true 且 autoPick=false 时【必填】。
     *
     * @param preview  降采样 8bit 预览（定标库内步长约 8）
     * @param fullNr   全图距离向行数
     * @param fullNa   全图方位向列数
     * @param stage    阶段标记，一般为 "cal"
     * @param out      【输出】须填全分辨率 RectRoi：x=方位列，y=距离行，w/h>0
     * @return true 表示框选有效并继续；false 表示取消/失败
     *
     * 预览坐标 → 全分辨率：
     *   sx = fullNa / preview.width;  sy = fullNr / preview.height;
     *   out.x = floor(px * sx); out.y = floor(py * sy);
     *   out.w = max(1, floor(pw * sx)); out.h = max(1, floor(ph * sy));
     */
    std::function<bool(const GrayImage &preview, int fullNr, int fullNa, const std::string &stage,
                       RectRoi &out)>
        onRectRoi;

    /**
     * 【本交付可不实现】后向散射多目标框选（ABI 占位）。
     */
    std::function<bool(const GrayImage &, int, int, std::vector<RectRoi> &outs)> onTargetRois;
};

#endif
