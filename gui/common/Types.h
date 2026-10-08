#ifndef TYPES_H
#define TYPES_H

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QPair>

// 界面侧结果/选点（Qt 类型）。
// 算法库用 InteractSelection、HrrpResponse 等，两边别共用同名结构体，否则头文件一混就撞车。

struct HrrpResult {
    QVector<double> rangeM;
    QVector<double> rcsDbsm;
    QVector<double> phaseDeg;
    QString pdatPath;
    QString hrrpPath;
};

struct RcsTargetResult {
    double rcsDbsm;
    double azViewDeg;
    double rangeM;
    bool valid;
    QString label;

    RcsTargetResult() : rcsDbsm(0), azViewDeg(0), rangeM(0), valid(false) {}
};

struct RcsResultBundle {
    QVector<RcsTargetResult> targets;
    QString pdatPath;
    QString rcsPath;
    QVector<double> rangeM;
    QVector<double> profileDbsm;
};

struct PickPoint {
    int az; // 脉冲下标
    int rg; // 距离门下标
    PickPoint() : az(0), rg(0) {}
};

struct UserSelection {
    QVector<PickPoint> corners;
    QVector<PickPoint> targets;
    int targetMode; // 1 点目标  2 扩展
    QVector<QPair<double, double> > extendedRanges;
    double cornerRangeOverrideM;

    UserSelection() : targetMode(1), cornerRangeOverrideM(-1) {}
};

Q_DECLARE_METATYPE(HrrpResult)
Q_DECLARE_METATYPE(RcsResultBundle)
Q_DECLARE_METATYPE(UserSelection)
Q_DECLARE_METATYPE(QVector<PickPoint>)

void registerRcsMetaTypes();

#endif
