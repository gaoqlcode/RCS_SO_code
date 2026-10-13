QT += core gui widgets
CONFIG += c++11
TARGET = sdk207_demo_gui
TEMPLATE = app

SDK_ROOT = $$PWD/..

INCLUDEPATH += $$SDK_ROOT/include
LIBS += -L$$SDK_ROOT/lib -lrcs_proc -pthread
QMAKE_RPATHDIR += $$SDK_ROOT/lib

HEADERS += \
    MainWindow.h \
    CalMosaicWorker.h \
    RectPickDialog.h

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    CalMosaicWorker.cpp \
    RectPickDialog.cpp
