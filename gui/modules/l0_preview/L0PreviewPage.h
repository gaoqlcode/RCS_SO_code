#ifndef L0PREVIEWPAGE_H
#define L0PREVIEWPAGE_H

#include <QWidget>
#include <QThread>
#include <QVector>
#include "L0PreviewWorker.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class LogProgressPanel;
class InteractivePlotWidget;
class SharedDataSession;

class L0PreviewPage : public QWidget {
    Q_OBJECT
public:
    explicit L0PreviewPage(SharedDataSession *session, QWidget *parent = 0);
    ~L0PreviewPage() override;

private slots:
    void onBrowseFolder();
    void onStart();
    void onCancel();
    void onDataFolderChanged(const QString &path);
    void onReady(const QVector<L0PulsePreviewItem> &items);
    void showIndex(int idx);
    void nextItem();
    void prevItem();

private:
    void setBusy(bool busy);

    SharedDataSession *session_;
    QLineEdit *folderEdit_;
    QComboBox *polCombo_;
    QSpinBox *pulseSpin_;
    QDoubleSpinBox *fsSpin_;
    QCheckBox *dcCheck_;
    QPushButton *startBtn_;
    QPushButton *cancelBtn_;
    QPushButton *prevBtn_;
    QPushButton *nextBtn_;
    LogProgressPanel *log_;
    QLabel *fileLabel_;
    InteractivePlotWidget *timePlot_;
    InteractivePlotWidget *freqPlot_;

    QVector<L0PulsePreviewItem> items_;
    int index_;

    QThread *thread_;
    L0PreviewWorker *worker_;
};

#endif
