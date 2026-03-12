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

#include "gamma-manager-helper.h"

static int
find_last_non_clamped(unsigned short  array[], int size) {
    int i;
    for (i = size - 1; i > 0; i--) {
        if (array[i] < 0xffff)
        return i;
    }
    return 0;
}

GmHelper::GmHelper(QObject *parent)
{
    m_temperature = COLOR_TEMPERATURE_DEFAULT;
    initOutput();
}

GmHelper::~GmHelper()
{
    if (m_pScreenRes) {
        XRRFreeScreenResources(m_pScreenRes);
    }
}

bool GmHelper::getLonAndLatByJson(QString url, QByteArray bytes, QSizeF& psize)
{
    if(!url.compare(IP_API_ADDRESS,Qt::CaseInsensitive)) {
        return getLonAndLatMozilla(bytes, psize);
    } else if(!url.compare(IP_API_ADDRESS_BACKUP,Qt::CaseInsensitive)) {
        return getLonAndLatIPAPI(bytes, psize);
    }

    return 0;
}

bool GmHelper::getLonAndLatIPAPI(QByteArray jsonBytes, QSizeF& psize)
{
    QStringList keysList;
    QJsonParseError parseError;
    QJsonDocument rootDoc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        USD_LOG(LOG_ERR, "%s parse error",jsonBytes.data());
        return 0;
    }

    QJsonObject rootObj = rootDoc.object();
    keysList = rootObj.keys();
    if (!keysList.contains("lon") || !keysList.contains("lat")) {
        USD_LOG(LOG_ERR, "%s parse error can't find log or lat",jsonBytes.data());
        return 0;
    }

    QJsonValue lonValue = rootObj["lon"];
    QJsonValue latValue = rootObj["lat"];

    psize.setHeight(lonValue.toDouble());
    psize.setWidth(latValue.toDouble());
    return 1;
}

bool GmHelper::getLonAndLatMozilla(QByteArray jsonBytes, QSizeF &psize)
{
    QStringList keysList;
    QJsonParseError parseError;
    QJsonDocument rootDoc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        USD_LOG(LOG_ERR, "%s parse error",jsonBytes.data());
        return 0;
    }

    QJsonObject rootObj = rootDoc.object();
    keysList = rootObj.keys();
    if (!keysList.contains("location")) {
        USD_LOG(LOG_ERR, "%s parse error can't find log or lat",jsonBytes.data());
        return 0;
    }

    QJsonValue valueArray = rootObj["location"];
    QJsonValue latValue = valueArray["lat"];
    QJsonValue lonValue = valueArray["lng"];
    if (!lonValue.toDouble() || !latValue.toDouble()) {
         USD_LOG(LOG_ERR, "%s parse error can't find log or lng(%f,%f)",jsonBytes.data(), lonValue.toDouble(), latValue.toDouble());
        return 0;
    }
    psize.setHeight(lonValue.toDouble());
    psize.setWidth(latValue.toDouble());
    return 1;
}

