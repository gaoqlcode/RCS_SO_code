#ifndef INTERACTIVEIMAGEWIDGET_H
#define INTERACTIVEIMAGEWIDGET_H

#include <QWidget>
#include <QVector>
#include <QImage>
#include "Types.h"

class InteractiveImageWidget : public QWidget {
    Q_OBJECT
public:
    explicit InteractiveImageWidget(QWidget *parent = nullptr);

    /**
     * @brief 设置选点预览图（灰度）；点击坐标映射回全分辨率 (fullNr×fullNp)
     */
    void setPreview(const QImage &previewGray, const QVector<double> &rangeM,
                    int fullNr, int fullNp);
    void clearPicks();
    bool undoLastPick();
    bool removeNearestPick(int az, int rg, int maxDist = 40);
    QVector<PickPoint> picks() const { return picks_; }
    void setPickEnabled(bool on) { pickEnabled_ = on; }

signals:
    void pickChanged(const QVector<PickPoint> &pts);
    void statusText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRect imageRect() const;
    bool mapToFullIndex(const QPoint &pos, int &az, int &rg) const;
    void clampView();

    QVector<double> rangeM_;
    int fullNr_ = 0, fullNp_ = 0;
    QImage img_;
    QVector<PickPoint> picks_;
    bool pickEnabled_ = true;
    double zoom_ = 1.0;
    QPointF pan_{0, 0};
    bool panning_ = false;
    QPoint lastPos_;
};

#endif
