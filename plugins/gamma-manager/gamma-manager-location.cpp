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
#include "gamma-manager-location.h"

/*
 *
 *
*/
GmLocation::GmLocation(QObject *parent):
    m_pTimer(nullptr),
    m_urlChangeTimes(0),
    m_NAM(nullptr),
    m_pGmHelper(nullptr)
{
    m_urlList.append(IP_API_ADDRESS);
    m_urlList.append(IP_API_ADDRESS_BACKUP);
    m_pTimer = new QTimer(this);
    m_pGmHelper = new GmHelper(this);
#ifdef TEST_HTTP
    m_pPingPongTest = new PingPongTest(this);
    m_pPingPongTest->setTestUrlList(m_urlList);
#endif
}

GmLocation::~GmLocation()
{

}

void GmLocation::start()
{
#ifdef TEST_HTTP
    m_pPingPongTest = new PingPongTest(this);
    m_pPingPongTest->setTestUrlList(m_urlList);
    m_pPingPongTest->start();
#else
    m_initState = false;
    connect(m_pTimer, SIGNAL(timeout()) ,this, SLOT(doNetWorkInterfaceTimeOut()), Qt::DirectConnection);
    m_pTimer->start(1000);
#endif
}

void GmLocation::setIpAddresses(QStringList &addresses)
{
    m_urlChangeTimes = 0;
    m_urlList = addresses;
    m_pTimer->deleteLater();
}

void GmLocation::setGsettings(QGSettings *settings)
{
    m_pSettings = settings;
}

void GmLocation::doNetWorkInterfaceTimeOut()
{
    USD_LOG(LOG_DEBUG,"check..network connect");
    m_pNetworkBus = new QDBusInterface("org.freedesktop.NetworkManager", \
                                       "/org/freedesktop/NetworkManager", \
                                       "org.freedesktop.NetworkManager", \
                                       QDBusConnection::systemBus(),this);
    if (m_pNetworkBus) {
        m_initState = true;
        m_pTimer->stop();
        disconnect(m_pTimer, SIGNAL(timeout()) ,this, SLOT(doNetWorkInterfaceTimeOut()));
        if (m_pNetworkBus->property("State").toInt() == 70) {
            USD_LOG(LOG_DEBUG,"network connect success");
            m_pTimer->setSingleShot(false);
            connect(m_pTimer, SIGNAL(timeout()) ,this, SLOT(getLocationByHttp()), Qt::DirectConnection);
            m_pTimer->start(1000);
        } else {
            m_pTimer->stop();
            connect(m_pTimer, SIGNAL(timeout()) ,this, SLOT(getLocationByHttp()), Qt::DirectConnection);
            connect(m_pNetworkBus, SIGNAL(StateChanged(uint)), this, SLOT(doNetworkStateCanged(uint)), Qt::DirectConnection);
        }
    }
}

void GmLocation::doNetworkStateCanged(uint state)
{
    if (m_lastNetworkState != state && state == 70) {//网络首次链接
        USD_LOG(LOG_DAEMON,"network had ready");
        m_lastNetworkState = state;
        m_pTimer->start(1000);
    } else if(m_lastNetworkState == 70 && state != 70) {//断开链接
        m_pTimer->stop();
    }

}

void GmLocation::doNAMFinished(QNetworkReply *reply)
{
    static int retryTimes = 0;
    USD_LOG(LOG_DEBUG, "get location already..");
    QTime stopTime = QTime::currentTime();
    QVariant statusCodeV =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    // Or the target URL if it was a redirect:
    QVariant redirectionTargetUrl =
            reply->attribute(QNetworkRequest::RedirectionTargetAttribute);


    // no error received?
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray bytes = reply->readAll();  // bytes
        QString msg = QString::fromUtf8(bytes);

        if (m_pGmHelper->getLonAndLatByJson(m_urlList[m_urlChangeTimes], bytes, m_location)) {
            m_pTimer->stop();
            retryTimes = 0;

            setLocation(m_location.width(),m_location.height());
            disconnect(m_pNetworkBus, SIGNAL(StateChanged(uint)), this, SLOT(doNetworkStateCanged(uint)));//只要同步一次信息即可
            USD_LOG(LOG_DEBUG,"location(%0.4f,%0.4f)",m_location.width(), m_location.height());

            return;
        } else{
            m_pTimer->start(1000);
        }

        USD_LOG(LOG_DEBUG,"elpased:%d:%s @%d", m_connectTime.msecsTo(stopTime), msg.toLatin1().data(),retryTimes);
    }  else {
        m_pTimer->start(1000);
        USD_LOG(LOG_DEBUG,"elpased:%d error!%d",m_connectTime.msecsTo(stopTime),reply->error());
    }

    if (retryTimes++ > 10) {//
        retryTimes = 0;
        m_urlChangeTimes++;
    }
}

void GmLocation::getLocationByHttp()
{
    QUrl url;

    m_pTimer->stop();
    if (m_urlChangeTimes >= m_urlList.count()) {
        m_urlChangeTimes = 0;
    }

    if(m_NAM == nullptr) {
       m_NAM = new QNetworkAccessManager(this);
       connect(m_NAM, SIGNAL(finished(QNetworkReply*)),this, SLOT(doNAMFinished(QNetworkReply*)));
    }

    url.setUrl(m_urlList[m_urlChangeTimes]);
    m_NAM->get(QNetworkRequest(url));
    USD_LOG(LOG_DEBUG, "ready get location..:%d", m_urlChangeTimes);
    m_connectTime = QTime::currentTime();
}

void GmLocation::TestPingPongBall(QStringList ppbUrl)
{
    QUrl url;
    if(m_NAM == nullptr) {
       m_NAM = new QNetworkAccessManager(this);

    }

    url.setUrl(m_urlList[m_urlChangeTimes]);
    m_NAM->get(QNetworkRequest(url));
}

void GmLocation::setLocation(double lon, double lat)
{
    QVariant qVar;
    QVariantList qvarList;
    QSizeF sunTime;
    QSizeF location;

    location.setWidth(lon);
    location.setHeight(lat);

    qvarList.append(lon);
    qvarList.append(lat);
    qVar = qvarList;

    if (m_pSettings) {
        m_pGmHelper->getRtSunriseSunset(location, sunTime);
        m_pSettings->set(COLOR_KEY_LAST_COORDINATES, qVar);
        m_pSettings->set(COLOR_KEY_AUTOMATIC_FROM, sunTime.width());
        m_pSettings->set(COLOR_KEY_AUTOMATIC_TO, sunTime.height());
    }
}
