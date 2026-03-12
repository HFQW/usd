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

#ifndef GAMMAMANANGERTHREAD_H
#define GAMMAMANANGERTHREAD_H
#include <QThread>
#include <QMutex>
#include <QX11Info>
#include "gamma-manager-define.h"
#include "gamma-manager-adaptor.h"
#include "gamma-manager-helper.h"



class GmWorkThread: public QThread
{
    Q_OBJECT
public:
    GmWorkThread(QObject *parent = nullptr);
    ~GmWorkThread();
    /**
     * @brief setAllOutputsBrightness
     * @param brightness
     */
    void setAllOutputsBrightness(const double brightness);

    /**
     * @brief setBrightness
     * @param outputName
     * @param brightness
     */
    void setBrightness(const QString outputName, const double brightness);

    /**
     * @brief getBrightness
     * @return
     */
    double getBrightness();

    /**
     * @brief setTemperature
     * @param temperature
     */
    void setTemperature(const int temperature);

    /**
     * @brief getTemperature
     * @return
     */
    int getTemperature();

    /**
     * @brief stopWork
     */
    void stopWork();

    /**
     * @brief getAllOutputGammaInfo
     * @return
     */
    OutputGammaInfoList getAllOutputGammaInfo();
private Q_SLOTS:
    /**
     * @brief doAddedScreen
     * @param outputName
     */
    void doAddedScreen(QString outputName);

    /**
     * @brief doRemovedScreen
     * @param outputName
     */
    void doRemovedScreen(QString outputName);

    /**
     * @brief doScreenStateChanged
     * @param outputName
     * @param state
     */
    void doScreenStateChanged(QString outputName, int state);
protected:
    void run();

private:
    bool isExit = false;
    int m_targetTemp;
    int m_lastTemperature;
    int m_rtTemperature;

    QTimer *m_pSetTimer;
    GmHelper *m_pGmHelper;
};

#endif // GAMMAMANANGERTHREAD_H
