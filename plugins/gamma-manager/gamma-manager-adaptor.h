/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2023 KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef GAMMAMANAGERADAPTOR_H
#define GAMMAMANAGERADAPTOR_H
#include <QtCore/QObject>
#include <QtDBus/QtDBus>
#include <QMetaType>
#include <QDBusMetaType>
#include "clib-syslog.h"

QT_BEGIN_NAMESPACE
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;
QT_END_NAMESPACE



struct OutputGammaInfo{
    QString OutputName;
    uint    Gamma;
    uint    Brignthess;
    uint    Temperature;
};

typedef QList<OutputGammaInfo> OutputGammaInfoList;

Q_DECLARE_METATYPE(OutputGammaInfo)
Q_DECLARE_METATYPE(OutputGammaInfoList)

inline QDBusArgument &operator << (QDBusArgument &argument, const OutputGammaInfo &outputInfo)
{
    argument.beginStructure();
    argument << outputInfo.OutputName;
    argument << outputInfo.Gamma;
    argument << outputInfo.Brignthess;
    argument << outputInfo.Temperature;
    argument.endStructure();

    return argument;
}

inline const QDBusArgument &operator >> (const QDBusArgument &argument, OutputGammaInfo &outputInfo) {
    argument.beginStructure();
    argument >> outputInfo.OutputName;
    argument >> outputInfo.Gamma;
    argument >> outputInfo.Temperature;
    argument >> outputInfo.Brignthess;
    argument.endStructure();
    return argument;
}


class GmAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", DBUS_GM_INTERFACE)
    Q_CLASSINFO("D-Bus Introspection", ""
                                       "  <interface name=\"org.ukui.SettingsDaemon.gammaManager\">\n"
                                       "    <signal name=\"screensGammaChanged\">\n"
                                       "        <arg direction=\"out\" type=\"s\" name=\"screenName\"/>\n"
                                       "        <arg direction=\"out\" type=\"i\" name=\"screensBrightness\"/>\n"
                                       "        <arg direction=\"out\" type=\"i\" name=\"screenGamma\"/>\n"
                                       "    </signal>\n"
                                       "    <method name=\"setScreenBrightness\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"appName\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"screenName\"/>\n"
                                       "      <arg direction=\"in\" type=\"i\" name=\"screenBrightness\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"setAllScreenBrightness\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"appName\"/>\n"
                                       "      <arg direction=\"in\" type=\"i\" name=\"screenBrightness\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"setColorTemperature\">\n"
                                       "      <arg direction=\"out\" type=\"i\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"appName\"/>\n"
                                       "      <arg direction=\"in\" type=\"i\" name=\"temp\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"getScreensGamma\">\n"
                                       "      <annotation name=\"org.qtproject.QtDBus.QtTypeName.Out\" value=\"OutputGammaInfo\"/>\n"
                                       "      <arg direction=\"out\" type=\"(suu)\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"appName\"/>\n"
                                       "    </method>\n"
                                       "    <method name=\"getScreensGammaList\">\n"
                                       "      <annotation name=\"org.qtproject.QtDBus.QtTypeName.Out0\" value=\"OutputGammaInfoList\"/>\n"
                                       "      <arg direction=\"out\" type=\"a{suu}\"/>\n"
                                       "      <arg direction=\"in\" type=\"s\" name=\"appName\"/>\n"
                                       "    </method>\n"
                                       "     <method name=\"getScreensGammaInfo\">\n"
                                       "        <annotation name=\"org.qtproject.QtDBus.QtTypeName.Out\" value=\"QHash<QString,QVariant>\"/>\n"
                                       "        <arg type=\"a{sv}\" direction=\"out\"/>\n"
                                       "    </method>\n"
                                       "  </interface>\n"
                                       "")
public:
    GmAdaptor(QObject *parent = nullptr);
    ~GmAdaptor();

public Q_SLOTS: // METHODS
    int setColorTemperature(QString appName, int screenBrightness);
    int setScreenBrightness(QString appName, QString screenName, int screenBrightness);
    int setAllScreenBrightness(QString appName, int screenBrightness);
    OutputGammaInfo getScreensGamma(QString appName);
    OutputGammaInfoList getScreensGammaList(QString appName);
    QHash<QString, QVariant> getScreensGammaInfo();
Q_SIGNALS:
    void screenGammChanged(QString screenName, int screenBrightness, int screenGamma);
};




#endif // GAMMAMANAGERADAPTOR_H
