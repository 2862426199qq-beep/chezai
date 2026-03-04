QT       += core gui multimedia network concurrent
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++14

DEFINES += DISABLE_HARDWARE

TARGET   = VehicleTerminal
TEMPLATE = app

INCLUDEPATH += ../middleware
INCLUDEPATH += ../ai_voice/include

LIBS += -L../ai_voice/build -lai_voice_lib -lstdc++ -lpthread

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    clock.cpp \
    dht11.cpp \
    settingwindow.cpp \
    speechrecognition.cpp \
    Map/baidumap.cpp \
    Map/gps.c \
    Map/uart.c \
    Monitor/ap3216.cpp \
    Monitor/capture_thread.cpp \
    Monitor/monitor.cpp \
    Music/musicplayer.cpp \
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
    Map/baidumap.h \
    Map/gps.h \
    Map/uart.h \
    Monitor/ap3216.h \
    Monitor/capture_thread.h \
    Monitor/monitor.h \
    Music/musicplayer.h \
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
