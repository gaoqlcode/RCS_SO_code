#ifndef MANUALALIGNDIALOG_H
#define MANUALALIGNDIALOG_H

#include <QDialog>
#include <QImage>

class QLabel;
class QSlider;
class QComboBox;

/** 手动拼接逐对对齐。返回后一张相对前一张的全分辨率位移。 */
class ManualAlignDialog : public QDialog {
    Q_OBJECT
public:
    ManualAlignDialog(const QImage &frontPreview, const QImage &backPreview, int previewStep,
                      int pairIndex, int pairTotal, QWidget *parent = 0);

    int dRowFull() const { return dRowFull_; }
    int dColFull() const { return dColFull_; }
    bool skipRestAuto() const { return skipRest_; }

protected:
    void keyPressEvent(QKeyEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

private:
    void rebuildComposite();
    void applyNudge(int dRowPrev, int dColPrev);

    QImage front_;
    QImage back_;
    int step_;
    int dRowPrev_;
    int dColPrev_;
    int dRowFull_;
    int dColFull_;
    bool skipRest_;
    bool dragging_;
    QPoint lastPos_;

    QLabel *view_;
    QLabel *info_;
    QSlider *alphaFront_;
    QSlider *alphaBack_;
    QSlider *bright_;
    QComboBox *nudgeStep_;
};

#endif
