#ifndef SIMPLEPLOTWIDGET_H
#define SIMPLEPLOTWIDGET_H

#include <QWidget>
#include <QVector>
#include <QString>

// 简单折线图，demo 够用；正式界面可换成别的
class SimplePlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit SimplePlotWidget(QWidget *parent = 0);
    void setAxisTitles(const QString &x, const QString &y);
    void setTitle(const QString &t);
    void setData(const QVector<double> &x, const QVector<double> &y);
    void setYRange(double lo, double hi); // lo>=hi 就不锁纵轴

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<double> x_, y_;
    QString xTitle_, yTitle_, title_;
    bool fixedY_;
    double yLo_, yHi_;
};

#endif
