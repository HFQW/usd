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

#include "gamma-manager.h"
#include "gamma-color-info.h"
//struct ColorInfo {
//    QString arg;
//    QDBusVariant out;
//};

//QDBusArgument &operator<<(QDBusArgument &argument, const ColorInfo &mystruct)
//{
//    argument.beginStructure();
//    argument << mystruct.arg << mystruct.out;
//    argument.endStructure();
//    return argument;
//}

//const QDBusArgument &operator>>(const QDBusArgument &argument, ColorInfo &mystruct)
//{
//    argument.beginStructure();
//    argument >> mystruct.arg >> mystruct.out;
//    argument.endStructure();
//    return argument;
//}

//Q_DECLARE_METATYPE(ColorInfo)
GammaManager *GammaManager::m_gammaManager = nullptr;
/*
 *
 * 目前拆分四个类：
 * 管理类：在合适的时机（开始运行、gsettings改变时）调用GmThread进行设置，GmLocation获取地理位置
 * 经纬度获取类：GmLocation，用于获取经纬度
 * 渐变类：GmApply，用户将计算后gamma值应用
 * 没有联网时，全天生效.
 * 缺省参数关闭。
 * 异常处理机制：
 * -地址链接失败，
 * --重复三次后检查网络状态，重复6次后更换地址
 * -json解析失败，
 * --重复10次后检查网络状态更换地址
*/
GammaManager::GammaManager():
    m_pColorSettings(nullptr),
    m_darkModeChangedBySelf(false),
    m_cachedTemperature(6500)
{
    m_pReckTimer     = new QTimer(this);

    m_pGmLocation    = new GmLocation(this);
    m_pGmThread      = new GmWorkThread(this);

    m_pukuiGtkConfig = new UkuiGtkConfig(this);
    m_pColorSettings = new QGSettings(USD_COLOR_SCHEMA);
    m_pQtSettings    = new QGSettings(QT_THEME_SCHEMA);
    m_pGtkSettings   = new QGSettings(GTK_THEME_SCHEMA);

    m_pGmDbus        = new GmDbus(this);
    m_pGmAdaptor     = new GmAdaptor(m_pGmDbus);

    m_pReckTimer->setTimerType(Qt::PreciseTimer);

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.registerService(DBUS_GM_NAME)) {
        sessionBus.registerObject(DBUS_GM_PATH,
                                  m_pGmDbus,
                                  QDBusConnection::ExportAllContents);
        USD_LOG(LOG_DEBUG, "register gamma manager dbus success");
    } else {
        USD_LOG(LOG_ERR, "register dbus error");
    }
}

GammaManager::~GammaManager()
{
    m_pReckTimer->stop();
    if (m_pColorSettings) {
        delete m_pColorSettings;
        m_pColorSettings = nullptr;
    }

    if (m_pGmLocation) {
        delete m_pGmLocation;
        m_pGmLocation = nullptr;
    }

    if (m_pQtSettings) {
        delete m_pQtSettings;
        m_pQtSettings = nullptr;
    }

    if (m_pGtkSettings) {
        delete m_pGtkSettings;
        m_pGtkSettings = nullptr;
    }

    if (m_pReckTimer) {
        delete m_pReckTimer;
        m_pReckTimer = nullptr;
    }

    if (m_pGmThread) {
        delete m_pGmThread;
        m_pGmThread = nullptr;
    }

    if (m_pukuiGtkConfig) {
        delete m_pukuiGtkConfig;
        m_pukuiGtkConfig = nullptr;
    }
}

GammaManager *GammaManager::GammaManagerNew()
{
    if (m_gammaManager == nullptr) {
        m_gammaManager = new GammaManager();
    }
    return m_gammaManager;
}

