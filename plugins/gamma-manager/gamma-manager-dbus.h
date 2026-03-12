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
#ifndef GMDBUS_H
#define GMDBUS_H
#include <QObject>
#include <QDBusArgument>
#include "clib-syslog.h"
#include "gamma-manager-adaptor.h"

class GmDbus: public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", DBUS_GM_INTERFACE)
public:
    GmDbus(QObject* parent = 0);
    ~GmDbus();
public Q_SLOTS:
    /**
     * @brief setScreenBrightness
     * @param appName
     * @param screenName
     * @param screenBrightness
     * @return
     */
    int setScreenBrightness(QString appName, QString screenName,unsigned int screenBrightness);

    /**
     * @brief setAllScreenBrightness
     * @param appName
     * @param screenBrightness
     * @return
     */
    int setAllScreenBrightness(QString appName,unsigned int screenBrightness);

    /**
     * @brief getScreensGamma
     * @param appName
     * @return
     */
    OutputGammaInfo getScreensGamma(QString appName);

    /**
     * @brief getScreensGammaList
     * @param appName
     * @return
     */
    OutputGammaInfoList getScreensGammaList(QString appName);

    /**
     * @brief getScreensGammaInfo
     * @param appName
     * @return
     */
    QHash<QString, QVariant> getScreensGammaInfo(QString appName);

    /**
     * @brief setColorTemperature
     * @param appName
     * @param colorTemp
     * @return
     */
    int setColorTemperature(QString appName,unsigned int colorTemp);

Q_SIGNALS:
    void screenGammaChanged(QString screenName, int screenBrightness, int screenGamma);
    void screenBrightnessChanged(QString screenName, int screenBrightness);
    void allScreenBrightnessChanged(QString screenName,int screenBrightness);
};

#endif // GMDBUS_H
