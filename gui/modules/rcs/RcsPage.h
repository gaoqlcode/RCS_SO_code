#ifndef RCSPAGE_H
#define RCSPAGE_H

#include <QWidget>
#include <QThread>
#include <QImage>
#include "RcsWorker.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class LogProgressPanel;
class InteractivePlotWidget;
class SharedDataSession;
class OutDirField;

class RcsPage : public QWidget {
    Q_OBJECT
public:
    explicit RcsPage(SharedDataSession *session, QWidget *parent = 0);
    ~RcsPage() override;

private slots:
    void onBrowseFolder();
    void onStart();
    void onCancel();
    void onDataFolderChanged(const QString &path);
    void onNeedCornerPick(const QVector<double> &R, const QImage &preview, int fullNr, int fullNp);
    void onNeedTargetPick(const QVector<double> &R, const QImage &preview, int fullNr, int fullNp,
                          const QVector<double> &meanPowerDb, int modeHint);
    void onReady(const RcsResultBundle &bundle);

private:
    void setBusy(bool busy);
    void refreshDefaultOut();
    UserSelection runPickDialog(const QString &title, const QVector<double> &R, const QImage &preview,
                                int fullNr, int fullNp, const QVector<double> &meanPowerDb, bool askMode);

    SharedDataSession *session_;
    QLineEdit *folderEdit_;
    OutDirField *outDir_;
    QLineEdit *nameEdit_;
    QComboBox *polCombo_;
    QComboBox *azModeCombo_;
    QDoubleSpinBox *vSpin_;
    QDoubleSpinBox *sigmaSpin_;
    QSpinBox *cropSpin_;
    QPushButton *startBtn_;
    QPushButton *cancelBtn_;
    LogProgressPanel *log_;
    InteractivePlotWidget *plot_;
    QLabel *cursorLabel_;

    QThread *thread_;
    RcsWorker *worker_;
};

#endif
