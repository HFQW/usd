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
#include "gamma-manager-wayland.h"
#include "gamma-color-info.h"
GammaManagerWayland *GammaManagerWayland::m_gammaWaylandManager = nullptr;

//struct ColorInfoWayland {
//    QString arg;
//    QDBusVariant out;
//};

//QDBusArgument &operator<<(QDBusArgument &argument, const ColorInfoWayland &mystruct)
//{
//    argument.beginStructure();
//    argument << mystruct.arg << mystruct.out;
//    argument.endStructure();
//    return argument;
//}

//const QDBusArgument &operator>>(const QDBusArgument &argument, ColorInfoWayland &mystruct)
//{
//    argument.beginStructure();
//    argument >> mystruct.arg >> mystruct.out;
//    argument.endStructure();
//    return argument;
//}

//Q_DECLARE_METATYPE(ColorInfoWayland)

/*
 * TODO:kwin使用的时天黑时间，而非日落时间。日落时间一般比天黑时间早半小时，需要提取kwin的算法
 *
*/
GammaManagerWayland::GammaManagerWayland()
{
    m_pColorSettings = new QGSettings(USD_COLOR_SCHEMA);
    m_pQtSettings    = new QGSettings(QT_THEME_SCHEMA);
    m_pGtkSettings   = new QGSettings(GTK_THEME_SCHEMA);
    m_pukuiGtkConfig = new UkuiGtkConfig(this);
    m_darkModeChangedBySelf = false;

    connect(m_pQtSettings, SIGNAL(changed(QString)), this, SLOT(doQtSettingsChanged(QString)), Qt::QueuedConnection);
    connect(m_pColorSettings, SIGNAL(changed(QString)), this, SLOT(doColorSettingsChanged(QString)), Qt::DirectConnection);
}

GammaManagerWayland::~GammaManagerWayland()
{
     USD_LOG(LOG_DEBUG,"wayland.....");
}

GammaManagerWayland *GammaManagerWayland::GammaManagerWaylandNew()
{
    if (m_gammaWaylandManager == nullptr) {
        m_gammaWaylandManager = new GammaManagerWayland();
    }
    return m_gammaWaylandManager;
}

bool GammaManagerWayland::Start()
{
    USD_LOG(LOG_DEBUG,"wayland start.....");
    syncColorSetToKwin();
    m_pukuiGtkConfig->connectGsettingSignal();
}

void GammaManagerWayland::Stop()
{
    USD_LOG(LOG_DEBUG,"wayland Stop.....");
    if (m_pColorSettings) {
        delete m_pColorSettings;
    }

    if (m_pQtSettings) {
        delete m_pQtSettings;
    }

    if (m_pGtkSettings) {
        delete m_pGtkSettings;
    }

    if (m_pukuiGtkConfig) {
        delete m_pukuiGtkConfig;
    }

}

void GammaManagerWayland::doQtSettingsChanged(QString setKey)
{
    if (setKey == QT_THEME_KEY) {
        bool isAllDay = m_pColorSettings->get(COLOR_KEY_ALLDAY).toBool();
        bool isEnable = m_pColorSettings->get(COLOR_KEY_ENABLED).toBool();
        USD_LOG(LOG_DEBUG,"get key:",setKey.toLatin1().data());
        if (m_pQtSettings->get(setKey).toString() != "ukui-dark") {
            if (m_pQtSettings->get(COLOR_KEY_DARK_MODE).toBool()) {
                m_darkModeChangedBySelf = true;
                m_pQtSettings->set(COLOR_KEY_STYLE_NAME_DM, m_pQtSettings->get(QT_THEME_KEY).toString());
                m_pQtSettings->set(COLOR_KEY_DARK_MODE,false);
                m_pQtSettings->apply();
            }
        }

        QString theme = m_pQtSettings->get(QT_THEME_KEY).toString();
        if (isAllDay && isEnable && theme == "ukui-dark") {
            m_darkModeChangedBySelf = true;
            m_pColorSettings->set(COLOR_KEY_DARK_MODE, true);
            m_pColorSettings->apply();
        }
    }
}

