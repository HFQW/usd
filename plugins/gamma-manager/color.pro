QT -= gui
QT += network

TEMPLATE = lib
TARGET = color
DEFINES += DEMO_LIBRARY

#link_pkgconfig no_keywords plugin app_bundlels 必须要加否则会出现so文件版本的链接
CONFIG += c++11  link_pkgconfig no_keywords plugin app_bundlels

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS MODULE_NAME=\\\"color\\\"
#DEFINES += TEST_HTTP
INCLUDEPATH += \
        -I ukui-settings-daemon/

include($$PWD/../../common/common.pri)
# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    gamma-manager-adaptor.cpp \
    gamma-manager-dbus.cpp \
    gamma-manager-gtkconfig.cpp \
    gamma-manager-helper.cpp \
    gamma-manager-location.cpp \
    gamma-manager-plugin.cpp \
    gamma-manager-thread.cpp \
    gamma-manager-wayland.cpp \
    gamma-manager.cpp \
    pingpongtest.cpp

HEADERS += \
    gamma-color-info.h \
    gamma-manager-adaptor.h \
    gamma-manager-dbus.h \
    gamma-manager-define.h \
    gamma-manager-gtkconfig.h \
    gamma-manager-helper.h \
    gamma-manager-location.h \
    gamma-manager-plugin.h \
    gamma-manager-thread.h \
    gamma-manager-wayland.h \
    gamma-manager.h \
    pingpongtest.h \
    rgb-gamma-table.h

# Default rules for deployment.
#    OTHER_FILES += org.ukui.peripherals-keyboard.demoplugin.xml


    demoplugin_lib.path = $${PLUGIN_INSTALL_DIRS}
    demoplugin_lib.files =  $$OUT_PWD/libcolor.so

    demoplugin_info.path = $$[QT_INSTALL_LIBS]/ukui-settings-daemon
    demoplugin_info.files = $$OUT_PWD/color.ukui-settings-plugin

    demoplugin_schema.path = /usr/share/glib-2.0/schemas/
    demoplugin_schema.files = $$OUT_PWD/org.ukui.SettingsDaemon.plugins.color.gschema.xml

#禁用优化
QMAKE_CFLAGS_DEBUG -= O2
QMAKE_CXXFLAGS_DEBUG -= -O2

QMAKE_CFLAGS_RELEASE -= O2
QMAKE_CXXFLAGS_RELEASE -= -O2

#lib must at last
INSTALLS +=  demoplugin_info demoplugin_schema demoplugin_lib

DISTFILES += \
    color.ukui-settings-plugin \
    org.ukui.SettingsDaemon.plugins.color.gschema.xml
