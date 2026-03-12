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

#ifndef DEMOMANAGER_H
#define DEMOMANAGER_H


#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDBusInterface>
#include <QObject>
#include <QTimer>

#include "clib-syslog.h"
#include "usd_base_class.h"
#include "QGSettings/qgsettings.h"

#include "gamma-manager-gtkconfig.h"
#include "gamma-manager-define.h"
#include "gamma-manager-dbus.h"
#include "gamma-manager-adaptor.h"
#include "gamma-manager-thread.h"
#include "gamma-manager-helper.h"
#include "gamma-manager-location.h"
#include "plugin-manager-interface.h"

class GammaManager : public ManagerInterface
{
    Q_OBJECT
public:
    GammaManager();
    GammaManager(GammaManager&)=delete;
    GammaManager&operator=(const GammaManager&)=delete;
public:
    ~GammaManager();
    static GammaManager *GammaManagerNew();

    virtual bool Start();
    virtual void Stop();
    /**
     * @brief setLocationIp  pingpang测试接口
     * @param addresses
     */
    void setLocationIp(QStringList addresses);//for test

    /**
     * @brief getScreensInfo 获取屏幕信息
     * @return
     */
    OutputGammaInfo getScreensInfo();

    /**
     * @brief getScreensInfoList
     * @return
     */
    OutputGammaInfoList getScreensInfoList();

    /**
     * @brief setTemperature 设置色温目标亮度值，渐变有thread实现
     * @param value
     * @return
     */
    int setTemperature(const uint value);//设置色温、亮度目标值，

public Q_SLOTS:

    /**
     * @brief doQtSettingsChanged 辅助做夜间模式
     * @param key
     */
    void doQtSettingsChanged(QString key);

    /**
     * @brief doColorSettingsChanged
     * @param key
     */
    void doColorSettingsChanged(QString key);

    /**
     * @brief doReckTimeout 每分钟执行的定时器
     */
    void doReckTimeout();

    /**
     * @brief doSetScreenBrightness
     * @param outputName
     * @param outputBrightness
     */
    void doScreenBrightnessChanged(QString outputName, int outputBrightness);

private:
    /**
     * @brief isDarkMode 此键值的改动是否导致进入夜间模式
     * @param key
     * @return
     */
    bool isDarkMode(QString key);

    /**
     * @brief isFracDayBetween
     * @param value
     * @param start
     * @param end
     * @return
     */
    bool isFracDayBetween (double  value, double start, double end);

    /**
     * @brief gammaRecheck 每分钟，或者收到color中key的changed信号重新设置色温
     * @param key
     */
    void gammaRecheck(QString key);

    /**
     * @brief ReadKwinColorTempConfig 2107早期版本升级参数兼容
     * @return
     */
    bool ReadKwinColorTempConfig();

    /**
     * @brief setBrightness 设置所有亮度
     * @param value
     */
    void setBrightness(const uint value);

    /**
     * @brief getFracTimeFromDt
     * @param dt
     * @return
     */
    double getFracTimeFromDt(QTime dt);

    /**
     * @brief linearInterpolate 差值计算va11，val2在factor时的取值，渐变色温
     * @param val1
     * @param val2
     * @param factor
     * @return
     */
    double linearInterpolate(double val1, double val2, double factor);

    /**
     * @brief hourMinuteToDouble
     * @param hour
     * @param minute
     * @return
     */
    double hourMinuteToDouble(int hour, int minute);

private:
    QGSettings *m_pColorSettings;
    QGSettings *m_pQtSettings;
    QGSettings *m_pGtkSettings;

    QTimer      *m_pReckTimer;
    GmLocation  *m_pGmLocation;
    GmWorkThread    *m_pGmThread;
    GmAdaptor   *m_pGmAdaptor;
    GmDbus      *m_pGmDbus;
    UkuiGtkConfig *m_pukuiGtkConfig;
    int  m_cachedTemperature;

    bool m_darkModeChangedBySelf;
    static GammaManager *m_gammaManager;
};


#endif // DEMOMANAGER_H