void GammaManagerWayland::doColorSettingsChanged(QString setKey)
{
    USD_LOG(LOG_DEBUG,"change key:%s",setKey.toLatin1().data());
    if (isDarkMode(setKey)) {
        USD_LOG(LOG_DEBUG,"is dark mode..");
        return;
    }

    syncColorSetToKwin();
}



void GammaManagerWayland::syncColorSetToKwin()
{
    QHash<QString, QVariant> nightConfig;
    QVector<ColorInfo> nightColor;
    QDBusInterface colorIft("org.ukui.KWin",
                            "/ColorCorrect",
                            "org.ukui.kwin.ColorCorrect",
                            QDBusConnection::sessionBus());

    QDBusMessage result = colorIft.call("nightColorInfo");

    const QDBusArgument &dbusArgs = result.arguments().at(0).value<QDBusArgument>().asVariant().value<QDBusArgument>();
    dbusArgs.beginArray();
    while (!dbusArgs.atEnd()) {
        ColorInfo color;
        dbusArgs >> color;
        nightColor.push_back(color);
    }
    dbusArgs.endArray();

    for (const ColorInfo it : nightColor) {
        nightConfig.insert(it.arg, it.out.variant());
    }
    qDebug()<<nightConfig;
    bool isColorEnabled = m_pColorSettings->get(COLOR_KEY_ENABLED).toBool();
    bool isAllDay = m_pColorSettings->get(COLOR_KEY_ALLDAY).toBool();
    bool isAutomatic = m_pColorSettings->get(COLOR_KEY_AUTOMATIC).toBool();

    if (isColorEnabled) {
        nightConfig[KWIN_COLOR_ACTIVE] = true;
    } else {
        nightConfig[KWIN_COLOR_ACTIVE] = false;
    }

    if (isAllDay) {
        nightConfig[KWIN_COLOR_MODE] = 3;
        USD_LOG_SHOW_PARAM1(nightConfig[KWIN_COLOR_MODE].toInt());
    } else if (isAutomatic) {
        QVariant qVar = m_pColorSettings->get(COLOR_KEY_LAST_COORDINATES);
        QVariantList qVarList =  qVar.value<QVariantList>();

        nightConfig[KWIN_COLOR_MODE] = 2;
        nightConfig[KWIN_LATITUDE] = qVarList[0].toDouble();
        nightConfig[KWIN_LONGITUDE] = qVarList[1].toDouble();
        USD_LOG_SHOW_PARAM1(nightConfig[KWIN_COLOR_MODE].toInt());
    } else {
//        if (inNightMode() == false) {//kwin 从夜间模式全天开启设置到开启自定义时间无法成功设置,必须中间过度一次
//            nightConfig[KWIN_COLOR_ACTIVE] = false;
//            colorIft.call("setNightColorConfig", nightConfig);
//            USD_LOG(LOG_DEBUG,"ready send to kwin!..");
//            USD_LOG(LOG_DEBUG, "active:%d,mode:%d,temp:%d long:%f lat:%f",nightConfig[KWIN_COLOR_ACTIVE].toBool(), nightConfig[KWIN_COLOR_MODE].toInt(),
//                    nightConfig[KWIN_NIGHT_TEMP].toInt(),nightConfig[KWIN_LONGITUDE].toDouble(),nightConfig[KWIN_LATITUDE].toDouble());

//        }
        nightConfig[KWIN_COLOR_ACTIVE] = isColorEnabled;
        nightConfig[KWIN_COLOR_MODE] = 1;//自定义时长
        int hour;
        int min;
        QString start,end;
        hour = m_pColorSettings->get(COLOR_KEY_FROM).toFloat();
        min = int((m_pColorSettings->get(COLOR_KEY_FROM).toFloat() * 60)) % 60;
        start = QString("%1:%2:00").arg(hour,2,10, QLatin1Char('0')).arg(min,2,10, QLatin1Char('0'));

        hour = m_pColorSettings->get(COLOR_KEY_TO).toFloat();
        min = int((m_pColorSettings->get(COLOR_KEY_TO).toFloat() * 60)) % 60;
        end = QString("%1:%2:00").arg(hour,2,10, QLatin1Char('0')).arg(min,2,10, QLatin1Char('0'));

        nightConfig[KWIN_COLOR_START] = start;
        nightConfig[KWIN_COLOR_END] = end;
        USD_LOG_SHOW_PARAM1(nightConfig[KWIN_COLOR_MODE].toInt());

        USD_LOG(LOG_DEBUG,"%s:%s",start.toLatin1().data(), end.toLatin1().data());
        qDebug()<<nightConfig;
    }

    nightConfig[KWIN_NIGHT_TEMP] = m_pColorSettings->get(COLOR_KEY_TEMPERATURE).toInt();
    colorIft.call("setNightColorConfig", nightConfig);
    USD_LOG(LOG_DEBUG,"ready send to kwin..");
    USD_LOG(LOG_DEBUG, "active:%d,mode:%d,temp:%d long:%f lat:%f",nightConfig[KWIN_COLOR_ACTIVE].toBool(), nightConfig[KWIN_COLOR_MODE].toInt(),
            nightConfig[KWIN_NIGHT_TEMP].toInt(),nightConfig[KWIN_LONGITUDE].toDouble(),nightConfig[KWIN_LATITUDE].toDouble());

}



