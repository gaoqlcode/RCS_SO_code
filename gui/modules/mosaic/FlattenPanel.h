#ifndef FLATTENPANEL_H
#define FLATTENPANEL_H

#include <QWidget>

class QSlider;
class QLabel;
class QPushButton;
class QLineEdit;

class FlattenPanel : public QWidget {
    Q_OBJECT
public:
    explicit FlattenPanel(QWidget *parent = nullptr);
    double medOut() const;
    void setTifPath(const QString &path);

signals:
    void applyRequested(const QString &tifPath, double medOut);

private:
    QLineEdit *pathEdit_ = nullptr;
    QSlider *slider_ = nullptr;
    QLabel *valueLabel_ = nullptr;
};

#endif
