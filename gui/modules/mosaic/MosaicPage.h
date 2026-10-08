#ifndef MOSAICPAGE_H
#define MOSAICPAGE_H

#include <QWidget>
#include <QThread>
#include "MosaicWorker.h"

class QLineEdit;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QComboBox;
class QLabel;
class LogProgressPanel;
class FlattenPanel;

class MosaicPage : public QWidget {
    Q_OBJECT
public:
    explicit MosaicPage(QWidget *parent = nullptr);
    ~MosaicPage() override;

private slots:
    void onStart();
    void onCancel();
    void onFlatten(const QString &path, double medOut);
    void onReady(const MosaicResult &result);

private:
    void setBusy(bool busy);
    void refreshDefaultOut();
    /** 逐对手动对齐，成功返回 true，并填好绝对偏移 */
    bool collectManualOffsets(const QString &inFolder, const QString &outFolder,
                              QVector<int> &offR, QVector<int> &offC,
                              int rawNrHint, int rawNaHint);

    QLineEdit *inEdit_ = nullptr;
    QLineEdit *outEdit_ = nullptr;
    QComboBox *modeCombo_ = nullptr;
    QCheckBox *flattenCheck_ = nullptr;
    QDoubleSpinBox *medSpin_ = nullptr;
    QSpinBox *rawNrSpin_ = nullptr;
    QSpinBox *rawNaSpin_ = nullptr;
    QPushButton *startBtn_ = nullptr;
    QPushButton *cancelBtn_ = nullptr;
    LogProgressPanel *log_ = nullptr;
    QLabel *resultLabel_ = nullptr;
    FlattenPanel *flattenPanel_ = nullptr;
    QString lastAutoOut_;
    bool outDirUserEdited_ = false;

    QThread *thread_ = nullptr;
    MosaicWorker *worker_ = nullptr;
};

#endif
