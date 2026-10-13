#ifndef RECTPICKDIALOG_H
#define RECTPICKDIALOG_H

#include "rcs/types.hpp"
#include <QDialog>
#include <QImage>
#include <QPoint>
#include <QRect>

class QLabel;
class QRubberBand;
class QScrollArea;

/** 简易框选：预览坐标 → 全分辨率 RectRoi（与正式工程公式一致） */
class RectPickDialog : public QDialog {
    Q_OBJECT
public:
    RectPickDialog(const QImage &preview, int fullNr, int fullNa, const QString &title,
                   QWidget *parent = 0);
    RectRoi roi() const { return roi_; }
    bool acceptedOk() const { return ok_; }

protected:
    bool eventFilter(QObject *obj, QEvent *ev);

private:
    void refreshDisplay();
    void finish(bool ok);
    QPointF labelToImage(const QPoint &p) const;

    QScrollArea *scroll_;
    QLabel *label_;
    QRubberBand *band_;
    QPoint origin_;
    QImage preview_;
    double scaleX_;
    double scaleY_;
    int offX_;
    int offY_;
    int fullNr_;
    int fullNa_;
    RectRoi roi_;
    bool ok_;
    bool dragging_;
};

#endif