bool GammaManager::Start()
{
    //wayland转化到x时需要禁用kwin的色温管理

    if (UsdBaseClass::isEdu())
    {
        if (!m_pColorSettings->get(HAD_SET_EDU).toBool()) {
            m_pColorSettings->set(COLOR_KEY_TEMPERATURE, 5150);
            m_pColorSettings->set(COLOR_KEY_ALLDAY, true);
            m_pColorSettings->set(COLOR_KEY_AUTOMATIC, false);
            m_pColorSettings->set(COLOR_KEY_TEMPERATURE, 5150);
            m_pColorSettings->set(HAD_SET_EDU,true);
             USD_LOG(LOG_DEBUG,"--edu first  start--");
        }
        USD_LOG(LOG_DEBUG,"--Color check end--");
    }

    if (!m_pColorSettings->get(HAD_READ_KWIN).toBool()){
        if (false == ReadKwinColorTempConfig()) {
            USD_LOG(LOG_ERR,"--Kwin Color check over--");
        }
    }

    m_pGmLocation->setGsettings(m_pColorSettings);
    m_pGmLocation->start();

    connect(m_pQtSettings, SIGNAL(changed(QString)), this, SLOT(doQtSettingsChanged(QString)), Qt::DirectConnection);
    connect(m_pColorSettings, SIGNAL(changed(QString)), this, SLOT(doColorSettingsChanged(QString)), Qt::DirectConnection);
    connect(m_pReckTimer, SIGNAL(timeout()), this, SLOT(doReckTimeout()), Qt::DirectConnection);

    connect(m_pGmDbus, SIGNAL(screenBrightnessChanged(QString, int)), this, SLOT(doScreenBrightnessChanged(QString,int)), Qt::DirectConnection);
    connect(m_pGmDbus, SIGNAL(allScreenBrightnessChanged(QString, int)), this, SLOT(doScreenBrightnessChanged(QString,int)), Qt::DirectConnection);






    doColorSettingsChanged("");
    m_pReckTimer->setSingleShot(false);
    m_pReckTimer->start(60000);
    m_pukuiGtkConfig->connectGsettingSignal();
    USD_LOG(LOG_DEBUG,"start in x.....");
    return true;
}

void GammaManager::Stop()
{
    m_pGmThread->stopWork();
    m_pGmThread->exit();
    m_pGmThread->wait();
    USD_LOG(LOG_DEBUG,"stop.....");
}

void GammaManager::setLocationIp(QStringList addresses)
{
    m_pGmLocation->setIpAddresses(addresses);
}