bool GmHelper::getSunriseSunset(QDateTime &rtDate, QSizeF &location, QSizeF &SunriseSunset)
{
//    g_autoptr(GDateTime) dt_zero = g_date_time_new_utc (1900, 1, 1, 0, 0, 0);
//    GTimeSpan ts = g_date_time_difference (dt, dt_zero);
    QDateTime  dtZero = QDateTime::fromString("1900-01-01 00:00.000", Qt::ISODate);//1900, 1, 1, 0, 0, 0
    quint64 ts = dtZero.msecsTo(rtDate);

    USD_CHECK_RETURN(location.width() <=90.f && location.width() >= -90.f, false);
    USD_CHECK_RETURN(location.height() <=180.f && location.height() >= -180.f, false);
    double pos_lat = location.width();
    double pos_long = location.height();
    double tz_offset = rtDate.offsetFromUtc() / 60 / 60;
    double date_as_number = ts / 1000 / 24 / 60 / 60 + 2;  // B7
    double time_past_local_midnight = 0;  // E2, unused in this calculation
    double julian_day = date_as_number + 2415018.5 +
            time_past_local_midnight - tz_offset / 24;
    double julian_century = (julian_day - 2451545) / 36525;
    double geom_mean_long_sun =  fmod (280.46646 + julian_century *
                                      (36000.76983 + julian_century * 0.0003032), 360); // I2
    double geom_mean_anom_sun = 357.52911 + julian_century *
            (35999.05029 - 0.0001537 * julian_century);  // J2
    double eccent_earth_orbit = 0.016708634 - julian_century *
            (0.000042037 + 0.0000001267 * julian_century); // K2
    double sun_eq_of_ctr = sin (deg2rad (geom_mean_anom_sun)) *
            (1.914602 - julian_century * (0.004817 + 0.000014 * julian_century)) +
            sin (deg2rad (2 * geom_mean_anom_sun)) * (0.019993 - 0.000101 * julian_century) +
            sin (deg2rad (3 * geom_mean_anom_sun)) * 0.000289; // L2
    double sun_true_long = geom_mean_long_sun + sun_eq_of_ctr; // M2
    double sun_app_long = sun_true_long - 0.00569 - 0.00478 *
            sin (deg2rad (125.04 - 1934.136 * julian_century)); // P2
    double mean_obliq_ecliptic = 23 +  (26 +  ((21.448 - julian_century *
                                                (46.815 + julian_century * (0.00059 - julian_century * 0.001813)))) / 60) / 60; // Q2
    double obliq_corr = mean_obliq_ecliptic + 0.00256 *
            cos (deg2rad (125.04 - 1934.136 * julian_century)); // R2
    double sun_declin = rad2deg (asin (sin (deg2rad (obliq_corr)) *
                                       sin (deg2rad (sun_app_long)))); // T2
    double var_y = tan (deg2rad (obliq_corr/2)) * tan (deg2rad (obliq_corr / 2)); // U2
    double eq_of_time = 4 * rad2deg (var_y * sin (2 * deg2rad (geom_mean_long_sun)) -
                                     2 * eccent_earth_orbit * sin (deg2rad (geom_mean_anom_sun)) +
                                     4 * eccent_earth_orbit * var_y *
                                     sin (deg2rad (geom_mean_anom_sun)) *
                                     cos (2 * deg2rad (geom_mean_long_sun)) -
                                     0.5 * var_y * var_y * sin (4 * deg2rad (geom_mean_long_sun)) -
                                     1.25 * eccent_earth_orbit * eccent_earth_orbit *
                                     sin (2 * deg2rad (geom_mean_anom_sun))); // V2
    double ha_sunrise = rad2deg (acos (cos (deg2rad (90.833)) / (cos (deg2rad (pos_lat)) *
                                                                 cos (deg2rad (sun_declin))) - tan (deg2rad (pos_lat)) *
                                       tan (deg2rad (sun_declin)))); // W2
    double solar_noon =  (720 - 4 * pos_long - eq_of_time + tz_offset * 60) / 1440; // X2
    double sunrise_time = solar_noon - ha_sunrise * 4 / 1440; //  Y2
    double sunset_time = solar_noon + ha_sunrise * 4 / 1440; // Z2

    /* convert to hours */
    SunriseSunset.setHeight(sunrise_time * 24);
    SunriseSunset.setWidth(sunset_time * 24);
    return true;
}

bool GmHelper::getRtSunriseSunset(QSizeF &location, QSizeF &SunriseSunset)
{
    QDateTime rtDate = QDateTime::currentDateTime();
    //
    USD_LOG_SHOW_PARAM1(rtDate.offsetFromUtc());
    getSunriseSunset(rtDate, location, SunriseSunset);
    USD_LOG_SHOW_PARAM2F(SunriseSunset.width(), SunriseSunset.height());

    return true;
}

bool GmHelper::getRgbWithTemperature(double temp, ColorRGB &result)
{
    bool ret = true;
    const ColorRGB *pColorRgb = blackbodyDataD65plankian;
    uint tempQuot = 0;
    uint tempRem = 0;
    if (temp < 1000 || temp > 10000) {
        return false;
    }
    tempQuot = (uint) temp / 100;
    tempRem = (uint) temp % 100;
    tempQuot -= 10;

    if (!tempRem) {
        result = pColorRgb[tempQuot];
        return ret;
    }

    getRgbInterpolate(pColorRgb[tempQuot], pColorRgb[tempQuot + 1], tempRem / 100.0f, result);

}

