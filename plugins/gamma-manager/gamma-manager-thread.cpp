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
#include "gamma-manager-thread.h"

GmWorkThread::GmWorkThread(QObject *parent)
{
    m_rtTemperature = COLOR_TEMPERATURE_DEFAULT;
    m_lastTemperature = COLOR_TEMPERATURE_DEFAULT;
    m_targetTemp = COLOR_TEMPERATURE_DEFAULT;
    m_pSetTimer = new QTimer(this);
    m_pGmHelper = new GmHelper(this);
#if 0
    m_pSetTimer->setSingleShot(false);
    m_pSetTimer->setTimerType(Qt::PreciseTimer);
    connect(m_pSetTimer, SIGNAL(timeout()), this, SLOT(doSetTimer()), Qt::QueuedConnection);
#endif

    QDBusConnection::sessionBus().connect(QString(),
                                          QString(DBUS_XRANDR_PATH),
                                          DBUS_XRANDR_INTERFACE,
                                          "screenAdded", this, SLOT(doAddedScreen(QString)));

    QDBusConnection::sessionBus().connect(QString(),
                                          QString(DBUS_XRANDR_PATH),
                                          DBUS_XRANDR_INTERFACE,
                                          "screenRemoved", this, SLOT(doRemovedScreen(QString)));

    QDBusConnection::sessionBus().connect(QString(),
                                          QString(DBUS_XRANDR_PATH),
                                          DBUS_XRANDR_INTERFACE,
                                          "screenStateChanged", this, SLOT(doScreenStateChanged(QString,int)));
}

GmWorkThread::~GmWorkThread()
{
    if (m_pSetTimer) {
        delete m_pSetTimer;
        m_pSetTimer = nullptr;
    }
}

void GmWorkThread::setAllOutputsBrightness(const double brightness)
{
    m_pGmHelper->setAllOutputsBrightness(brightness);
    USD_LOG(LOG_DEBUG,"update brightness...:%d",brightness);
}

void GmWorkThread::setBrightness(const QString outputName, const double brightness)
{
    m_pGmHelper->setBrightness(outputName, brightness);
}

double GmWorkThread::getBrightness()
{
    return 0;
}

void GmWorkThread::setTemperature(const int temperature)
{
    m_targetTemp = temperature;
    USD_LOG(LOG_DEBUG, "update setTemperature...:%d", m_targetTemp);
}

int GmWorkThread::getTemperature()
{
    return m_targetTemp;
}

void GmWorkThread::stopWork()
{
    isExit = true;
}

OutputGammaInfoList GmWorkThread::getAllOutputGammaInfo()
{
   return m_pGmHelper->getAllOutputsInfo();
}

void GmWorkThread::run()
{
    double frac;
    int errorTimes = 0;
    int brightSetCount = 0;
    int tempSetCount = 0;
    static int runTimes = 0;
    runTimes = 0;
    QList<OutputInfo>& outputList = m_pGmHelper->getOutputInfo();

    for (int var = 0; var < outputList.count(); var++) {
        if (outputList[var].rtBrightness == outputList[var].targetBrightness) {
            brightSetCount++;
        }
    }

    do {
        errorTimes = 0;
        tempSetCount = 0;
        if (isExit){
            break;
        }
        QTime timerStart = QTime::currentTime();
        frac = (runTimes * 50.f) / (1000.f*USD_NIGHT_LIGHT_SMOOTH_SMEAR);//调整值从大逐步变小以达到渐变效果。
        if (qAbs(m_rtTemperature - m_targetTemp) < 10.f) {
            m_rtTemperature = m_targetTemp;
        } else {
            m_rtTemperature = (m_targetTemp - m_lastTemperature) * frac + m_lastTemperature;
        }

        if (m_rtTemperature < 0) {
            USD_LOG(LOG_DEBUG,"had bug reset %d-->%d", m_rtTemperature, m_targetTemp);
            m_rtTemperature = m_targetTemp;
        }

        for (int var = 0; var < outputList.count(); var++) {
            if (qAbs(outputList[var].rtBrightness - outputList[var].targetBrightness) < 3 && outputList[var].rtBrightness != outputList[var].targetBrightness) {
                outputList[var].rtBrightness = outputList[var].targetBrightness;
                outputList[var].lastBrightness = outputList[var].rtBrightness;
                brightSetCount++;
            } else {
                outputList[var].rtBrightness = (outputList[var].targetBrightness - outputList[var].lastBrightness) * (frac*2) + outputList[var].lastBrightness;
                outputList[var].lastBrightness = outputList[var].rtBrightness;
                USD_LOG(LOG_DEBUG,"%s ---> %0.4f ---> %0.4f",outputList[var].name.toLatin1().data(), outputList[var].rtBrightness, outputList[var].targetBrightness);
            }

            if(outputList[var].rtTemp == m_targetTemp || outputList[var].connectState != RR_Connected) {
                tempSetCount++;
            }
        }

        do{
             msleep(10);
             errorTimes++;
        } while (!m_pGmHelper->setGammaWithTemp(m_rtTemperature) && errorTimes < 30);
        m_lastTemperature = m_rtTemperature;
        runTimes++;
        USD_LOG(LOG_DEBUG,"cost time:%d rtTemperature:%d:,runtime:%d,",timerStart.msecsTo(QTime::currentTime()), m_rtTemperature, runTimes);
        USD_LOG_SHOW_PARAM2(brightSetCount, outputList.count());

    } while (m_targetTemp != m_rtTemperature || brightSetCount != outputList.count() || tempSetCount != outputList.count());

}

void GmWorkThread::doAddedScreen(QString outputName)
{
    USD_LOG(LOG_DEBUG,"output:%s added", outputName.toLatin1().data());
}

void GmWorkThread::doRemovedScreen(QString outputName)
{
    USD_LOG(LOG_DEBUG,"output:%s removed", outputName.toLatin1().data());
}

/**
 * @brief GmWorkThread::doScreenStateChanged
 * @param outputName
 * @param state
 * 设备
 */
void GmWorkThread::doScreenStateChanged(QString outputName, int state)
{
    USD_LOG(LOG_DEBUG,"%s--->%d", outputName.toLatin1().data(), state);
    QList<OutputInfo>& outputList = m_pGmHelper->getOutputInfo();
    for (int var = 0; var < outputList.count(); var++) {
        if (!outputList[var].name.compare(outputName,Qt::CaseInsensitive)) {
            if (state) {
                outputList[var].targetTemp = m_targetTemp;
//                outputList[var].rtTemp = COLOR_TEMPERATURE_DEFAULT;
//                outputList[var].lastTemp = COLOR_TEMPERATURE_DEFAULT;
                this->run();
            } else {
                outputList[var].targetTemp = COLOR_TEMPERATURE_DEFAULT;
                outputList[var].rtTemp = COLOR_TEMPERATURE_DEFAULT;
                outputList[var].lastTemp = COLOR_TEMPERATURE_DEFAULT;
                this->run();
            }
        }
    }
    USD_LOG(LOG_DEBUG,"output:%s state:%d", outputName.toLatin1().data(), state);
}

