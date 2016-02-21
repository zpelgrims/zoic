QT       -= gui
CONFIG += c++11

TARGET = arnoldCamera
TEMPLATE = lib

DEFINES += ARNOLDCAMERA_LIBRARY

INCLUDEPATH += \
    /home/i7210038/Arnold-4.2.11.0-linux/include \

LIBS += \
    -L/home/i7210038/Arnold-4.2.11.0-linux/bin \
    -L/usr/lib64

LIBS += \
    /home/i7210038/Arnold-4.2.11.0-linux/bin/libai.so \
    /usr/lib64/libOpenImageIO.so



SOURCES += \
    zoic.cpp

HEADERS += \

unix {
    target.path = /usr/lib
    INSTALLS += target
}

DISTFILES += \
    ../ae/aiZoicTemplate.py \
    ../bin/zoic.mtd
