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

#ifndef GAMMAMANAGERHELPER_H
#define GAMMAMANAGERHELPER_H

#include <QTime>

#include <QtMath>
#include <QDebug>
#include <QSizeF>
#include <QString>
#include <QX11Info>
#include <QObject>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include "clib-syslog.h"
#include "gamma-manager-define.h"
#include "gamma-manager-adaptor.h"
#include "rgb-gamma-table.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <math.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#ifdef __cplusplus
}
#endif

typedef struct _OutputInfo{
    QString name;
    bool connectState;
    uint targetTemp;
    uint lastTemp;
    uint rtTemp;
    double targetBrightness;
    double lastBrightness;
    double rtBrightness;
}OutputInfo;

class GmHelper: public QObject
{
    Q_OBJECT
public:
    GmHelper(QObject *parent = nullptr);
    ~GmHelper();

    /**
     * @brief getLonAndLatByJson 根据服务器地址，提取经纬度信息
     * @param url
     * @param bytes
     * @param psize
     * @return
     */
    bool getLonAndLatByJson(QString url, QByteArray bytes, QSizeF& psize);

    /**
     * @brief getSunriseSunset 计算指定日期日出日落
     * @param rtDate
     * @param location
     * @param SunriseSunset
     * @return
     */
    bool getSunriseSunset(QDateTime &rtDate,QSizeF &location, QSizeF &SunriseSunset);

    /**
     * @brief getRtSunriseSunset 计算当前日期的日出日落
     * @param location
     * @param SunriseSunset
     * @return
     */
    bool getRtSunriseSunset(QSizeF &location, QSizeF &SunriseSunset);

    /**
     * @brief getRgbWithTemperature 色温值转化gamma所需的RGB
     * @param temp
     * @param result
     * @return
     */
    bool getRgbWithTemperature(double temp, ColorRGB &result);


    /**
     * @brief getTempInterpolate
     * @param svalue
     * @param bvalue
     * @param value
     * @return
     */
    uint getTempInterpolate(const double svalue, const double bvalue, double value);

    /**
     * @brief getTemperatureWithRgb
     * @param red
     * @param green
     * @param blue
     * @return
     */
    uint getTemperatureWithRgb(const double red, const double green, const double blue);

    /**
     * @brief setAllOutputsBrightness
     * @param brightness
     */
    void setAllOutputsBrightness(const uint brightness);

    /**
     * @brief setBrightness
     * @param outputName
     * @param brightness
     */
    void setBrightness(const QString outputName, const double brightness);

    /**
     * @brief getAllOutputsInfo
     * @return
     */
    OutputGammaInfoList getAllOutputsInfo();

    /**
     * @brief getOutputInfo 为设置单个屏幕的亮度，外部方法先获取outputinfo，然后逐个显示设置targetVaue，并根据target与运行次数设置rtvalue
     * @return
     */
    QList<OutputInfo>& getOutputInfo();

    /**
     * @brief setTemperature 设置gamma，便利outputinfo的rtvalue与
     * @param temp
     */
    bool setGammaWithTemp(const uint rtTemp);

    /**
     * @brief freeScreenResource
     */
    void freeScreenResource();
private:

    /**
     * @brief initOutput
     * @return
     */
    QList<OutputInfo> initOutput();

    /**
     * @brief getRgbInterpolate
     * @param p1
     * @param p2
     * @param index
     * @param result
     */
    void getRgbInterpolate(const ColorRGB &p1, const ColorRGB &p2, double index, ColorRGB &result);

    /**
     * @brief getLonAndLatMozilla
     * @param jsonBytes
     * @param psize
     * @return
     * //{"location": {"lat": 39.9075, "lng": 116.3972}, "accuracy": 49000.0}
     */
    bool getLonAndLatMozilla(QByteArray jsonBytes, QSizeF& psize);

     /**
     * @brief getLonAndLatIPAPI
     * @param jsonBytes
     * @param psize
     * @return
     *  //{"status":"success","country":"China","countryCode":"CN","region":"TJ","regionName":"Tianjin","city":"Tianjin","zip":"","lat":39.3434,"lon":117.362,"timezone":"Asia/Shanghai","isp":"ASN for TIANJIN Provincial Net of CT","org":"Chinanet TJ","as":"AS17638 ASN for TIANJIN Provincial Net of CT","query":"123.150.8.42"}
     */
    bool getLonAndLatIPAPI(QByteArray jsonBytes, QSizeF& psize);
private:
    double deg2rad (double degrees);
    double  rad2deg (double radians);

    XRRScreenResources  *m_pScreenRes = nullptr;
    QList<OutputInfo> m_outputList;
    uint m_brightness;
    uint m_temperature;
    ColorRGB m_colorRGB;
};

#endif // GAMMAMANAGERHELPER_H