/*Active:1,使能，0禁用。
 *0:全天，1跟随日出日落，2自定义
 *Mode:1 自定义
 *Mode:2--EveningBeginFixed（17:55:01）---跟随日出日落
 *Mode:3--全天
*/
bool GammaManager::ReadKwinColorTempConfig()
{
    QHash<QString, QVariant> nightConfig;
    QVector<ColorInfo> nightColor;
    if (m_pColorSettings->keys().contains(HAD_READ_KWIN)) {
        if (m_pColorSettings->get(HAD_READ_KWIN).toBool() == true) {
            USD_LOG(LOG_DEBUG,"Kwin had read over..");
            return false;
        }
    } else {
        USD_LOG(LOG_DEBUG,"can't find key:%s", HAD_READ_KWIN);
        return false;
    }

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

    for (ColorInfo it : nightColor) {
        nightConfig.insert(it.arg, it.out.variant());
    }

    m_pColorSettings->set(COLOR_KEY_TEMPERATURE, nightConfig[KWIN_NIGHT_TEMP].toInt());
    m_pColorSettings->set(COLOR_KEY_ENABLED,nightConfig[KWIN_COLOR_ACTIVE].toBool());

    if (3 == nightConfig[KWIN_COLOR_MODE].toInt()) {
        m_pColorSettings->set(COLOR_KEY_ALLDAY, true);
    } else if (2 == nightConfig[KWIN_COLOR_MODE].toInt() && nightConfig[KWIN_COLOR_START].toString() == "17:55:01"){
        m_pColorSettings->set(COLOR_KEY_AUTOMATIC, true);
    } else {
        QTime startTime = QTime::fromString(nightConfig[KWIN_COLOR_START].toString(),"hh:mm:ss");
        QTime endTime = QTime::fromString(nightConfig[KWIN_COLOR_END].toString(),"hh:mm:ss");

        m_pColorSettings->set(COLOR_KEY_FROM, hourMinuteToDouble(startTime.hour(), startTime.minute()));
        m_pColorSettings->set(COLOR_KEY_TO, hourMinuteToDouble(endTime.hour(), endTime.minute()));
    }

    USD_LOG_SHOW_PARAM1(nightConfig[KWIN_COLOR_ACTIVE].toBool());
    USD_LOG_SHOW_PARAM1(nightConfig[KWIN_COLOR_MODE].toInt());
    USD_LOG_SHOW_PARAMS(nightConfig[KWIN_COLOR_START].toString().toLatin1().data());
    USD_LOG_SHOW_PARAMS(nightConfig[KWIN_COLOR_END].toString().toLatin1().data());

    m_pColorSettings->set(HAD_READ_KWIN,true);
    nightConfig[KWIN_COLOR_ACTIVE] = false;
    colorIft.call("setNightColorConfig", nightConfig);

    nightConfig[KWIN_NIGHT_TEMP] = nightConfig[KWIN_CURRENT_TEMP];
    nightConfig[KWIN_COLOR_ACTIVE] = false;
    colorIft.call("setNightColorConfig", nightConfig);
    return true;
}
OutputGammaInfo GammaManager::getScreensInfo()
{
    OutputGammaInfo hdmi;
    OutputGammaInfo vga;
    hdmi.OutputName = "hdmi";
    hdmi.Gamma = 6500;
    hdmi.Brignthess = 100;

    vga.OutputName = "vga";
    vga.Gamma = 6000;
    vga.Brignthess = 80;
    OutputGammaInfoList varRet;

    return hdmi;
}

OutputGammaInfoList GammaManager::getScreensInfoList()
{
    OutputGammaInfoList infoList;
    infoList = m_pGmThread->getAllOutputGammaInfo();
    return infoList;
}


void GammaManager::doColorSettingsChanged(QString key)
{
    USD_LOG(LOG_DEBUG,"change key:%s",key.toLatin1().data());
    if (isDarkMode(key)) {
        USD_LOG(LOG_DEBUG,"return...");
        return;
    }

//    if (key == COLOR_KEY_LAST_COORDINATES) {
//        QVariant qVar;
//        QVariantList qVarList;

//        qVar = m_pColorSettings->get(key);
//        qVarList = qVar.value<QVariantList>();
//        USD_LOG(LOG_DEBUG,"key:%s(%0.4f,%0.4f)",key.toLatin1().data(), qVarList[0].toDouble(), qVarList[1].toDouble());
//    }

    gammaRecheck(key);
}

void GammaManager::doReckTimeout()
{
    gammaRecheck("");
}

void GammaManager::doScreenBrightnessChanged(QString outputName, int outputBrightness)
{
    m_pGmThread->setBrightness(outputName,outputBrightness);
    if (!m_pGmThread->isRunning()) {
        m_pGmThread->start();
    }
    USD_LOG(LOG_DEBUG,"set %s to %d", outputName.toLatin1().data(), outputBrightness);
}

bool GammaManager::isDarkMode(QString key)
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
        return true;
    }

    return false;
}

bool GammaManager::isFracDayBetween(double value, double start, double end)
{
    if (end <= start) {
        end += 24;
    }

    if (value < start && value < end) {
        value += 24;
    }

   return value >= start && value < end;
}

