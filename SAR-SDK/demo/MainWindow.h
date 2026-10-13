#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QDoubleSpinBox;
class QComboBox;
class QSpinBox;
class QThread;
class CalMosaicWorker;

/**
 * SAR-SDK GUI 示例：定标 / 拼接 / 零级成像，调用方式与正式工程 Worker 一致
 * （工作线程调库，主线程弹框选）。
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void onBrowseCalib();
    void onBrowseRawDir();
    void onBrowseCalOut();
    void onStartCal();
    void onBrowseMosaicIn();
    void onBrowseMosaicOut();
    void onStartMosaic();
    void onBrowseZeroSrc();
    void onBrowseZeroOut();
    void onBrowseZeroEngine();
    void onStartZeroImage();
    void onCancel();
    void appendLog(const QString &s);
    void setProgress(int p, const QString &s);
    void onFinished(bool ok, const QString &msg);
    void onNeedRectRoi(const QImage &preview, int fullNr, int fullNa);

private:
    void setBusy(bool busy);

    QLineEdit *calibEdit_;
    QLineEdit *rawDirEdit_;
    QLineEdit *calOutEdit_;
    QComboBox *bandCombo_;
    QComboBox *polCombo_;
    QDoubleSpinBox *sigmaSpin_;
    QPushButton *calStartBtn_;

    QLineEdit *mosaicInEdit_;
    QLineEdit *mosaicOutEdit_;
    QDoubleSpinBox *medSpin_;
    QPushButton *mosaicStartBtn_;

    QLineEdit *ziSrcEdit_;
    QLineEdit *ziOutEdit_;
    QLineEdit *ziEngineEdit_;
    QLineEdit *ziMcrEdit_;
    QSpinBox *ziBeginSpin_;
    QSpinBox *ziDeltaSpin_;
    QSpinBox *ziLastSpin_;
    QSpinBox *ziNumSpin_;
    QSpinBox *ziTimeoutSpin_;
    QPushButton *ziStartBtn_;

    QPushButton *cancelBtn_;
    QProgressBar *progress_;
    QPlainTextEdit *log_;

    QThread *workerThread_;
    CalMosaicWorker *worker_;
    bool busy_;
};

#endif
