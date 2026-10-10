#include "InteractivePlotWidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
double safeMinSpan(double v)
{
    return qMax(1e-6, std::abs(v) * 1e-6 + 1e-6);
}
int selectTickPrecision(double span)
{
    if (span < 2.0) return 3;
    if (span < 20.0) return 2;
    if (span < 200.0) return 1;
    return 0;
}
}

InteractivePlotWidget::InteractivePlotWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setAutoFillBackground(true);
}

void InteractivePlotWidget::setAxisTitles(const QString &xTitle, const QString &yTitle)
{
    xTitle_ = xTitle;
    yTitle_ = yTitle;
    update();
}

void InteractivePlotWidget::setPlotTitle(const QString &title)
{
    title_ = title;
    update();
}

void InteractivePlotWidget::setData(const QVector<double> &x, const QVector<double> &y)
{
    x_ = x;
    y_ = y;
    showCrosshair_ = false;
    updateBounds();
    resetView();
}

bool InteractivePlotWidget::snapToCurve(double dataX, double &sx, double &sy) const
{
    const int n = qMin(x_.size(), y_.size());
    if (n <= 0)
        return false;
    // 按 x 最近邻（曲线通常单调）；若非严格单调也取 |x-dataX| 最小点
    int best = 0;
    double bestDx = std::abs(x_[0] - dataX);
    for (int i = 1; i < n; ++i) {
        const double dx = std::abs(x_[i] - dataX);
        if (dx < bestDx) {
            bestDx = dx;
            best = i;
        }
    }
    sx = x_[best];
    sy = y_[best];
    return true;
}

void InteractivePlotWidget::updateBounds()
{
    if (x_.isEmpty() || y_.isEmpty()) {
        xMin_ = 0; xMax_ = 1; yMin_ = -1; yMax_ = 1;
        return;
    }
    xMin_ = xMax_ = x_[0];
    yMin_ = yMax_ = y_[0];
    const int n = qMin(x_.size(), y_.size());
    for (int i = 0; i < n; ++i) {
        xMin_ = qMin(xMin_, x_[i]);
        xMax_ = qMax(xMax_, x_[i]);
        yMin_ = qMin(yMin_, y_[i]);
        yMax_ = qMax(yMax_, y_[i]);
    }
    if (qFuzzyCompare(xMin_, xMax_)) { xMin_ -= 1; xMax_ += 1; }
    if (qFuzzyCompare(yMin_, yMax_)) { yMin_ -= 1; yMax_ += 1; }
}

void InteractivePlotWidget::setFixedYRange(double lo, double hi)
{
    if (lo >= hi) {
        fixedY_ = false;
    } else {
        fixedY_ = true;
        fixedY0_ = lo;
        fixedY1_ = hi;
    }
    if (!x_.isEmpty())
        resetView();
    else
        update();
}

void InteractivePlotWidget::resetView()
{
    const double xPad = qMax(safeMinSpan(xMax_ - xMin_), (xMax_ - xMin_) * 0.05);
    vx0_ = xMin_ - xPad;
    vx1_ = xMax_ + xPad;
    if (fixedY_) {
        vy0_ = fixedY0_;
        vy1_ = fixedY1_;
    } else {
        const double yPad = qMax(safeMinSpan(yMax_ - yMin_), (yMax_ - yMin_) * 0.08);
        vy0_ = yMin_ - yPad;
        vy1_ = yMax_ + yPad;
    }
    clampViewRanges();
    update();
}

