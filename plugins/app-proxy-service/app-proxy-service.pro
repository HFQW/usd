QT       += core gui KWindowSystem

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = lib
TARGET = app-proxy-service

DEFINES += APPPROXYSERVICE_LIBRARY MODULE_NAME=\\\"app-proxy-service\\\"


CONFIG += c++11 plugin
include($$PWD/../../common/common.pri)


# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    $$PWD/app-proxy-service-plugin.cpp \
    $$PWD/proxy-service-manager.cpp

HEADERS += \
    $$PWD/app-proxy-service-plugin.h \
    $$PWD/proxy-service-manager.h

# Default rules for deployment.
app-proxy-service_lib.path = $${PLUGIN_INSTALL_DIRS}
app-proxy-service_lib.files = $$OUT_PWD/libapp-proxy-service.so

INSTALLS += app-proxy-service_lib
