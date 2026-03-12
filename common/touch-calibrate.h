/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2023 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef TOUCHCALIBRATE_H
#define TOUCHCALIBRATE_H
#include <QObject>
#include <QSharedPointer>
#include <QVariant>
#include <QX11Info>
#include <QSettings>
#include <QFileInfo>
#include <QProcess>
#include <QDir>

extern "C"{
#include <X11/Xlib.h>
}

struct TouchDevice
{
    QString name;
    QString node;
    int id;
    int width = 0;
    int height = 0;
    bool isMapped = false; //已映射
};

struct ScreenInfo
{
    QString name;
    int width = 0;
    int height = 0;
    bool isMapped = false;
};

struct TouchConfig
{
    QString sTouchName;     //触摸屏的名称
    QString sTouchSerial;   //触摸屏的序列号
    QString sMonitorName;   //显示器的名称
};//配置文件中记录的映射关系信息

typedef QSharedPointer<TouchDevice> TouchDevicePtr;
typedef QMap<QString, TouchDevicePtr> TouchDeviceMap;

typedef QSharedPointer<ScreenInfo> ScreenInfoPtr;
typedef QMap<QString, ScreenInfoPtr> ScreenInfoMap;

typedef QSharedPointer<TouchConfig> TouchConfigPtr;
typedef QList<TouchConfigPtr> TouchConfigList;

class TouchCalibrate : public QObject
{
    Q_OBJECT
public:
    explicit TouchCalibrate(const QString& path, QObject *parent = nullptr);
    ~TouchCalibrate();

private:
    bool initDisplay();
    /**
     * @brief 获取已连接屏幕信息
     */
    void getScreenList();
    /**
     * @brief 获取触摸设备信息
     */
    void getTouchDeviceList();
    /**
     * @brief 获取屏幕映射信息
     */
    void getTouchConfigure();

    /**
     * @brief touchscreen 映射
     */
    void calibrateTouchScreen();
    /**
     * @brief tablet 映射
     */
    void calibrateTablet();
    /**
     * @brief calibrateDevice
     * @param id 触摸设备ID
     * @param output 屏幕名称
     */
    void calibrateDevice(int id, const QString &output);
    /**
     * @brief checkMatch
     * @param output_width
     * @param output_height
     * @param input_width
     * @param input_height
     * @return 尺寸比较
     */
    bool checkMatch(double output_width, double output_height, double input_width, double input_height);
    /**
     * @brief 获取设备节点
     * @param id
     * @return
     */
    QString getDeviceNode(int id);
    /**
     * @brief 获取触摸设备尺寸
     * @param node
     * @param width
     * @param height
     */
    void getTouchSize(const char *node, int &width, int &height);

public:
    void calibrate();

    bool hasTouchDevice();

Q_SIGNALS:
private:
    Display *m_display = nullptr;
    QString m_touchConfigPath;
    ScreenInfoMap m_screenInfoMap;
    TouchDeviceMap m_touchScreenMap;
    TouchDeviceMap m_tabletMap;
    TouchConfigList m_touchConfigList;
};

#endif // TOUCHCALIBRATE_H
