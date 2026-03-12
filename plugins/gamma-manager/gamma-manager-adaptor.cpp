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

#include "gamma-manager-adaptor.h"

GmAdaptor::GmAdaptor(QObject *parent) :
    QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
    qRegisterMetaType<OutputGammaInfo>("OutputGammaInfo");
    qRegisterMetaType<OutputGammaInfoList>("OutputGammaInfoList");
    qDBusRegisterMetaType<OutputGammaInfo>();
    qDBusRegisterMetaType<OutputGammaInfoList>();
}

GmAdaptor::~GmAdaptor()
{

}

int GmAdaptor::setColorTemperature(QString appName, int colorTemp)
{
    int out0;
    USD_LOG(LOG_DEBUG," appName:%s", appName.toLatin1().data());
    QMetaObject::invokeMethod(parent(), "setColorTemperature", Q_RETURN_ARG(int, out0), Q_ARG(int, colorTemp));
    return out0;
}

int GmAdaptor::setScreenBrightness(QString appName, QString screenName, int screenBrightness)
{
    int out0;
    USD_LOG(LOG_DEBUG," appName:%s", appName.toLatin1().data());
    QMetaObject::invokeMethod(parent(), "setScreenBrightness", Q_RETURN_ARG(int, out0),  Q_ARG(QString, screenName), Q_ARG(int, screenBrightness));
    return out0;
}

int GmAdaptor::setAllScreenBrightness(QString appName, int screenBrightness)
{
    int out0;
    USD_LOG(LOG_DEBUG," appName:%s", appName.toLatin1().data());
    QMetaObject::invokeMethod(parent(), "setAllScreenBrightness", Q_RETURN_ARG(int, out0), Q_ARG(int, screenBrightness));
    return out0;
}

OutputGammaInfo GmAdaptor::getScreensGamma(QString appName)
{
    OutputGammaInfo out0;
    USD_LOG(LOG_DEBUG," appName:%s", appName.toLatin1().data());
    QMetaObject::invokeMethod(parent(), "getScreensGamma", Q_RETURN_ARG(OutputGammaInfo, out0));
    return out0;
}

OutputGammaInfoList GmAdaptor::getScreensGammaList(QString appName)
{
    OutputGammaInfoList out0;
    USD_LOG(LOG_DEBUG," appName:%s", appName.toLatin1().data());
    QMetaObject::invokeMethod(parent(), "getScreensGammaList", Q_RETURN_ARG(OutputGammaInfoList, out0));
    return out0;
}

QHash<QString, QVariant> GmAdaptor::getScreensGammaInfo()
{
    QHash<QString, QVariant> out0;
//    QMetaObject::invokeMethod(parent(), "getScreensGammaInfo", Q_RETURN_ARG(QHash<QString, QVariant>, out0));
    return out0;
}
