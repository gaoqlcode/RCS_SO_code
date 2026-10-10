#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "DemoWorker.h"
#include <QMainWindow>
#include <QThread>

class QLineEdit;
class QComboBox;
class QPushButton;
class QTextEdit;
class QLabel;
class QProgressBar;
class SimplePlotWidget;
class ConfirmBridge;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow() override;

private slots:
    void onBrowseIn();
    void onBrowseOut();
    void onRefreshPol();
    void onProbe();
    void onPreview();
    void onHrrp();
    void onCancel();
    void onPrevFile();
    void onNextFile();
    void showPreviewIndex(int idx);
    void setBusy(bool busy);

private:
    QLineEdit *inEdit_;
    QLineEdit *outEdit_;
    QComboBox *polCombo_;
    QPushButton *probeBtn_;
    QPushButton *previewBtn_;
    QPushButton *hrrpBtn_;
    QPushButton *cancelBtn_;
    QPushButton *prevBtn_;
    QPushButton *nextBtn_;
    QLabel *fileLabel_;
    QLabel *infoLabel_;
    QTextEdit *log_;
    QProgressBar *bar_;
    SimplePlotWidget *timePlot_;
    SimplePlotWidget *freqPlot_;
    SimplePlotWidget *hrrpPlot_;

    QThread *thread_;
    DemoWorker *worker_;
    ConfirmBridge *confirmBridge_;
    QVector<PreviewBundle> previewItems_;
    int previewIndex_;
};

#endif
