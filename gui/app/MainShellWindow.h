#ifndef MAINSHELLWINDOW_H
#define MAINSHELLWINDOW_H

#include <QMainWindow>

class SharedDataSession;

class MainShellWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainShellWindow(QWidget *parent = 0);

private:
    SharedDataSession *session_;
};

#endif
