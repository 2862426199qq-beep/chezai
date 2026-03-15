QT += core gui widgets
TARGET = camera_qt
TEMPLATE = app
SOURCES += main.cpp mainwindow.cpp camera_thread.cpp
HEADERS += mainwindow.h camera_thread.h

# RGA 硬件加速（与 camera_thread.h 中的 USE_RGA 宏配套）
# 如果注释掉 USE_RGA，这里也要注释掉
INCLUDEPATH += /usr/include/rga
LIBS += -lrga
