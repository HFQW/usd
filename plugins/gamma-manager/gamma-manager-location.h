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

#ifndef GAMMAMANAGERLOCATION_H
#define GAMMAMANAGERLOCATION_H
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDBusInterface>
#include <QObject>
#include <QTimer>

#include "clib-syslog.h"
#include "gamma-manager-define.h"
#include "QGSettings/qgsettings.h"
#include "gamma-manager-helper.h"
#include "pingpongtest.h"

class GmLocation:public QObject
{
    Q_OBJECT
public:
    GmLocation(QObject *parent = nullptr);
    ~GmLocation();
    /**
     * @brief start
     */
    void start();

    /**
     * @brief setIpAddresses
     * @param addresses
     */
    void setIpAddresses(QStringList &addresses);//for unit test

    /**
     * @brief setGsettings 同步gsettings
     * @param settings
     */
    void setGsettings(QGSettings *settings);
public Q_SLOTS:
   /* 初始化目前只有网络接口初始化的监控，如果该接口初始化失败，下个timeout继续初始化。
      如果初始化成功，则链接信号，继续工作。
    */
    /**
     * @brief doNetWorkInterfaceTimeOut
     */
    void doNetWorkInterfaceTimeOut();

    /**
     * @brief doNetworkStateCanged
     * @param state
     */
    void doNetworkStateCanged(uint state);

    /**
     * @brief doNAMFinished
     * @param reply
     */
    void doNAMFinished(QNetworkReply *reply);

    /**
     * @brief getLocationByHttp
     */
    void getLocationByHttp();

    /**
     * @brief TestPingPongBall
     * @param url
     */
    void TestPingPongBall(QStringList url);
private:
    void setLocation(double lon, double lat);
private:
    bool                     m_initState;
    int                      m_lastNetworkState;
    int                      m_urlChangeTimes;

    QTime                    m_connectTime;
    QSizeF                   m_location;
    QStringList              m_urlList;

    QTimer                  *m_pTimer;
    QDBusInterface          *m_pNetworkBus;
    QNetworkAccessManager   *m_NAM;
    GmHelper                *m_pGmHelper;
    QGSettings              *m_pSettings;
    PingPongTest            *m_pPingPongTest;
};

#endif // GAMMAMANAGERLOCATION_H
