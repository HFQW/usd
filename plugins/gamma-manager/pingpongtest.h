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

#ifndef PINGPONGTEST_H
#define PINGPONGTEST_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDBusInterface>
#include <QObject>
#include <QTimer>

#include "clib-syslog.h"
#include "gamma-manager-define.h"
#include "QGSettings/qgsettings.h"
#include "gamma-manager-helper.h"

class testInfo:public QObject
{
    Q_OBJECT
public:
    testInfo(QObject *parent = nullptr){
        url = "";
        m_less100ms = 0;
        m_less1000ms = 0;
        m_less2000ms = 0;
        m_less3000ms = 0;
        m_less4000ms = 0;
        m_exceed4000ms = 0;
        testTimes = 0;
        totalTime = 0;
        avgTime = 0;
        m_miss = 0;
    }
    QString url;
    int m_less100ms;
    int m_less1000ms;
    int m_less2000ms;
    int m_less3000ms;
    int m_less4000ms;
    int m_exceed4000ms;
    int testTimes;
    qulonglong totalTime;
    double avgTime;
    int m_miss;
};

class PingPongTest: public QObject
{
    Q_OBJECT
public:
    PingPongTest(QObject *parent =nullptr);

    void setTestUrlList(QStringList urlList);
    void start();
public Q_SLOTS:
    void doNAMFinished(QNetworkReply *reply);
    void doTimerOut();
    void doNAMTimerOut();
private:
    void setBeyondTime(QString url, int time, int error = 0);
private:
    QStringList m_urlList;
    QString     m_currentUrl;
    int m_RTurl;
    QTimer      *m_pGetTimer;
    QTimer      *m_NAMTimr;
    QTime       m_connectTime;
    GmHelper                *m_pGmHelper;
    QNetworkAccessManager   *m_NAM;
    QSizeF                   m_location;
    QList<testInfo*> m_testDataList;
};

#endif // PINGPONGTEST_H
