#ifndef RECTPICKDIALOG_H
#define RECTPICKDIALOG_H

#include "rcs/types.hpp"
#include <QDialog>
#include <QImage>
#include <QPoint>
#include <QRect>

class QLabel;
class QRubberBand;
class QSlider;
class QSpinBox;
class QScrollArea;

// 框选全分辨率 RectRoi：可最大化、图像拉伸铺满视口、滚轮缩放、自动色阶+白场微调
class RectPickDialog : public QDialog {
    Q_OBJECT
public:
    RectPickDialog(const QImage &preview, int fullNr, int fullNa, const QString &title,
                   QWidget *parent = 0);

    RectRoi roi() const { return roi_; }
    bool acceptedOk() const { return ok_; }

protected:
    bool eventFilter(QObject *obj, QEvent *ev);
    void resizeEvent(QResizeEvent *ev);
    void showEvent(QShowEvent *ev);

private slots:
    void onWhiteChanged(int white);
    void fitView();
    void resetZoom();

private:
    void rebuildToneMapped();
    void refreshDisplay();
    void finish(bool ok);
    QPointF labelToImage(const QPoint &p) const;
    QRect labelRectToImage(const QRect &r) const;
    int suggestWhite(const QImage &src) const;

    QScrollArea *scroll_;
    QLabel *label_;
    QRubberBand *band_;
    QSlider *whiteSlider_;
    QSpinBox *whiteSpin_;
    QPoint origin_;
    QImage source_;     ///< 输入灰度
    QImage toneMapped_; ///< 色阶后
    QImage preview_;    ///< 当前用于坐标换算的逻辑图像（与 toneMapped_ 同尺寸）
    double zoom_;       ///< 相对当前模式基准的倍率
    double scaleX_;     ///< 显示像素 / 图像像素（X）
    double scaleY_;     ///< 显示像素 / 图像像素（Y）
    int offX_;
    int offY_;
    int fullNr_;
    int fullNa_;
    RectRoi roi_;
    bool ok_;
    bool dragging_;
    bool firstShow_;
    bool stretchFill_; ///< true=拉伸铺满视口；false=1:1 原图像素
};

#endif
