#ifndef TOZEROPAGE_H
#define TOZEROPAGE_H

#include <QWidget>
#include <QThread>
#include "ToZeroWorker.h"

class QLineEdit;
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
    void onStart();
    void onCancel();
    void onReady(const ToZeroResultQt &r);

private:
    void setBusy(bool busy);
    void refreshDefaultOut();
    QString buildOutputBasePath() const;

    QLineEdit *srcEdit_;
    OutDirField *outDir_;
    QLineEdit *baseNameEdit_;
    QCheckBox *saveRecCheck_;
    QPushButton *startBtn_;
    QPushButton *cancelBtn_;
    LogProgressPanel *log_;
    QLabel *resultLabel_;

    QThread *thread_;
    ToZeroWorker *worker_;
};

#endif
