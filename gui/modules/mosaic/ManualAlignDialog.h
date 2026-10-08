#ifndef MANUALALIGNDIALOG_H
#define MANUALALIGNDIALOG_H

#include <QDialog>
#include <QImage>

class QLabel;
class QSlider;
class QComboBox;

/**
 * @brief 手动拼接逐对对齐：双层灰度叠图，调透明度，拖动后一张对齐
 * 返回全分辨率位移 (dRow, dCol)：后一张相对前一张
 */
class ManualAlignDialog : public QDialog {
    Q_OBJECT
public:
    ManualAlignDialog(const QImage &frontPreview, const QImage &backPreview,
                      int previewStep, int pairIndex, int pairTotal,
                      QWidget *parent = nullptr);

    int dRowFull() const { return dRowFull_; }
    int dColFull() const { return dColFull_; }
    bool skipRestAuto() const { return skipRest_; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void rebuildComposite();
    void applyNudge(int dRowPrev, int dColPrev);

    QImage front_;
    QImage back_;
    int step_ = 8;
    int dRowPrev_ = 0; // 预览像素：后相对前
    int dColPrev_ = 0;
    int dRowFull_ = 0;
    int dColFull_ = 0;
    bool skipRest_ = false;
    bool dragging_ = false;
    QPoint lastPos_;

    QLabel *view_ = nullptr;
    QLabel *info_ = nullptr;
    QSlider *alphaFront_ = nullptr;
    QSlider *alphaBack_ = nullptr;
    QSlider *bright_ = nullptr;
    QComboBox *nudgeStep_ = nullptr;
};

#endif