void GmHelper::getRgbInterpolate(const ColorRGB &p1, const ColorRGB &p2, double index, ColorRGB &result)
{
    result.R =  (1.0 - index) * p1.R + index * p2.R;
    result.G =  (1.0 - index) * p1.G + index * p2.G;
    result.B =  (1.0 - index) * p1.B + index * p2.B;
}

uint GmHelper::getTempInterpolate(const double svalue, const double bvalue, double value)
{

    USD_CHECK_RETURN((svalue > 0 && svalue <=1.f), 0);
    USD_CHECK_RETURN((bvalue > 0 && bvalue <=1.f), 0);
    USD_CHECK_RETURN((value > 0 && value <=1.f), 0);

    return ((value - svalue) / (bvalue - svalue)) * 100;
}


uint GmHelper::getTemperatureWithRgb(const double red, const double green, const double blue)
{
    USD_CHECK_RETURN((red >= 0 && red <=1.f), 0);
    USD_CHECK_RETURN((green >= 0 && green <=1.f), 0);
    USD_CHECK_RETURN((blue >= 0 && blue <=1.f), 0);
    uint space = 0;
    uint temp;

    const ColorRGB *pColorRgb = blackbodyDataD65plankian;
    uint circleLength = sizeof(blackbodyDataD65plankian) / sizeof(ColorRGB);
    USD_LOG_SHOW_PARAM1(circleLength);
    for (int k = 0; k < circleLength; k++) {
        if (red <= pColorRgb[k].R && green <= pColorRgb[k].G && blue <= pColorRgb[k].B) {
            USD_LOG(LOG_DEBUG,"%.02f(%.02f),%.02f(%.02f),%.02f(%.02f)",red, pColorRgb[k].R, green, pColorRgb[k].G, blue, pColorRgb[k].B);
            space = k;
            break;
        }
    }

    if (space == 0) {
        return COLOR_MIN_TEMPERATURE;
    }

    temp = 1000 + (space - 1) * 100 + getTempInterpolate(pColorRgb[space-1].B, pColorRgb[space].B, blue);
    USD_LOG(LOG_DEBUG,"%.2f--%.2f--%.2f=====>%d",red, green, blue, temp);
    return temp;
}

