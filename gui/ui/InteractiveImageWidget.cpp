#include "InteractiveImageWidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QFontMetrics>
#include <cmath>
#include <limits>

InteractiveImageWidget::InteractiveImageWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(500, 360);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void InteractiveImageWidget::setPreview(const QImage &previewGray, const QVector<double> &rangeM,
                                        int fullNr, int fullNp)
{
    rangeM_ = rangeM;
    fullNr_ = fullNr;
    fullNp_ = fullNp;
    img_ = previewGray.convertToFormat(QImage::Format_RGB32);
    zoom_ = 1.0;
    pan_ = QPointF(0, 0);
    picks_.clear();
    update();
}

void InteractiveImageWidget::clearPicks()
{
    if (picks_.isEmpty())
        return;
    picks_.clear();
    update();
    emit pickChanged(picks_);
    emit statusText(QStringLiteral("已清空全部选点"));
}

bool InteractiveImageWidget::undoLastPick()
{
    if (picks_.isEmpty())
        return false;
    picks_.removeLast();
    update();
    emit pickChanged(picks_);
    emit statusText(QStringLiteral("已撤销上一点，剩余 %1").arg(picks_.size()));
    return true;
}

bool InteractiveImageWidget::removeNearestPick(int az, int rg, int maxDist)
{
    if (picks_.isEmpty())
        return false;
    int best = -1;
    double bestD = std::numeric_limits<double>::max();
    for (int i = 0; i < picks_.size(); ++i) {
        const double d = std::hypot(static_cast<double>(picks_[i].az - az),
                                    static_cast<double>(picks_[i].rg - rg));
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    if (best < 0 || bestD > maxDist)
        return false;
    picks_.removeAt(best);
    update();
    emit pickChanged(picks_);
    emit statusText(QStringLiteral("已删除附近选点，剩余 %1").arg(picks_.size()));
    return true;
}

QRect InteractiveImageWidget::imageRect() const
{
    return rect().adjusted(78, 32, -18, -42);
}

void InteractiveImageWidget::clampView()
{
    zoom_ = qBound(0.35, zoom_, 12.0);
    const QRect r = imageRect();
    const double maxPanX = r.width() * 0.45 * zoom_;
    const double maxPanY = r.height() * 0.45 * zoom_;
    pan_.rx() = qBound(-maxPanX, pan_.x(), maxPanX);
    pan_.ry() = qBound(-maxPanY, pan_.y(), maxPanY);
}

bool InteractiveImageWidget::mapToFullIndex(const QPoint &pos, int &az, int &rg) const
{
    const QRect r = imageRect();
    if (!r.contains(pos) || img_.isNull() || fullNr_ <= 0 || fullNp_ <= 0)
        return false;
    const double cx = r.center().x() + pan_.x();
    const double cy = r.center().y() + pan_.y();
    const double w = r.width() * zoom_;
    const double h = r.height() * zoom_;
    const double left = cx - w / 2;
    const double top = cy - h / 2;
    const double fx = (pos.x() - left) / w;
    const double fy = (pos.y() - top) / h;
    if (fx < 0 || fx > 1 || fy < 0 || fy > 1)
        return false;
    az = qBound(0, static_cast<int>(std::llround(fx * (fullNp_ - 1))), fullNp_ - 1);
    rg = qBound(0, static_cast<int>(std::llround((1.0 - fy) * (fullNr_ - 1))), fullNr_ - 1);
    return true;
}

void InteractiveImageWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(32, 32, 32));
    const QRect r = imageRect();
    p.fillRect(r, QColor(20, 20, 20));
    p.setPen(QPen(QColor(180, 180, 180)));
    p.drawRect(r);

    if (!img_.isNull()) {
        const QSize target(static_cast<int>(r.width() * zoom_), static_cast<int>(r.height() * zoom_));
        QPoint c = r.center() + pan_.toPoint();
        QRect dest(c.x() - target.width() / 2, c.y() - target.height() / 2, target.width(), target.height());
        p.setClipRect(r.adjusted(1, 1, -1, -1));
        p.drawImage(dest, img_);

        p.setPen(QPen(Qt::red, 2));
        for (const PickPoint &pt : picks_) {
            const double fx = fullNp_ > 1 ? pt.az / double(fullNp_ - 1) : 0;
            const double fy = fullNr_ > 1 ? 1.0 - pt.rg / double(fullNr_ - 1) : 0;
            const int x = dest.left() + static_cast<int>(fx * dest.width());
            const int y = dest.top() + static_cast<int>(fy * dest.height());
            p.drawEllipse(QPoint(x, y), 6, 6);
        }
        p.setClipping(false);

        // 轴刻度：方位 / 距离
        p.setPen(QColor(220, 220, 220));
        QFont f = p.font();
        f.setPointSize(8);
        p.setFont(f);
        for (int i = 0; i <= 6; ++i) {
            const double ratio = i / 6.0;
            const int x = r.left() + static_cast<int>(ratio * r.width());
            p.drawLine(x, r.bottom(), x, r.bottom() + 5);
            const int az = static_cast<int>(ratio * qMax(0, fullNp_ - 1));
            p.drawText(QRect(x - 28, r.bottom() + 6, 56, 16), Qt::AlignHCenter, QString::number(az));
        }
        for (int i = 0; i <= 6; ++i) {
            const double ratio = i / 6.0;
            const int y = r.bottom() - static_cast<int>(ratio * r.height());
            p.drawLine(r.left() - 5, y, r.left(), y);
            const int rg = static_cast<int>(ratio * qMax(0, fullNr_ - 1));
            QString lab = QString::number(rg);
            if (rg < rangeM_.size())
                lab = QString::number(rangeM_[rg], 'f', 0);
            p.drawText(QRect(4, y - 8, r.left() - 10, 16), Qt::AlignRight | Qt::AlignVCenter, lab);
        }
        p.drawText(QRect(r.left(), height() - 18, r.width(), 16), Qt::AlignCenter,
                   QStringLiteral("方位脉冲 / 距离(m)"));
    }

    p.setPen(Qt::white);
    p.drawText(10, 18, QStringLiteral("滚轮缩放(有界) | 右键拖拽 | 左键选点 | Shift+左键删点 | Backspace撤销  已选 %1")
                           .arg(picks_.size()));
}

void InteractiveImageWidget::wheelEvent(QWheelEvent *event)
{
    zoom_ *= (event->angleDelta().y() > 0) ? 1.12 : 0.90;
    clampView();
    update();
}

void InteractiveImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        panning_ = true;
        lastPos_ = event->pos();
        return;
    }
    if (event->button() == Qt::LeftButton && pickEnabled_) {
        int az = 0, rg = 0;
        if (mapToFullIndex(event->pos(), az, rg)) {
            if (event->modifiers() & Qt::ShiftModifier) {
                removeNearestPick(az, rg);
                return;
            }
            PickPoint pt;
            pt.az = az;
            pt.rg = rg;
            picks_.push_back(pt);
            emit pickChanged(picks_);
            double rm = (rg < rangeM_.size()) ? rangeM_[rg] : rg;
            emit statusText(QStringLiteral("选点 az=%1 rg=%2 R=%.2fm").arg(az).arg(rg).arg(rm));
            update();
        }
    }
}

void InteractiveImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (panning_) {
        pan_ += event->pos() - lastPos_;
        lastPos_ = event->pos();
        clampView();
        update();
    }
}

void InteractiveImageWidget::mouseReleaseEvent(QMouseEvent *)
{
    panning_ = false;
}

void InteractiveImageWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
        undoLastPick();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        undoLastPick();
        return;
    }
    QWidget::keyPressEvent(event);
}
