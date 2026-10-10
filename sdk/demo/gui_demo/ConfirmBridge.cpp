#include "ConfirmBridge.h"
#include <QInputDialog>

bool ConfirmBridge::askCornerConfirm(double suggestedM)
{
    bool ok = false;
    const double v = QInputDialog::getDouble(
        0, QStringLiteral("核对角反峰距"),
        QStringLiteral("角反峰值距离 (m)"),
        suggestedM, -1.0e6, 1.0e6, 3, &ok);
    if (!ok)
        return false;
    lastRangeM_ = v;
    return true;
}
