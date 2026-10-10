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

    /** 固定纵轴范围（频域常用 -60~3）；lo>=hi 表示取消固定 */
    void setFixedYRange(double lo, double hi);

signals:
    void cursorInfo(const QString &text);
    /** 短点击（未拖拽），用于「下一组」等 */
    void plotClicked();
    void hoverEntered();
    void hoverLeft();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRect plotRect() const;
    void updateBounds();
    void clampViewRanges();
    void zoomAt(const QPoint &pos, double factor);
    bool mapPixelToData(const QPoint &pos, double &x, double &y) const;
    QPointF mapDataToPixel(double x, double y) const;
    /** 按鼠标数据 x 吸附到最近采样点；成功则写 snapX_/snapY_ */
    bool snapToCurve(double dataX, double &sx, double &sy) const;

    QVector<double> x_, y_;
    QString xTitle_, yTitle_, title_;
    double xMin_ = 0, xMax_ = 1, yMin_ = 0, yMax_ = 1;
    double vx0_ = 0, vx1_ = 1, vy0_ = 0, vy1_ = 1;
    bool panning_ = false;
    bool dragged_ = false;
    QPoint pressPos_;
    QPoint lastPos_;
    bool fixedY_ = false;
    double fixedY0_ = 0, fixedY1_ = 1;
    bool showCrosshair_ = false;
    double snapX_ = 0;
    double snapY_ = 0;
};

#endif