bool GmHelper::setGammaWithTemp(const uint rtTemp)
{
    int size = 0;
    int ret = true;
    float gammaRed = 0.f;
    float gammaGreen = 0.f;
    float gammaBlue = 0.f;

    float brightness = 1.f;

    XRRCrtcGamma *pCrtcGamma;
    XRRScreenResources  *pScreenRes;
    m_temperature = rtTemp;

    if (rtTemp<COLOR_MIN_TEMPERATURE) {
        return true;
    }

    getRgbWithTemperature(rtTemp, m_colorRGB);

    if (m_pScreenRes==nullptr) {
         m_pScreenRes = XRRGetScreenResources(QX11Info::display(), QX11Info::appRootWindow());
    }
    pScreenRes = m_pScreenRes;
    for(int k = 0; k < pScreenRes->noutput; k++) {
        RROutput outputId = pScreenRes->outputs[k];
        XRROutputInfo	*outputInfo = XRRGetOutputInfo (QX11Info::display(), pScreenRes, outputId);
        QString outputname = QString::fromLatin1(outputInfo->name);
        if (outputInfo->connection != RR_Connected) {
            XRRFreeOutputInfo(outputInfo);
            for (int var = 0; var < m_outputList.count(); var++) {
                if (!m_outputList[var].name.compare(outputname,Qt::CaseInsensitive)) {
                    m_outputList[var].connectState = RR_Disconnected;
                }
            }
            continue;
        }

        for (int var = 0; var < m_outputList.count(); var++) {
            if (!m_outputList[var].name.compare(outputname,Qt::CaseInsensitive)) {
                m_outputList[var].rtTemp = rtTemp;
                m_outputList[var].lastTemp = rtTemp;
                m_outputList[var].connectState = RR_Connected;
                brightness = (m_outputList[var].rtBrightness / 100.0f) * MAX_GAMMA_BRIGHTNESS + MIN_GAMMA_BRIGHTNESS;
                USD_LOG(LOG_DEBUG,"find:%s set brigntness:%0.4f", m_outputList[var].name.toLatin1().data(), m_outputList[var].rtBrightness);
                break;
            } else {
                USD_LOG(LOG_DEBUG,"skip:%s:%s", outputInfo->name, m_outputList[var].name.toLatin1().data());
            }
        }
        if (!outputInfo->crtc) {
            ret = true;
            USD_LOG(LOG_ERR,"crtc size is 0.\n");
            goto FREEOUTPUT;
        }

        size = XRRGetCrtcGammaSize(QX11Info::display(), outputInfo->crtc);
        if (!size) {
            ret = false;
            USD_LOG(LOG_ERR,"Gamma size is 0.\n");
            goto FREEOUTPUT;
        }

        /*
         * The gamma-correction lookup table managed through XRR[GS]etCrtcGamma
         * is 2^n in size, where 'n' is the number of significant bits in
         * the X Color.  Because an X Color is 16 bits, size cannot be larger
         * than 2^16.
         */
        if (size > 65536) {
            ret = false;
            USD_LOG(LOG_ERR,"Gamma correction table is impossibly large.\n");
            goto FREEOUTPUT;
        }

        pCrtcGamma = XRRAllocGamma(size);
        if (!pCrtcGamma) {
             USD_LOG(LOG_ERR,"Gamma allocation failed.\n");
            continue;
        }
        m_colorRGB.R == m_colorRGB.R ? m_colorRGB.R : 1.0;
        m_colorRGB.G == m_colorRGB.G ? m_colorRGB.G : 1.0;
        m_colorRGB.B == m_colorRGB.B ? m_colorRGB.B : 1.0;

        gammaRed = 1 / m_colorRGB.R;
        gammaGreen = 1 / m_colorRGB.G;
        gammaBlue = 1 / m_colorRGB.B;

#if 1
        for (int i = 0; i < size; i++) {\
            uint value = (i * 0xffff) / (size - 1);
            pCrtcGamma->red[i] = value * m_colorRGB.R * brightness;
            pCrtcGamma->green[i]= value * m_colorRGB.G * brightness;
            pCrtcGamma->blue[i] = value * m_colorRGB.B * brightness;
        }
#else

        for (int i = 0; i < size; i++) {
            if (gammaRed == 1.0 && m_brightness == 1.0) {
                pCrtcGamma->red[i] = (double)i / (double)(size - 1) * 65535.0;
            }  else {
                pCrtcGamma->red[i] = qMin(qPow((double)i/(double)(size - 1),
                                              gammaRed) * m_brightness,
                                          1.0) * 65535.0;
            }
            
            if (gammaGreen == 1.0 && m_brightness == 1.0) {
                pCrtcGamma->green[i] = (double)i / (double)(size - 1) * 65535.0;
            } else {
                pCrtcGamma->green[i] = qMin(qPow((double)i/(double)(size - 1),
                                                gammaGreen) * m_brightness,
                                            1.0) * 65535.0;
            }
            
            if (gammaBlue == 1.0 && m_brightness == 1.0) {
                pCrtcGamma->blue[i] = (double)i / (double)(size - 1) * 65535.0;
            } else {
                pCrtcGamma->blue[i] = qMin(qPow((double)i/(double)(size - 1),
                                               gammaBlue) * m_brightness,
                                           1.0) * 65535.0;
            }
        }
#endif
        XRRSetCrtcGamma(QX11Info::display(), outputInfo->crtc, pCrtcGamma);
        XSync(QX11Info::display(), NULL);
        USD_LOG(LOG_DEBUG," %s color temp(%d) set ok r:%.4f,g:%.4f,b:%.4f,(bright):%0.4f", outputInfo->name, m_temperature,m_colorRGB.R,m_colorRGB.G,m_colorRGB.B,brightness);
        XRRFreeGamma(pCrtcGamma);
FREEOUTPUT:
         XRRFreeOutputInfo(outputInfo);
    }
    return ret;
}

void GmHelper::freeScreenResource()
{
    if (m_pScreenRes != nullptr) {
        XRRFreeScreenResources(m_pScreenRes);
        m_pScreenRes = nullptr;
    }
}

