#ifndef SIGMA0PAGE_H
#define SIGMA0PAGE_H

#include <QWidget>
#include <QThread>
#include <QImage>
#include "Sigma0Worker.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class LogProgressPanel;
class OutDirField;

class Sigma0Page : public QWidget {
    Q_OBJECT
public:
    explicit Sigma0Page(QWidget *parent = 0);
    ~Sigma0Page() override;

private slots:
    void onBrowseRaw();
    void onStart();
    void onCancel();
    void onNeedRectRoi(const QImage &preview, int fullNr, int fullNa, const QString &stage);
    void onNeedTargetRois(const QImage &preview, int fullNr, int fullNa);
    void onReady(const Sigma0ResultQt &r);

private:
    void setBusy(bool busy);
    void refreshDefaultOut();
    bool pickOneRoi(const QImage &preview, int fullNr, int fullNa, const QString &title,
                    QRect *out);

    QLineEdit *rawEdit_;
    OutDirField *outDir_;
    QLineEdit *nameEdit_;
    QLineEdit *timeEdit_;
    QSpinBox *nrSpin_;
    QSpinBox *naSpin_;
    QDoubleSpinBox *fcSpin_;
    QDoubleSpinBox *vsSpin_;
    QDoubleSpinBox *incSpin_;
    QDoubleSpinBox *hSpin_;
    QDoubleSpinBox *sigmaSpin_;
    QComboBox *tgtTypeCombo_;
    QComboBox *bgModeCombo_;
    QPushButton *startBtn_;
    QPushButton *cancelBtn_;
    LogProgressPanel *log_;
    QLabel *resultLabel_;

    QThread *thread_;
    Sigma0Worker *worker_;
};

#endif
