// =============================================================================
// ConfirmBridge.cpp — 在 UI 线程弹出「核对角反峰距」对话框
// =============================================================================

#include "ConfirmBridge.h" // 本类声明
#include <QInputDialog>    // 简易数值输入对话框（demo 够用；正式可换成自定义窗）

// suggestedM：库建议的峰距（米），填进对话框初值
bool ConfirmBridge::askCornerConfirm(double suggestedM)
{
    // ok：用户是否点了「确定」（点取消则为 false）
    bool ok = false;

    // 父窗口传 0：以桌面为父，避免跨线程拿 MainWindow 指针；demo 足够
    // 标题 / 提示文字用 QStringLiteral，编译期 UTF-16，避免源文件编码坑
    // 初值 suggestedM；范围给得很宽；小数位 3
    const double v = QInputDialog::getDouble(
        0,                                          // 父控件（nullptr）
        QStringLiteral("核对角反峰距"),               // 窗口标题
        QStringLiteral("角反峰值距离 (m)"),           // 标签说明
        suggestedM,                                 // 默认值
        -1.0e6,                                     // 最小值
        1.0e6,                                      // 最大值
        3,                                          // 小数位数
        &ok);                                       // 输出：是否确定

    // 用户取消或关闭对话框：告诉库「这次确认失败」→ processHrrp 一般会失败退出
    if (!ok)
        return false;

    // 记下用户输入的距离，供 Worker 写入 InteractSelection::cornerRangeOverrideM
    lastRangeM_ = v;

    // 返回 true：库继续后续裁剪与写文件
    return true;
}
