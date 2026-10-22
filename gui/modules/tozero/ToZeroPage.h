#ifndef TOZEROPAGE_H
#define TOZEROPAGE_H

#include <QWidget>
#include <QThread>
#include "ToZeroWorker.h"

class QComboBox;
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
    void onKindChanged(int index);
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
    QComboBox *kindCombo_;
    QSpinBox *beginSpin_;
    QSpinBox *deltaSpin_;
    QSpinBox *lastSpin_;
    QSpinBox *numSpin_;
    QSpinBox *offsetSpin_;
    QSpinBox *rangeSpin_;
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
