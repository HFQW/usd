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
#include "pingpongtest.h"

PingPongTest::PingPongTest(QObject *parent):m_pGetTimer(nullptr),
    m_NAM(nullptr)
{
    m_pGetTimer = new QTimer(this);
    m_pGmHelper = new GmHelper(this);
    connect(m_pGetTimer, SIGNAL(timeout()), this, SLOT(doTimerOut()), Qt::DirectConnection);
}

void PingPongTest::setTestUrlList(QStringList urlList)
{
    m_urlList = urlList;
}

void PingPongTest::start()
{
    m_pGetTimer->start(1000);
}

void PingPongTest::doNAMFinished(QNetworkReply *reply)
{
    static int retryTimes = 0;

    QTime stopTime = QTime::currentTime();
    QVariant statusCodeV =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    // Or the target URL if it was a redirect:
    QVariant redirectionTargetUrl =
            reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    // see CS001432 on how to handle this

    // no error received?
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray bytes = reply->readAll();  // bytes
        QString msg = QString::fromUtf8(bytes);

        if (m_pGmHelper->getLonAndLatByJson(m_currentUrl, bytes, m_location)) {
            USD_LOG(LOG_DEBUG,"location(%0.4f,%0.4f)",m_location.width(), m_location.height());
            //return;
        }
        setBeyondTime(m_currentUrl, m_connectTime.msecsTo(stopTime));
        //USD_LOG(LOG_DEBUG,"elpased:%d:%s @%d", m_connectTime.msecsTo(stopTime), msg.toLatin1().data(),retryTimes);
    }  else {
        USD_LOG(LOG_DEBUG,"elpased:%d error!%d",m_connectTime.msecsTo(stopTime),reply->error());
        setBeyondTime(m_currentUrl, m_connectTime.msecsTo(stopTime),1);
    }

}

void PingPongTest::doTimerOut()
{
    QUrl url;
    if (m_RTurl >= m_urlList.count()) {
        m_RTurl = 0;
    }

    if(m_NAM == nullptr) {
       m_NAM = new QNetworkAccessManager(this);
       connect(m_NAM, SIGNAL(finished(QNetworkReply*)),this, SLOT(doNAMFinished(QNetworkReply*)));
    }

    m_currentUrl = m_urlList[m_RTurl];
    url.setUrl(m_currentUrl);
    m_NAM->get(QNetworkRequest(url));
    m_connectTime = QTime::currentTime();
    m_RTurl++;
}

void PingPongTest::doNAMTimerOut()
{

}

void PingPongTest::setBeyondTime(QString url, int time, int error)
{
START:
    Q_FOREACH(testInfo *info, m_testDataList) {
        if (url==info->url){
            if (error){
                info->m_miss++;
            }
            else if (time < 101) {
                info->m_less100ms++;
            } else if (time < 1001) {
                info->m_less1000ms++;
            } else if (time < 2001) {
                info->m_less2000ms++;
            } else if (time < 3001) {
                info->m_less3000ms++;
            } else if (time < 4001) {
                info->m_less4000ms++;
            } else {
                info->m_exceed4000ms++;
            }
            info->testTimes++;
            info->totalTime += time;
            info->avgTime = info->totalTime / info->testTimes;
            //USD_LOG(LOG_DEBUG,"..");
            USD_LOG(LOG_DEBUG,"url[%s],100:%d,less 1000:%d,less 2000:%d,less 3000:%d,less 4000:%d, exceed 4000:%d,avg:%0.2f, times:%d(miss:%d)",info->url.toLatin1().data(),
                            info->m_less100ms,info->m_less1000ms,info->m_less2000ms,info->m_less3000ms,info->m_less4000ms, info->m_exceed4000ms,info->avgTime,info->testTimes,info->m_miss);
            return;
        }
    }
    testInfo *pinfo = new testInfo(this);
    pinfo->url = url;
    m_testDataList.append(pinfo);
    goto START;
}

