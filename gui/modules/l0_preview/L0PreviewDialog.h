#ifndef L0PREVIEWDIALOG_H
#define L0PREVIEWDIALOG_H

#include "rcs/types.hpp"
#include <QDialog>
#include <QVector>

class QLabel;
class InteractivePlotWidget;

// 弹出窗：同屏时域+频域，显示文件名；点击切换下一组（不自动循环）
class L0PreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit L0PreviewDialog(const QVector<L0PulsePreviewItem> &items, QWidget *parent = 0);

private slots:
    void showIndex(int idx);
    void nextItem();
    void prevItem();

private:
    QVector<L0PulsePreviewItem> items_;
    int index_;
    QLabel *fileLabel_;
    QLabel *hintLabel_;
    InteractivePlotWidget *timePlot_;
    InteractivePlotWidget *freqPlot_;
};

#endif
