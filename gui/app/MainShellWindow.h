#ifndef MAINSHELLWINDOW_H
#define MAINSHELLWINDOW_H

#include <QMainWindow>

class SharedDataSession;
class QLineEdit;
class QLabel;

class MainShellWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainShellWindow(QWidget *parent = 0);

private slots:
    void onBrowseDataFolder();
    void onFolderEdited();

private:
    SharedDataSession *session_;
    QLineEdit *folderEdit_;
    QLabel *folderHint_;
};

#endif
