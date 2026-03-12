#-------------------------------------------------
#
# Project created by QtCreator 2020-11-12T15:37:31
#
#-------------------------------------------------

QT       +=xml core gui dbus network KWindowSystem KScreen

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = save-param
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS MODULE_NAME=\\\"save-param\\\"

QMAKE_CFLAGS_DEBUG -= O2
QMAKE_CXXFLAGS_DEBUG -= -O2

QMAKE_CFLAGS_RELEASE -= O2
QMAKE_CXXFLAGS_RELEASE -= -O2


LIBS += -lX11


CONFIG += link_pkgconfig
CONFIG += C++11

PKGCONFIG += libxklavier \
        x11 \
        xrandr \
        xtst \
        atk \
        xi \
        xrandr  \
        gudev-1.0


# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

target_sp.files += $$TARGET
target_sp.path = /usr/bin


INSTALLS +=  \
            target_sp

SOURCES += \
    calibrate-touch-device.cpp \
    lightdm-config-helper.cpp \
    save-screen.cpp \
    xrandr-config.cpp \
    xrandr-output.cpp   \
    main.cpp

HEADERS += \
    calibrate-touch-device.h \
    color-temp-info.h \
    lightdm-config-helper.h \
    main.h \
    save-screen.h \
    xrandr-config.h \
    xrandr-output.h



TRANSLATIONS +=  \
                res/zh_CN.ts \

include($$PWD/../../common/common.pri)
