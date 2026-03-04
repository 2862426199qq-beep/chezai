QT       += core gui widgets network
TARGET    = radar_app
TEMPLATE  = app
CONFIG   += c++14

SOURCES += \
    main.cpp \
    BluetoothAudio.cpp \
    radar_widget.cpp \
    radar_reader.cpp \
    ../middleware/radar_processor.cpp

HEADERS += \
    BluetoothAudio.h \
    radar_widget.h \
    radar_reader.h \
    ../middleware/radar_processor.h

INCLUDEPATH += ../middleware
