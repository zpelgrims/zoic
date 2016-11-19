QT       -= gui
CONFIG += c++11

TARGET = arnoldCamera
TEMPLATE = lib

DEFINES += ARNOLDCAMERA_LIBRARY

INCLUDEPATH += \
    /Volumes/ZENO_2016/misc/Arnold-4.2.14.3-darwin/include \

LIBS += \
    -L/Volumes/ZENO_2016/misc/Arnold-4.2.14.3-darwin/bin \

LIBS += \
    /Volumes/ZENO_2016/misc/Arnold-4.2.14.3-darwin/bin/libai.dylib \

SOURCES += \
    zoic.cpp \

DISTFILES += \
    /Volumes/ZENO_2016/projects/zoic/maya/ae/aiZoicTemplate.py \
    /Volumes/ZENO_2016/projects/zoic/src/zoic.mtd \
    draw.py \
    draw.zoic
