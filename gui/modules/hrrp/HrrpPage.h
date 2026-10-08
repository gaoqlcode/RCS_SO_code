#ifndef HRRPPAGE_H
#define HRRPPAGE_H

#include <QWidget>
#include <QThread>
#include "HrrpWorker.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class LogProgressPanel;
class InteractivePlotWidget;
class SharedDataSession;

class HrrpPage : public QWidget {
    Q_OBJECT
public:
    explicit HrrpPage(SharedDataSession *session, QWidget *parent = 0);
    ~HrrpPage() override;

private slots:
    void onBrowseOut();
    void onStart();
    void onCancel();
    void onDataFolderChanged(const QString &path);
    void onNeedCorner(const QVector<double> &R, const QVector<double> &profileDb, int peakIdx);
    void onReady(const HrrpResult &result);

private:
    void setBusy(bool busy);
    void refreshDefaultOut();

    SharedDataSession *session_;
    QLineEdit *outEdit_;
    QLineEdit *nameEdit_;
    QComboBox *polCombo_;
    QDoubleSpinBox *sigmaSpin_;
    QSpinBox *cropSpin_;
    QPushButton *startBtn_;
    QPushButton *cancelBtn_;
    LogProgressPanel *log_;
    InteractivePlotWidget *plot_;
    QString lastAutoOut_;
    bool outDirUserEdited_;

    QThread *thread_;
    HrrpWorker *worker_;
};

#endif
