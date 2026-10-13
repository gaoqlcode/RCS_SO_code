#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QThread;
class CalMosaicWorker;

/**
 * SDK_207 GUI 示例：定标 / 拼接调用方式与正式工程 Worker 一致
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
    void onBrowseToZeroSrc();
    void onBrowseToZeroOut();
    void onStartToZero();
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

    QLineEdit *tzSrcEdit_;
    QLineEdit *tzOutEdit_;
    QCheckBox *tzRecCheck_;
    QPushButton *tzStartBtn_;

    QPushButton *cancelBtn_;
    QProgressBar *progress_;
    QPlainTextEdit *log_;

    QThread *workerThread_;
    CalMosaicWorker *worker_;
    bool busy_;
};

#endif
