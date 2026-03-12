/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2020 KylinSoft Co., Ltd.
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

#ifndef USDBASECLASS_H
#define USDBASECLASS_H
#include <QObject>
#include <QVariant>
#include <QMetaEnum>
#include <libkysysinfo.h>
#include <libkycpu.h>

#include "rfkillswitch.h"
extern QString g_motify_poweroff;

//lenovo N70Z
#define EC_TOUCHPADSTATE "/sys/devices/platform/lenovo_ec/touchpad"  //触摸板状态节点
#define PERFORMANCE_MODE "/sys/devices/platform/lenovo_ec/mode"  //性能模式节点


class UsdBaseClass: public QObject
{
    Q_OBJECT

public:
    UsdBaseClass();
    ~UsdBaseClass();
    enum eScreenMode {
        firstScreenMode = 0,
        cloneScreenMode,
        extendScreenMode,
        secondScreenMode};

    Q_ENUM(eScreenMode)

    enum BatteryState {
        Connected = 1,
        Disconnect};

    Q_ENUM(BatteryState)


    enum PowerMode {
        PerformanceMode = 0,
        AutoMode,
        EcoMode};

    Q_ENUM(PowerMode)

    static bool isTablet();

    static bool isMasterSP1();

    static bool is9X0();

    static bool isWayland();

    static bool isXcb();

    static bool isNotebook();

    static bool isLoongarch();

    static bool isUseXEventAsShutKey();

    static bool readPowerOffConfig();

    static bool isPowerOff();

    static bool isJJW7200();

    static int getDPI();

    static double getScaleWithSize(int heightmm, int widthmm, int height, int width);

    static double getScale(double scaling);

    static double getScoreScale(double scaling);

    static bool isEdu();

    static void writeUserConfigToLightDM(QString group, QString key, QVariant value, QString userName = "");

    static QVariant readUserConfigToLightDM(QString group, QString key, QString userName = "");

    static bool getPowerModeList(QList<int> &list);

/*
 * 针对EC自己控制的硬件设备，usd先获取控制权是否在EC，然后读取状态进行状态显示。
*/
    static bool brightnessControlByHardware(int &step);

    static bool flightModeControlByHardware(int &mode);

    static bool touchpadControlByHardware(int &state);

    static bool powerModeControlByHardware(int &mode);

    static QVariant readInfoFromFile(QString filePath);
private:

};

#endif // USDBASECLASS_H
