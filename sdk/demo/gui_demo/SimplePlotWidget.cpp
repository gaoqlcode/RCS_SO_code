#include "SimplePlotWidget.h"
#include <QPainter>
#include <QtMath>

SimplePlotWidget::SimplePlotWidget(QWidget *parent)
    : QWidget(parent), fixedY_(false), yLo_(0), yHi_(1)
{
    setMinimumSize(400, 220);
    setAutoFillBackground(true);
}

void SimplePlotWidget::setAxisTitles(const QString &x, const QString &y)
{
    xTitle_ = x;
    yTitle_ = y;
    update();
}

void SimplePlotWidget::setTitle(const QString &t)
{
    title_ = t;
    update();
}

void SimplePlotWidget::setYRange(double lo, double hi)
{
    fixedY_ = (lo < hi);
    yLo_ = lo;
    yHi_ = hi;
    update();
}

void SimplePlotWidget::setData(const QVector<double> &x, const QVector<double> &y)
{
    x_ = x;
    y_ = y;
    update();
}

void SimplePlotWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);

    const QRect plot(70, 36, width() - 90, height() - 70);
    p.setPen(Qt::black);
    p.drawRect(plot);
    p.drawText(QRect(0, 6, width(), 24), Qt::AlignCenter, title_);
    p.drawText(QRect(plot.left(), height() - 28, plot.width(), 20), Qt::AlignCenter, xTitle_);

    if (x_.size() < 2 || y_.size() < 2)
        return;

    double xmin = x_[0], xmax = x_[0], ymin = y_[0], ymax = y_[0];
    const int n = qMin(x_.size(), y_.size());
    for (int i = 1; i < n; ++i) {
        xmin = qMin(xmin, x_[i]);
        xmax = qMax(xmax, x_[i]);
        ymin = qMin(ymin, y_[i]);
        ymax = qMax(ymax, y_[i]);
    }
    if (fixedY_) {
        ymin = yLo_;
        ymax = yHi_;
    }
    if (qFuzzyCompare(xmin, xmax)) {
        xmin -= 1;
        xmax += 1;
    }
    if (qFuzzyCompare(ymin, ymax)) {
        ymin -= 1;
        ymax += 1;
    }

    p.setPen(QPen(QColor(30, 90, 180), 1.5));
    QPolygonF line;
    for (int i = 0; i < n; ++i) {
        const double nx = (x_[i] - xmin) / (xmax - xmin);
        const double ny = (y_[i] - ymin) / (ymax - ymin);
        line << QPointF(plot.left() + nx * plot.width(), plot.bottom() - ny * plot.height());
    }
    p.drawPolyline(line);

    p.setPen(Qt::black);
    p.drawText(QRect(4, plot.top(), 60, 16), Qt::AlignRight, QString::number(ymax, 'f', 1));
    p.drawText(QRect(4, plot.bottom() - 16, 60, 16), Qt::AlignRight, QString::number(ymin, 'f', 1));

    p.save();
    p.translate(14, plot.center().y());
    p.rotate(-90);
    p.drawText(QRect(-plot.height() / 2, -10, plot.height(), 20), Qt::AlignCenter, yTitle_);
    p.restore();
}