void GammaManager::gammaRecheck(QString key)
{
    double fracDay;
    double scheduleFrom = -1.f;
    double scheduleTo = -1.f;
    double themeFrom = -1.f;
    double themeTo = -1.f;
    double smear = USD_NIGHT_LIGHT_POLL_SMEAR; /* hours */
    int theme_now = -1;

    uint temperature;
    uint tempSmeared;
    QTime rtTime = QTime::currentTime();

    fracDay = getFracTimeFromDt(rtTime);
    scheduleFrom = m_pColorSettings->get(COLOR_KEY_AUTOMATIC_FROM).toDouble();
    scheduleTo = m_pColorSettings->get(COLOR_KEY_AUTOMATIC_TO).toDouble();
    if (scheduleFrom < 0.f || scheduleFrom >= 24.f) {
        USD_LOG(LOG_ERR, "scheduleForm are error value :%f", scheduleFrom);
        return;
    }

    if (scheduleTo < 0.f || scheduleTo >= 24.f) {
        USD_LOG(LOG_ERR, "scheduleForm are error value :%f", scheduleTo);
        return;
    }

    //合法性检测
    temperature = m_pColorSettings->get(COLOR_KEY_TEMPERATURE).toUInt();
    if(temperature < 1100 || temperature > COLOR_TEMPERATURE_DEFAULT) {
        USD_LOG(LOG_ERR, "temperature had error value:%d", temperature);
        return;
    }

    //自动主题
    if (m_pColorSettings->get(COLOR_KEY_AUTO_THEME).toBool()) {
        if (isFracDayBetween(fracDay, scheduleFrom, scheduleTo)) {
            m_pGtkSettings->set(GTK_THEME_KEY, "ukui-black");
            m_pQtSettings->set(QT_THEME_KEY, "ukui-dark");
        } else {
            m_pGtkSettings->set(GTK_THEME_KEY, "ukui-white");
            m_pQtSettings->set(QT_THEME_KEY, "ukui-light");
        }

        if(key == COLOR_KEY_AUTO_THEME) {
            return;
        }
    }

    //色温使能
    if (!m_pColorSettings->get(COLOR_KEY_ENABLED).toBool()) {
        setTemperature(COLOR_TEMPERATURE_DEFAULT);
        return;
    }

    //全天生效
    if (m_pColorSettings->get(COLOR_KEY_ALLDAY).toBool()) {
        setTemperature(temperature);
        return;
    }

    //跟随日出日落
    if (m_pColorSettings->get(COLOR_KEY_AUTOMATIC).toBool()) {
        scheduleFrom = m_pColorSettings->get(COLOR_KEY_AUTOMATIC_FROM).toDouble();
        scheduleTo = m_pColorSettings->get(COLOR_KEY_AUTOMATIC_TO).toDouble();
        if (scheduleFrom < 0.f || scheduleTo < 0.f) {
            scheduleFrom = m_pColorSettings->get(COLOR_KEY_FROM).toDouble();
            scheduleTo = m_pColorSettings->get(COLOR_KEY_TO).toDouble();
        }
    } else {
        scheduleFrom = m_pColorSettings->get(COLOR_KEY_FROM).toDouble();
        scheduleTo = m_pColorSettings->get(COLOR_KEY_TO).toDouble();
    }

    /* lower smearing period to be smaller than the time between start/stop */
    smear = qMin (smear,
                (qMin (qAbs ((scheduleTo - scheduleFrom)),
                     (24 - qAbs ((scheduleTo - scheduleFrom))))));

    USD_LOG(LOG_DEBUG,"fracDay:%.2f, %.2f %.2f", fracDay,scheduleFrom - smear, scheduleTo);
    if (!isFracDayBetween (fracDay,
                           scheduleFrom - smear,
                           scheduleTo)) {
        USD_LOG(LOG_DEBUG,".");
        setTemperature(COLOR_TEMPERATURE_DEFAULT);
        return;
    }

    /* smear the temperature for a short duration before the set limits
    *
    *   |----------------------| = from->to
    * |-|                        = smear down
    *                        |-| = smear up
    *
    * \                        /
    *  \                      /
    *   \--------------------/
    */

    if (smear < 0.01) {
        /* Don't try to smear for extremely short or zero periods */
        tempSmeared = temperature;
    } else if (isFracDayBetween(fracDay,
                                           scheduleFrom - smear,
                                           scheduleFrom)) {
        double factor = 1.f - ((fracDay - (scheduleFrom - smear)) / smear);
        tempSmeared = linearInterpolate (COLOR_TEMPERATURE_DEFAULT,
                                          temperature, factor);
        USD_LOG(LOG_DEBUG,"val1:%d val2:%d factor:%f,frac_day:%f,schedule_from:%f",COLOR_TEMPERATURE_DEFAULT, temperature, factor,
                fracDay,scheduleFrom);
    } else if (isFracDayBetween (fracDay,
                                           scheduleTo - smear,
                                           scheduleTo)) {
        double factor = (fracDay - (scheduleTo - smear)) / smear;
        tempSmeared = linearInterpolate (COLOR_TEMPERATURE_DEFAULT,
                                          temperature, factor);
        USD_LOG(LOG_DEBUG,"val1:%d val2:%d factor:%f,frac_day:%f,schedule_from:%f",COLOR_TEMPERATURE_DEFAULT, temperature, factor,
                fracDay,scheduleFrom);
    } else {
        tempSmeared = temperature;
    }
    USD_LOG(LOG_DEBUG,"temp_smeared:%d ...%d", tempSmeared, COLOR_TEMPERATURE_DEFAULT-tempSmeared);
    setTemperature(tempSmeared);
}

