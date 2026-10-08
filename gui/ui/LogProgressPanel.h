#ifndef LOGPROGRESSPANEL_H
#define LOGPROGRESSPANEL_H

#include <QWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>

class LogProgressPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogProgressPanel(QWidget *parent = nullptr);
    void appendLog(const QString &text);
    void setProgress(int value, const QString &status);
    void reset();

private:
    QTextEdit *log_ = nullptr;
    QProgressBar *bar_ = nullptr;
    QLabel *status_ = nullptr;
};

#endif