void GmHelper::setAllOutputsBrightness(const uint brightness)
{
    for (int var = 0; var < m_outputList.count(); ++var) {
        m_outputList[var].rtBrightness = brightness;
    }
    USD_LOG(LOG_DEBUG, "m_brightness:%d",m_brightness);
}

void GmHelper::setBrightness(const QString outputName, const double brightness)
{
    for (int var = 0; var < m_outputList.count(); ++var) {
        if (!m_outputList[var].name.compare(outputName,Qt::CaseInsensitive) || !outputName.compare("all",Qt::CaseInsensitive) ) {
            m_outputList[var].targetBrightness = brightness;
        }
    }
}

QList<OutputInfo> GmHelper::initOutput()
{
    if (m_pScreenRes==nullptr) {
         m_pScreenRes = XRRGetScreenResources(QX11Info::display(), QX11Info::appRootWindow());
    }

    for(int k = 0; k < m_pScreenRes->noutput; k++) {
        OutputInfo info;
        XRROutputInfo	*outputInfo = XRRGetOutputInfo (QX11Info::display(), m_pScreenRes, m_pScreenRes->outputs[k]);
        info.name = QString::fromLatin1(outputInfo->name);
        info.targetBrightness = 100;
        info.rtBrightness = 100;
        info.lastBrightness = 100;
        info.connectState = outputInfo->connection;
        info.lastTemp =  info.rtTemp =  info.targetTemp = COLOR_TEMPERATURE_DEFAULT;
        XRRFreeOutputInfo(outputInfo);
        m_outputList.append(info);
    }
    return m_outputList;
}

OutputGammaInfoList GmHelper::getAllOutputsInfo()
{
    OutputGammaInfoList outputGamaInfoList;
    for (int var = 0; var < m_outputList.count(); var++) {
        OutputGammaInfo outputGamaInfo;

    }

    for(int k = 0; k < m_pScreenRes->noutput; k++) {
        OutputGammaInfo outputGamaInfo;
        RROutput outputId = m_pScreenRes->outputs[k];
        XRROutputInfo	*outputInfo = XRRGetOutputInfo (QX11Info::display(), m_pScreenRes, outputId);
        if (outputInfo->connection != RR_Connected) {
            goto FREEOUPUT;
            continue;
        }
        for (int var = 0; var < m_outputList.count(); var++) {
            if (!m_outputList[var].name.compare(QString::fromLatin1(outputInfo->name),Qt::CaseInsensitive)) {
                outputGamaInfo.OutputName = m_outputList[var].name;
                outputGamaInfo.Gamma = getTemperatureWithRgb(m_colorRGB.R, m_colorRGB.G, m_colorRGB.B);
                outputGamaInfo.Temperature = m_temperature;
                outputGamaInfo.Brignthess = m_outputList[var].rtBrightness;
                outputGamaInfoList.append(std::move(outputGamaInfo));
            }
        }
FREEOUPUT:
        XRRFreeOutputInfo(outputInfo);
    }
    return outputGamaInfoList;
}