int GammaManager::setTemperature(const uint value)
{
    if (m_pGmThread->getTemperature() == value) {
        USD_LOG(LOG_DEBUG,"same value!!!");
        return 0;
    }

    m_pGmThread->setTemperature(value);
    if (!m_pGmThread->isRunning()) {
        m_pGmThread->start();
    }

    return 0;
}

void GammaManager::doQtSettingsChanged(QString key)
{
    USD_LOG(LOG_DEBUG,".%s",key.toLatin1().data());
    if (key == QT_THEME_KEY) {
        bool isAllDay = m_pColorSettings->get(COLOR_KEY_ALLDAY).toBool();
        bool isEnable = m_pColorSettings->get(COLOR_KEY_ENABLED).toBool();

        if (m_pQtSettings->get(key).toString().compare("ukui-dark",Qt::CaseInsensitive)) {
            if (m_pColorSettings->get(COLOR_KEY_DARK_MODE).toBool()) {
                m_darkModeChangedBySelf = true;
                m_pColorSettings->set(COLOR_KEY_STYLE_NAME_DM, m_pQtSettings->get(QT_THEME_KEY).toString());
                m_pColorSettings->set(COLOR_KEY_DARK_MODE,false);
                m_pColorSettings->apply();
                USD_LOG(LOG_DEBUG,"leave dark mode",key.toLatin1().data());
            }
        }

        QString theme = m_pQtSettings->get(QT_THEME_KEY).toString();
        if (isAllDay && isEnable && theme == "ukui-dark") {
            m_darkModeChangedBySelf = true;
            m_pColorSettings->set(COLOR_KEY_DARK_MODE, true);
            m_pColorSettings->apply();
            USD_LOG(LOG_DEBUG,"enter dark mode",key.toLatin1().data());
        }
    }
}

void GammaManager::setBrightness(const uint value)
{
    if(m_pGmThread->getBrightness() == value) {
        return;
    }
    USD_LOG(LOG_DEBUG, "set brightness:%d", value);
    m_pGmThread->setAllOutputsBrightness(value);
    if (!m_pGmThread->isRunning()) {
        m_pGmThread->start();
    }
}

double GammaManager::getFracTimeFromDt(QTime dt)
{
    return dt.hour() + (double) dt.minute() / 60 + (double) dt.second() / 3600;
}

double GammaManager::linearInterpolate(double val1, double val2, double factor)
{
    USD_CHECK_RETURN (factor >= 0.f, -1.f);
    USD_CHECK_RETURN (factor <= 1.f, -1.f);
    return ((val1 - val2) * factor) + val2;
}

double GammaManager::hourMinuteToDouble(int hour, int minute)
{
    double value = (double)minute/60;
    return (double) hour + value;
}


