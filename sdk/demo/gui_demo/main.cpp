// =============================================================================
// main.cpp — 程序入口
// 职责：创建 QApplication、注册跨线程信号要用的元类型、弹出主窗口、进入事件循环。
// 这里不调用 SDK；真正调库在 DemoWorker（工作线程）里。
// =============================================================================

#include "MainWindow.h"   // 主窗口：控件布局、线程与 Worker 的接线
#include <QApplication>   // Qt 应用程序对象：事件循环、全局设置
#include <QMetaType>      // qRegisterMetaType：让自定义类型能进信号队列

// argc/argv：命令行参数，交给 QApplication（可处理 -style 等 Qt 参数）
int main(int argc, char **argv)
{
    // 创建应用对象：必须在任何控件之前；一个进程通常只有一个
    QApplication app(argc, argv);

    // PreviewBundle 会通过 signal 从 Worker 线程传到 UI 线程。
    // 跨线程排队传递时，Qt 需要知道如何拷贝/识别该类型，故注册元类型。
    // 名称字符串要与信号里写的类型名一致。
    qRegisterMetaType<PreviewBundle>("PreviewBundle");

    // QVector<PreviewBundle> 作为 previewDone 的参数，同样要注册
    // 注意模板参数列表里的空格：有的 moc/旧编译器对 >> 敏感，写成 "> >"
    qRegisterMetaType<QVector<PreviewBundle> >("QVector<PreviewBundle>");

    // 构造主窗口（内部会创建线程、Worker、ConfirmBridge、所有控件）
    MainWindow w;

    // 显示窗口（默认构造后是隐藏的）
    w.show();

    // 进入 Qt 事件循环：处理鼠标、重绘、信号槽排队等；窗口关闭后返回退出码
    return app.exec();
}
