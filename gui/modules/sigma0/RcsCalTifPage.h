#ifndef RCSCALTIFPAGE_H
#define RCSCALTIFPAGE_H

#include <QWidget>
#include <QThread>
#include <QImage>
#include "RcsCalTifWorker.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class LogProgressPanel;
class OutDirField;

class RcsCalTifPage : public QWidget {
    Q_OBJECT
public:
    explicit RcsCalTifPage(QWidget *parent = 0);
    ~RcsCalTifPage() override;

private slots:
    void onBrowseCalib();
    void onBrowseFolder();
    void onBrowseL0();
    void onBrowseMerge();
    void onStart();
    void onCancel();
    void onNeedRectRoi(const QImage &preview, int fullNr, int fullNa, const QString &stage);
    void onReady(const RcsCalTifResultQt &r);

private:
    void setBusy(bool busy);

    QLineEdit *calibEdit_;
    QLineEdit *folderEdit_;
    QLineEdit *l0Edit_;
    QLineEdit *mergeEdit_;
    OutDirField *outDir_;
    QLineEdit *nameEdit_;
    QComboBox *bandCombo_;
    QComboBox *polCombo_;
    QDoubleSpinBox *sigmaSpin_;
    QDoubleSpinBox *fsSpin_;
    QDoubleSpinBox *incSpin_;
    QDoubleSpinBox *hSpin_;
    QSpinBox *maxFilesSpin_;
    QPushButton *startBtn_;
    QPushButton *cancelBtn_;
    LogProgressPanel *log_;
    QLabel *resultLabel_;

    QThread *thread_;
    RcsCalTifWorker *worker_;
};

#endif
