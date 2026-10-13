#ifndef TOZEROPAGE_H
#define TOZEROPAGE_H

#include <QWidget>
#include <QThread>
#include "ToZeroWorker.h"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class LogProgressPanel;
class OutDirField;

class ToZeroPage : public QWidget {
    Q_OBJECT
public:
    explicit ToZeroPage(QWidget *parent = 0);
    ~ToZeroPage();

private slots:
    void onBrowseSource();
    void onBrowseEngine();
    void refreshDefaultOut();
    QString buildOutputBasePath() const;
    void onStart();
    void onCancel();
    void onReady(const ToZeroResultQt &r);

private:
    void setBusy(bool busy);
    int spinValue(QSpinBox *box) const;

    QLineEdit *srcEdit_;
    OutDirField *outDir_;
    QLineEdit *baseNameEdit_;
    QLineEdit *engineEdit_;
    QLineEdit *mcrEdit_;
    QSpinBox *beginSpin_;
    QSpinBox *deltaSpin_;
    QSpinBox *lastSpin_;
    QSpinBox *numSpin_;
    QSpinBox *offsetSpin_;
    QSpinBox *testSpin_;
    QSpinBox *aperSpin_;
    QSpinBox *tarYSpin_;
    QSpinBox *iq1Spin_;
    QSpinBox *iq2Spin_;
    QSpinBox *rangeSpin_;
    QSpinBox *errStartSpin_;
    QSpinBox *errLenSpin_;
    QSpinBox *timeoutSpin_;
    QCheckBox *engineCheck_;
    QPushButton *startBtn_;
    QPushButton *cancelBtn_;
    LogProgressPanel *log_;
    QLabel *resultLabel_;

    QThread *thread_;
    ToZeroWorker *worker_;
};

#endif
