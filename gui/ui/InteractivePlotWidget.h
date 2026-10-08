#ifndef INTERACTIVEPLOTWIDGET_H
#define INTERACTIVEPLOTWIDGET_H

#include <QWidget>
#include <QVector>
#include <QString>

class InteractivePlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit InteractivePlotWidget(QWidget *parent = nullptr);
    void setAxisTitles(const QString &xTitle, const QString &yTitle);
    void setPlotTitle(const QString &title);
    void setData(const QVector<double> &x, const QVector<double> &y);
    void resetView();

signals:
    void cursorInfo(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRect plotRect() const;
    void updateBounds();
    void clampViewRanges();
    void zoomAt(const QPoint &pos, double factor);
    bool mapPixelToData(const QPoint &pos, double &x, double &y) const;
    QPointF mapDataToPixel(double x, double y) const;

    QVector<double> x_, y_;
    QString xTitle_, yTitle_, title_;
    double xMin_ = 0, xMax_ = 1, yMin_ = 0, yMax_ = 1;
    double vx0_ = 0, vx1_ = 1, vy0_ = 0, vy1_ = 1;
    bool panning_ = false;
    QPoint lastPos_;
};

#endif
