// =============================================================================
// SimplePlotWidget.h — 极简折线图控件
// 用途：把 SDK 返回的 range/amp 等 QVector 画成线；正式产品可换成专业图库。
// =============================================================================

#ifndef SIMPLEPLOTWIDGET_H
#define SIMPLEPLOTWIDGET_H

#include <QWidget>  // 自定义绘制的基类
#include <QVector>  // 存放曲线采样点
#include <QString>  // 标题与坐标轴文字

// 简单折线图，demo 够用；正式界面可换成别的
class SimplePlotWidget : public QWidget {
    Q_OBJECT // 虽无信号槽，保留 Q_OBJECT 便于以后扩展；无也可，但与工程风格一致
public:
    // parent：纳入对象树，随父销毁
    explicit SimplePlotWidget(QWidget *parent = 0);

    // 设置横轴、纵轴标题（仅显示用，不参与数据）
    void setAxisTitles(const QString &x, const QString &y);

    // 设置图上方标题（如「时域 峰 xxx us」）
    void setTitle(const QString &t);

    // 设置曲线数据：x/y 点数应大致一致；内部取较小长度绘制
    void setData(const QVector<double> &x, const QVector<double> &y);

    // 锁定纵轴范围；若 lo>=hi 则视为不锁定，按数据自动缩放
    void setYRange(double lo, double hi);

protected:
    // Qt 要求重绘时调用；在这里用 QPainter 画框、坐标文字和折线
    void paintEvent(QPaintEvent *) override;

private:
    QVector<double> x_;   // 横轴数据（如时间 μs、频率 MHz、距离 m）
    QVector<double> y_;   // 纵轴数据（幅度、dB、RCS dBsm）
    QString xTitle_;      // 横轴标题
    QString yTitle_;      // 纵轴标题
    QString title_;       // 图标题
    bool fixedY_;         // true：使用 yLo_/yHi_；false：按数据 min/max
    double yLo_;          // 锁定时的纵轴下限
    double yHi_;          // 锁定时的纵轴上限
};

#endif
