// =============================================================================
// MainWindow.h — 主窗口：布局控件、创建线程/Worker、把用户操作派到后台
// =============================================================================

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "DemoWorker.h" // PreviewBundle、DemoWorker 信号类型（connect 需要完整类型）
#include <QMainWindow>  // 带菜单栏/状态栏框架的主窗（本 demo 只用中央部件）
#include <QThread>      // 工作线程：承载 DemoWorker 的事件循环

// 前向声明：头文件里只存指针，减少编译依赖；实现里再 #include 完整头
class QLineEdit;
class QComboBox;
class QPushButton;
class QTextEdit;
class QLabel;
class QProgressBar;
class SimplePlotWidget;
class ConfirmBridge;

class MainWindow : public QMainWindow {
    Q_OBJECT // moc：识别 private slots 与以后可能加的信号
public:
    // parent 一般为 0：顶级窗口
    explicit MainWindow(QWidget *parent = 0);

    // 析构：请求取消、quit 线程并 wait，避免库还在跑时销毁对象
    ~MainWindow() override;

private slots:
    // 下列槽绑定到按钮 clicked（同线程，直接调即可）

    void onBrowseIn();    // 选输入数据目录
    void onBrowseOut();   // 选 HRRP 输出目录
    void onRefreshPol();  // 调用 listAvailablePolarizations 刷新下拉框
    void onProbe();       // Queued → worker_->runProbe
    void onPreview();     // Queued → worker_->runPreview
    void onHrrp();        // Queued → worker_->runHrrp
    void onCancel();      // worker_->requestCancel()
    void onPrevFile();    // 预览列表上一个文件
    void onNextFile();    // 预览列表下一个文件
    void showPreviewIndex(int idx); // 按索引刷新时域/频域图与文件名标签
    void setBusy(bool busy);        // true：禁用业务按钮、启用取消

private:
    // ---- 路径与极化 ----
    QLineEdit *inEdit_;   // 输入目录
    QLineEdit *outEdit_;  // 输出目录
    QComboBox *polCombo_; // 极化 HH/HV/VV/VH

    // ---- 操作按钮 ----
    QPushButton *probeBtn_;
    QPushButton *previewBtn_;
    QPushButton *hrrpBtn_;
    QPushButton *cancelBtn_;
    QPushButton *prevBtn_; // 预览：上一个文件
    QPushButton *nextBtn_; // 预览：下一个文件

    // ---- 信息与日志 ----
    QLabel *fileLabel_;   // 当前预览文件名
    QLabel *infoLabel_;   // 探查/HRRP 文字结果
    QTextEdit *log_;      // 滚动日志
    QProgressBar *bar_;   // 0~100 进度

    // ---- 三张图 ----
    SimplePlotWidget *timePlot_; // 时域
    SimplePlotWidget *freqPlot_; // 频域
    SimplePlotWidget *hrrpPlot_; // HRRP 距离像

    // ---- 线程与库调用 ----
    QThread *thread_;              // Worker 所在线程
    DemoWorker *worker_;           // 无 parent，moveToThread 后由 deleteLater 释放
    ConfirmBridge *confirmBridge_; // 父=this，住在 UI 线程

    // ---- 预览缓存（UI 侧翻页，不再调库）----
    QVector<PreviewBundle> previewItems_;
    int previewIndex_; // 当前显示的预览下标
};

#endif
