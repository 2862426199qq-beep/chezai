QT       += core gui widgets
TARGET    = radar_app
TEMPLATE  = app
CONFIG   += c++14

SOURCES += \
    main.cpp \
    radar_widget.cpp \
    radar_reader.cpp \
    ../middleware/radar_processor.cpp

HEADERS += \
    radar_widget.h \
    radar_reader.h \
    ../middleware/radar_processor.h

INCLUDEPATH += ../middleware
