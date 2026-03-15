QT       += core gui multimedia network concurrent
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++14

DEFINES += DISABLE_HARDWARE

TARGET   = VehicleTerminal
TEMPLATE = app

INCLUDEPATH += ../middleware

# RGA 硬件加速（与 camera_thread.h 中的 USE_RGA 宏配套）
INCLUDEPATH += /usr/include/rga
LIBS += -lrga

# MPP 硬件 H.264 编码 + ffmpeg RTMP 推流
INCLUDEPATH += /usr/include/rockchip
LIBS += -lrockchip_mpp
LIBS += -lavformat -lavcodec -lavutil

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    clock.cpp \
    dht11.cpp \
    settingwindow.cpp \
    speechrecognition.cpp \
    camera_thread.cpp \
    voice_assistant.cpp \
    mjpeg_server.cpp \
    rtmp_streamer.cpp \
    Map/baidumap.cpp \
    Map/gps.c \
    Map/uart.c \
    Monitor/ap3216.cpp \
    Monitor/capture_thread.cpp \
    Monitor/monitor.cpp \
    Music/musicplayer.cpp \
    Music/music_worker.cpp \
    Music/searchmusic.cpp \
    Weather/weather.cpp \
    Radar/radar_widget.cpp \
    Radar/radar_reader.cpp \
    ../middleware/radar_processor.cpp

HEADERS += \
    mainwindow.h \
    clock.h \
    dht11.h \
    settingwindow.h \
    speechrecognition.h \
    camera_thread.h \
    voice_assistant.h \
    mjpeg_server.h \
    rtmp_streamer.h \
    Map/baidumap.h \
    Map/gps.h \
    Map/uart.h \
    Monitor/ap3216.h \
    Monitor/capture_thread.h \
    Monitor/monitor.h \
    Music/musicplayer.h \
    Music/music_worker.h \
    Music/searchmusic.h \
    Weather/weather.h \
    Radar/radar_widget.h \
    Radar/radar_reader.h \
    ../middleware/radar_processor.h

FORMS += \
    clock.ui \
    settingwindow.ui \
    Map/baidumap.ui \
    Monitor/monitor.ui \
    Music/musicplayer.ui \
    Music/searchmusic.ui \
    Weather/weather.ui

RESOURCES += img.qrc

DISTFILES += \
    Monitor/ap3216 \
    Music/style.qss