void InteractivePlotWidget::clampViewRanges()
{
    const double minXSpan = safeMinSpan(xMax_ - xMin_);
    const double minYSpan = fixedY_ ? safeMinSpan(fixedY1_ - fixedY0_) : safeMinSpan(yMax_ - yMin_);
    if (vx1_ - vx0_ < minXSpan) {
        const double c = 0.5 * (vx0_ + vx1_);
        vx0_ = c - 0.5 * minXSpan;
        vx1_ = c + 0.5 * minXSpan;
    }
    if (!fixedY_ && vy1_ - vy0_ < minYSpan) {
        const double c = 0.5 * (vy0_ + vy1_);
        vy0_ = c - 0.5 * minYSpan;
        vy1_ = c + 0.5 * minYSpan;
    }
    // 放大不超过数据范围过多；缩小(放大视野)也不要无限飞出
    const double maxXPad = (xMax_ - xMin_) * 2.0 + minXSpan;
    vx0_ = qMax(xMin_ - maxXPad, vx0_);
    vx1_ = qMin(xMax_ + maxXPad, vx1_);
    if (vx1_ - vx0_ > (xMax_ - xMin_) + 2 * maxXPad) {
        const double c = 0.5 * (xMin_ + xMax_);
        const double half = 0.5 * ((xMax_ - xMin_) + 2 * maxXPad);
        vx0_ = c - half;
        vx1_ = c + half;
    }
    if (!fixedY_) {
        const double maxYPad = (yMax_ - yMin_) * 2.0 + minYSpan;
        vy0_ = qMax(yMin_ - maxYPad, vy0_);
        vy1_ = qMin(yMax_ + maxYPad, vy1_);
        if (vy1_ - vy0_ > (yMax_ - yMin_) + 2 * maxYPad) {
            const double c = 0.5 * (yMin_ + yMax_);
            const double half = 0.5 * ((yMax_ - yMin_) + 2 * maxYPad);
            vy0_ = c - half;
            vy1_ = c + half;
        }
    }
}

QRect InteractivePlotWidget::plotRect() const
{
    // 刻度与轴标题都画在 plot 框外，边距必须够大，否则会被控件边界裁切
    const int left = 100;
    const int top = 44;
    const int right = 56;
    const int bottom = 64;
    return QRect(left, top, qMax(80, width() - left - right), qMax(80, height() - top - bottom));
}

bool InteractivePlotWidget::mapPixelToData(const QPoint &pos, double &x, double &y) const
{
    const QRect r = plotRect();
    if (!r.contains(pos) || r.width() <= 0 || r.height() <= 0)
        return false;
    x = vx0_ + (pos.x() - r.left()) * (vx1_ - vx0_) / r.width();
    y = vy0_ + (r.bottom() - pos.y()) * (vy1_ - vy0_) / r.height();
    return true;
}

QPointF InteractivePlotWidget::mapDataToPixel(double x, double y) const
{
    const QRect r = plotRect();
    const double nx = (x - vx0_) / qMax(1e-12, vx1_ - vx0_);
    const double ny = (y - vy0_) / qMax(1e-12, vy1_ - vy0_);
    return QPointF(r.left() + nx * r.width(), r.bottom() - ny * r.height());
}

void InteractivePlotWidget::zoomAt(const QPoint &pos, double factor)
{
    if (x_.isEmpty() || factor <= 0)
        return;
    double cx = 0, cy = 0;
    if (!mapPixelToData(pos, cx, cy)) {
        cx = 0.5 * (vx0_ + vx1_);
        cy = 0.5 * (vy0_ + vy1_);
    }
    vx0_ = cx - (cx - vx0_) * factor;
    vx1_ = cx + (vx1_ - cx) * factor;
    vy0_ = cy - (cy - vy0_) * factor;
    vy1_ = cy + (vy1_ - cy) * factor;
    clampViewRanges();
    update();
}

void InteractivePlotWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    // 稠密曲线关抗锯齿，切换「下一组」时明显更快
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), Qt::white);
    const QRect r = plotRect();
    p.fillRect(r, Qt::white);
    p.setPen(QPen(Qt::black, 1));
    p.drawRect(r);

    QFont titleFont = font();
    titleFont.setPointSize(10);
    QFont tickFont = font();
    tickFont.setPointSize(8);
    QFont labelFont = font();
    labelFont.setPointSize(9);
    p.setFont(titleFont);
    p.drawText(QRect(0, 8, width(), 24), Qt::AlignCenter, title_);

    if (vy0_ <= 0 && vy1_ >= 0) {
        p.setPen(QPen(QColor(200, 40, 40), 1));
        p.drawLine(mapDataToPixel(vx0_, 0), mapDataToPixel(vx1_, 0));
    }

    if (x_.size() >= 2) {
        p.save();
        p.setClipRect(r.adjusted(1, 1, -1, -1));
        QPolygonF poly;
        const int n = qMin(x_.size(), y_.size());
        // 按像素列抽稀：每列保留 min/max，点数从数万降到约 2*宽度
        const int maxSeg = qMax(2, r.width());
        if (n <= maxSeg * 2) {
            poly.reserve(n);
            for (int i = 0; i < n; ++i) {
                if (x_[i] < vx0_ || x_[i] > vx1_)
                    continue;
                poly << mapDataToPixel(x_[i], y_[i]);
            }
        } else {
            poly.reserve(maxSeg * 2 + 2);
            int i0 = 0;
            while (i0 < n && x_[i0] < vx0_)
                ++i0;
            int i1 = n - 1;
            while (i1 > i0 && x_[i1] > vx1_)
                --i1;
            const int span = i1 - i0 + 1;
            if (span > 0) {
                const int buckets = qMin(maxSeg, span);
                for (int b = 0; b < buckets; ++b) {
                    const int a = i0 + (b * span) / buckets;
                    const int c = i0 + ((b + 1) * span) / buckets;
                    int imin = a, imax = a;
                    for (int i = a; i < c && i <= i1; ++i) {
                        if (y_[i] < y_[imin])
                            imin = i;
                        if (y_[i] > y_[imax])
                            imax = i;
                    }
                    if (imin <= imax) {
                        poly << mapDataToPixel(x_[imin], y_[imin]);
                        if (imax != imin)
                            poly << mapDataToPixel(x_[imax], y_[imax]);
                    } else {
                        poly << mapDataToPixel(x_[imax], y_[imax]);
                        poly << mapDataToPixel(x_[imin], y_[imin]);
                    }
                }
            }
        }
        p.setPen(QPen(QColor(30, 90, 180), 1));
        if (!poly.isEmpty())
            p.drawPolyline(poly);

        // 吸附十字线：竖线落到 x 轴、横线落到 y 轴，交点在曲线采样点上
        if (showCrosshair_) {
            const QPointF pt = mapDataToPixel(snapX_, snapY_);
            p.setPen(QPen(QColor(200, 60, 40), 1, Qt::DashLine));
            p.drawLine(QPointF(pt.x(), r.top()), QPointF(pt.x(), r.bottom()));
            p.drawLine(QPointF(r.left(), pt.y()), QPointF(r.right(), pt.y()));
            p.setPen(QPen(QColor(200, 60, 40), 1));
            p.setBrush(QColor(200, 60, 40));
            p.drawEllipse(pt, 3.5, 3.5);

            // 交点旁显示吸附点坐标（与 cursorInfo 精度一致）
            const QString coord = QStringLiteral("(%1, %2)")
                                     .arg(snapX_, 0, 'f', 3)
                                     .arg(snapY_, 0, 'f', 2);
            QFont coordFont = p.font();
            coordFont.setPointSize(qMax(9, coordFont.pointSize()));
            p.setFont(coordFont);
            const QFontMetrics fm(coordFont);
            const QSize ts = fm.size(0, coord);
            const int pad = 3;
            const int gap = 8;
            int lx = static_cast<int>(pt.x()) + gap;
            int ly = static_cast<int>(pt.y()) - gap - ts.height() - pad * 2;
            if (lx + ts.width() + pad * 2 > r.right())
                lx = static_cast<int>(pt.x()) - gap - ts.width() - pad * 2;
            if (ly < r.top())
                ly = static_cast<int>(pt.y()) + gap;
            if (lx < r.left())
                lx = r.left() + 2;
            if (ly + ts.height() + pad * 2 > r.bottom())
                ly = r.bottom() - ts.height() - pad * 2 - 2;
            const QRect bg(lx, ly, ts.width() + pad * 2, ts.height() + pad * 2);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 210));
            p.drawRoundedRect(bg, 3, 3);
            p.setPen(QColor(40, 40, 40));
            p.drawText(bg.adjusted(pad, 0, -pad, 0), Qt::AlignLeft | Qt::AlignVCenter, coord);
        }
        p.restore();
    }

    p.setPen(Qt::black);
    p.setFont(tickFont);
    const int xPrec = selectTickPrecision(std::abs(vx1_ - vx0_));
    const int yPrec = std::abs(vy1_ - vy0_) < 20.0 ? 2 : 1;
    for (int i = 0; i <= 8; ++i) {
        const double ratio = i / 8.0;
        const int x = r.left() + static_cast<int>(ratio * r.width());
        const double v = vx0_ + ratio * (vx1_ - vx0_);
        p.drawLine(x, r.bottom(), x, r.bottom() + 5);
        const QString lab = QString::number(v, 'f', xPrec);
        // 首尾刻度内收对齐，避免贴边被裁切
        if (i == 0) {
            p.drawText(QRect(x, r.bottom() + 8, 72, 18), Qt::AlignLeft | Qt::AlignVCenter, lab);
        } else if (i == 8) {
            p.drawText(QRect(x - 72, r.bottom() + 8, 72, 18), Qt::AlignRight | Qt::AlignVCenter, lab);
        } else {
            p.drawText(QRect(x - 36, r.bottom() + 8, 72, 18), Qt::AlignHCenter | Qt::AlignVCenter, lab);
        }
    }
    for (int i = 0; i <= 6; ++i) {
        const double ratio = i / 6.0;
        const int y = r.bottom() - static_cast<int>(ratio * r.height());
        const double v = vy0_ + ratio * (vy1_ - vy0_);
        p.drawLine(r.left() - 5, y, r.left(), y);
        p.drawText(QRect(28, y - 10, r.left() - 36, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', yPrec));
    }

    p.setFont(labelFont);
    p.drawText(QRect(r.left(), r.bottom() + 30, r.width(), 22), Qt::AlignCenter, xTitle_);
    p.save();
    p.translate(14, r.center().y());
    p.rotate(-90);
    p.drawText(QRect(-r.height() / 2, -12, r.height(), 24), Qt::AlignCenter, yTitle_);
    p.restore();
}

void InteractivePlotWidget::wheelEvent(QWheelEvent *event)
{
    // factor<1 放大，>1 缩小；并强制 clamp，避免一滚就飞出数据范围
    const double factor = (event->angleDelta().y() > 0) ? (1.0 / 1.15) : 1.15;
    zoomAt(event->pos(), factor);
    event->accept();
}

void InteractivePlotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton
        || event->button() == Qt::RightButton) {
        panning_ = true;
        dragged_ = false;
        pressPos_ = event->pos();
        lastPos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void InteractivePlotWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (panning_) {
        const QPoint d = event->pos() - lastPos_;
        if ((event->pos() - pressPos_).manhattanLength() > 4)
            dragged_ = true;
        lastPos_ = event->pos();
        const QRect r = plotRect();
        if (r.width() > 0 && r.height() > 0 && dragged_) {
            const double dx = -d.x() * (vx1_ - vx0_) / r.width();
            const double dy = d.y() * (vy1_ - vy0_) / r.height();
            vx0_ += dx;
            vx1_ += dx;
            vy0_ += dy;
            vy1_ += dy;
            clampViewRanges();
            update();
        }
    }

    double x = 0, y = 0;
    if (mapPixelToData(event->pos(), x, y) && snapToCurve(x, snapX_, snapY_)) {
        showCrosshair_ = true;
        emit cursorInfo(QStringLiteral("x=%1  y=%2（曲线）")
                            .arg(snapX_, 0, 'f', 3)
                            .arg(snapY_, 0, 'f', 2));
        update();
    } else {
        if (showCrosshair_) {
            showCrosshair_ = false;
            update();
        }
        emit cursorInfo(QString());
    }
}

void InteractivePlotWidget::mouseReleaseEvent(QMouseEvent *event)
{
    const bool wasDrag = dragged_;
    panning_ = false;
    dragged_ = false;
    unsetCursor();
    if (!wasDrag && event->button() == Qt::LeftButton)
        emit plotClicked();
}

void InteractivePlotWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    resetView();
}

void InteractivePlotWidget::enterEvent(QEvent *)
{
    emit hoverEntered();
}

void InteractivePlotWidget::leaveEvent(QEvent *)
{
    if (showCrosshair_) {
        showCrosshair_ = false;
        update();
    }
    emit cursorInfo(QString());
    emit hoverLeft();
}