QList<OutputInfo>& GmHelper::getOutputInfo()
{
    return m_outputList;
}
//rgb 会修正亮度。所以，需要记录亮度值，然后根据亮度值回调出正常的色温再带入重新进行计算。
#if 0
OutputGammaInfoList GmHelper::getAllOutputGammaInfo()
{
    double i1, v1, i2, v2;
    int size, middle, lastBest, lastRed, lastGreen, lastBlue;
    float red = 0;
    float green = 0;
    float blue = 0;

    float brightness = 0;
    unsigned short  *bestArray;
    XRRCrtcGamma *pCrtcGamma;
    XRRScreenResources  *pScreenRes;
    OutputGammaInfoList outputGamaInfoList;
//    pScreenRes = XRRGetScreenResources(QX11Info::display(), QX11Info::appRootWindow());

    for(int k = 0; k < m_pScreenRes->noutput; k++) {
        ColorRGB colorRgb;
        OutputGammaInfo outputGamaInfo;
        RROutput outputId = m_pScreenRes->outputs[k];
        XRROutputInfo	*outputInfo = XRRGetOutputInfo (QX11Info::display(), m_pScreenRes, outputId);
        if (outputInfo->connection != RR_Connected) {
            XRRFreeOutputInfo(outputInfo);
            continue;
        }

        size = XRRGetCrtcGammaSize(QX11Info::display(), outputInfo->crtc);
        if (!size) {
            USD_LOG(LOG_ERR, "size get error");
            goto FREEOUTPUT;;
        }
        pCrtcGamma = XRRGetCrtcGamma(QX11Info::display(), outputInfo->crtc);
        if (!pCrtcGamma) {
            USD_LOG(LOG_ERR, "CrtcGamma get error");
            goto FREEOUTPUT;;
        }

        /*
         * The gamma-correction lookup table managed through XRR[GS]etCrtcGamma
         * is 2^n in size, where 'n' is the number of significant bits in
         * the X Color.  Because an X Color is 16 bits, size cannot be larger
         * than 2^16.
         */
        if (size > 65536) {
            USD_LOG(LOG_ERR, "size get error");
            goto FREEOUTPUT;;
        }
        lastRed = find_last_non_clamped(pCrtcGamma->red, size);
        lastBlue = find_last_non_clamped(pCrtcGamma->blue, size);
        lastGreen = find_last_non_clamped(pCrtcGamma->green, size);
        USD_LOG_SHOW_PARAM1(lastRed);
        USD_LOG_SHOW_PARAM1(lastBlue);
        USD_LOG_SHOW_PARAM1(lastGreen);
        bestArray = pCrtcGamma->red;
        lastBest = lastRed;

        if (lastGreen > lastBest) {
            lastBest = lastGreen;
            bestArray = pCrtcGamma->green;
        }

        if (lastBlue > lastBest) {
            lastBest = lastBlue;
            bestArray = pCrtcGamma->blue;
        }

        if (lastBest == 0) {
            lastBest == 1;
        }

        middle = lastBest / 2;
        i1 = (double)(middle + 1) / size;
        v1 = (double)(bestArray[middle]) / 65535;
        i2 = (double)(lastBest + 1) / size;
        v2 = (double)(bestArray[lastBest]) / 65535;
        USD_LOG_SHOW_PARAM1(lastBest);
        USD_LOG_SHOW_PARAM1(v2);
        if (v2 < 0.0001) {
            USD_LOG(LOG_ERR, "size get error");
            goto FREEOUTPUT;
        } else if ((lastBest + 1) == size) {
            brightness = v2;
        } else {
            brightness = exp((log(v2)*log(i1) - log(v1)*log(i2))/log(i1/i2));
        }

        brightness = 1;
        red = log((double)(pCrtcGamma->red[lastRed / 2]) / brightness
                / 65535) / log((double)((lastRed / 2) + 1) / size);
        green =  log((double)(pCrtcGamma->green[lastGreen / 2]) / brightness
                / 65535) / log((double)((lastGreen / 2) + 1) / size);
        blue = log((double)(pCrtcGamma->blue[lastBlue / 2]) / brightness
                / 65535) / log((double)((lastBlue / 2) + 1) / size);

        colorRgb.R = red == 0 ? 0 : 1/red;
        colorRgb.G = green == 0 ? 0 : 1/green;
        colorRgb.B = blue == 0 ? 0 : 1/blue;
        USD_LOG(LOG_DEBUG,"[%s] r:%.2f,g:%.2f,b:%.2f brightness:%.2f", outputInfo->name, colorRgb.R, colorRgb.G, colorRgb.B, brightness);
        outputGamaInfo.OutputName = QString::fromLatin1(outputInfo->name);
        outputGamaInfo.Gamma = getTemperatureWithRgb(colorRgb.R, colorRgb.G, colorRgb.B);
        outputGamaInfoList.append(std::move(outputGamaInfo));

FREEOUTPUT:
        XRRFreeGamma(pCrtcGamma);
        XRRFreeOutputInfo(outputInfo);
    }
    return outputGamaInfoList;
}
#endif

double GmHelper::deg2rad(double degrees)
{
     return (M_PI * degrees) / 180.f;
}

double GmHelper::rad2deg(double radians)
{
  return radians * (180.f / M_PI);
}