bool GammaManagerWayland::isFracDayBetween(double value, double start, double end)
{
    if (end <= start) {
        end += 24;
    }

    if (value < start && value < end) {
        value += 24;
    }

   return value >= start && value < end;
}

bool GammaManagerWayland::inNightMode()
{
    QTime rtTime = QTime::currentTime();
    double fracDay = rtTime.hour() + (double) rtTime.minute() / 60 + (double) rtTime.second() / 3600;
    double scheduleFrom = m_pColorSettings->get(COLOR_KEY_FROM).toDouble();
    double scheduleTo = m_pColorSettings->get(COLOR_KEY_TO).toDouble();

    return isFracDayBetween(fracDay, scheduleFrom, scheduleTo);
}

bool GammaManagerWayland::isDarkMode(QString key)
{
    bool darkMode = m_pColorSettings->get(COLOR_KEY_DARK_MODE).toBool();
    bool ret;
    if (key.contains("-dm") || key == COLOR_KEY_REAL_TIME_TEMPERATURE) {
        return true;
    }

    //外部修改，则直接退出夜间模式。
    if (key == COLOR_KEY_ALLDAY || key == COLOR_KEY_ENABLED) {
        bool isAllDay = m_pColorSettings->get(COLOR_KEY_ALLDAY).toBool();
        bool isEnable = m_pColorSettings->get(COLOR_KEY_ENABLED).toBool();

        if (darkMode && false == (isAllDay & isEnable)) {
            m_darkModeChangedBySelf = true;
            m_pColorSettings->set(COLOR_KEY_DARK_MODE,false);
            m_pColorSettings->apply();
            return false;
        } else  if (isAllDay && isEnable && darkMode  == false){
            if (m_pQtSettings->get(QT_THEME_KEY).toString() == "ukui-dark") {
                m_darkModeChangedBySelf = true;
                m_pColorSettings->set(COLOR_KEY_DARK_MODE,true);
                m_pColorSettings->apply();
                return false;
            }
        }
    } else if (key == COLOR_KEY_AUTOMATIC) {
        ret = m_pColorSettings->get(key).toBool();
        if (darkMode && true == ret) {
            m_darkModeChangedBySelf = true;
            m_pColorSettings->set(COLOR_KEY_DARK_MODE,false);
            m_pColorSettings->apply();
            return false;
        }
    } else if (key == COLOR_KEY_AUTO_THEME) {
        ret = m_pColorSettings->get(key).toBool();
        if (darkMode && true == ret) {
            m_darkModeChangedBySelf = true;
            m_pColorSettings->set(COLOR_KEY_DARK_MODE,false);
            m_pColorSettings->apply();
            return false;
        }
    }

    if (key == COLOR_KEY_DARK_MODE) {
        if (m_darkModeChangedBySelf) {
            USD_LOG(LOG_DEBUG, "skip it....");
            m_darkModeChangedBySelf = false;
            return true;
        }
        if (m_pColorSettings->get(key).toBool()) {//进入夜间模式
            m_pColorSettings->delay();
            m_pColorSettings->set(COLOR_KEY_ALLDAY_DM, m_pColorSettings->get(COLOR_KEY_ALLDAY).toBool());
            m_pColorSettings->set(COLOR_KEY_ENABLED_DM, m_pColorSettings->get(COLOR_KEY_ENABLED).toBool());
            m_pColorSettings->set(COLOR_KEY_AUTOMATIC_DM, m_pColorSettings->get(COLOR_KEY_AUTOMATIC).toBool());
            m_pColorSettings->set(COLOR_KEY_STYLE_NAME_DM, m_pQtSettings->get(QT_THEME_KEY).toString());
            m_pColorSettings->set(COLOR_KEY_AUTO_THEME_DM, m_pColorSettings->get(COLOR_KEY_AUTO_THEME).toString());//四个任意一个改变则退出夜间模式。

            m_pColorSettings->set(COLOR_KEY_ALLDAY, true);
            m_pColorSettings->set(COLOR_KEY_ENABLED, true);
            m_pColorSettings->set(COLOR_KEY_AUTOMATIC, false);

            m_pColorSettings->set(COLOR_KEY_AUTO_THEME, false);
            m_pQtSettings->set(QT_THEME_KEY, "ukui-dark");
            m_pGtkSettings->set(GTK_THEME_KEY, "ukui-black");
            m_pColorSettings->apply();
            USD_LOG(LOG_DEBUG, "enter dark mode");

        } else {//退出夜间模式1
            m_pColorSettings->delay();
            m_pColorSettings->set(COLOR_KEY_ALLDAY, m_pColorSettings->get(COLOR_KEY_ALLDAY_DM).toBool());
            m_pColorSettings->set(COLOR_KEY_ENABLED, m_pColorSettings->get(COLOR_KEY_ENABLED_DM).toBool());
            m_pColorSettings->set(COLOR_KEY_AUTOMATIC, m_pColorSettings->get(COLOR_KEY_AUTOMATIC_DM).toBool());
            m_pColorSettings->set(COLOR_KEY_AUTO_THEME, m_pColorSettings->get(COLOR_KEY_AUTO_THEME_DM).toBool());

            if (false == m_pColorSettings->get(COLOR_KEY_AUTO_THEME).toBool()) {
                if (m_pColorSettings->get(COLOR_KEY_STYLE_NAME_DM).toString() == "ukui-default") {
                    m_pQtSettings->set(QT_THEME_KEY, "ukui-default");
                    m_pGtkSettings->set(GTK_THEME_KEY, "ukui-white");
                } else if(m_pColorSettings->get(COLOR_KEY_STYLE_NAME_DM).toString() == "ukui-light"){
                    m_pQtSettings->set(QT_THEME_KEY, "ukui-light");
                    m_pGtkSettings->set(GTK_THEME_KEY, "ukui-white");
                } else {
                    m_pQtSettings->set(QT_THEME_KEY, "ukui-dark");
                    m_pGtkSettings->set(GTK_THEME_KEY, "ukui-black");
                }
            }
            m_pColorSettings->apply();
            USD_LOG(LOG_DEBUG, "exit dark mode");
        }
        syncColorSetToKwin();
        return true;
    }

    return false;
}
