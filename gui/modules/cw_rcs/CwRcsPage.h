#ifndef CWRCSPAGE_H
#define CWRCSPAGE_H

#include <QWidget>
#include <QThread>
#include <QImage>
#include "CwRcsWorker.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class LogProgressPanel;
class InteractivePlotWidget;
class SharedDataSession;

class CwRcsPage : public QWidget {
    Q_OBJECT
public:
    explicit CwRcsPage(SharedDataSession *session, QWidget *parent = 0);
    ~CwRcsPage() override;

private slots:
    void onBrowseOut();
    void onStart();
    void onCancel();
    void onDataFolderChanged(const QString &path);
    void onNeedCorner(const QVector<double> &R, const QVector<double> &profileDb, int peakIdx);
    void onNeedTargetPick(const QVector<double> &R, const QImage &preview, int fullNr, int fullNp,
                          const QVector<double> &meanPowerDb, int modeHint);
    void onReady(const CwRcsResultQt &bundle);

private:
    void setBusy(bool busy);
    void refreshDefaultOut();
    UserSelection runTargetDialog(const QVector<double> &R, const QVector<double> &meanPowerDb);

    SharedDataSession *session_;
    QLineEdit *outEdit_;
    QLineEdit *nameEdit_;
    QComboBox *polCombo_;
    QDoubleSpinBox *sigmaSpin_;
    QPushButton *startBtn_;
    QPushButton *cancelBtn_;
    LogProgressPanel *log_;
    InteractivePlotWidget *plot_;
    QLabel *cursorLabel_;
    QString lastAutoOut_;
    bool outDirUserEdited_;

    QThread *thread_;
    CwRcsWorker *worker_;
};

#endif
