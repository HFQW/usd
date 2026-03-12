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

#include <QX11Info>
#include <QGuiApplication>
#include "clib-syslog.h"
#include "usd_base_class.h"

#include <QDBusMessage>
#include <QDBusInterface>
#include <QSettings>
#include <QDBusReply>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QtMath>
#define STR_EQUAL 0

#define DBUS_SERVICE "org.freedesktop.UPower"
#define DBUS_OBJECT "/org/freedesktop/UPower"
#define DBUS_INTERFACE "org.freedesktop.DBus.Properties"
#define POWER_OFF_CONFIG_FILE "/sys/class/dmi/id/modalias"

QString g_motify_poweroff = "";
#define UBC_UNSET 999
extern "C"{
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
}


UsdBaseClass::UsdBaseClass()
{
}

UsdBaseClass::~UsdBaseClass()
{

}

bool UsdBaseClass::isMasterSP1()
{
    return false;
}

bool UsdBaseClass::isUseXEventAsShutKey()
{
    return true;
}

bool UsdBaseClass::isTablet()
{
    static int ret = UBC_UNSET;

    if (ret!=UBC_UNSET) {
        return ret;
    }

    ret = false;
    if ((kdk_system_get_productFeatures()&2) == 2) {
        ret = true;
    }

    return ret;
}

bool UsdBaseClass::isLoongarch()
{
    QString cpuMode = kdk_cpu_get_model();
    USD_LOG(LOG_DEBUG,"GetCpuModelName : %s",cpuMode.toStdString().c_str());
    if(cpuMode.toLower().contains("loongson-3a4000")){
        return true;
    }
    return false;
}

bool UsdBaseClass::is9X0()
{
#ifdef USD_9X0
     return true;
#endif
    return false;
}

bool UsdBaseClass::isWayland()
{
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        USD_LOG(LOG_DEBUG,"is wayland app");
        return true;
    }

    USD_LOG(LOG_DEBUG,"is xcb app");
    return false;
}

bool UsdBaseClass::isXcb()
{
    if (QGuiApplication::platformName().startsWith(QLatin1String("xcb"))) {
        USD_LOG(LOG_DEBUG,"is xcb app");
        return true;
    }
    return false;
}

bool UsdBaseClass::isNotebook()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(DBUS_SERVICE,DBUS_OBJECT,DBUS_INTERFACE,"Get");
    msg<<DBUS_SERVICE<<"LidIsPresent";
    QDBusMessage res = QDBusConnection::systemBus().call(msg);
    if(res.type()==QDBusMessage::ReplyMessage)
    {
        QVariant v = res.arguments().at(0);
        QDBusVariant dv = qvariant_cast<QDBusVariant>(v);
        QVariant result = dv.variant();
        return result.toBool();
    }

    return false;
}

bool UsdBaseClass::readPowerOffConfig()
{
    QDir dir;
    QFile file;
    QString filePath = POWER_OFF_CONFIG_FILE;

    file.setFileName(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
       return false;
    }

    QTextStream pstream(&file);
    g_motify_poweroff = pstream.readAll();
    file.close();
    return true;
}

bool UsdBaseClass::isPowerOff()
{
    const QStringList devName ={"pnPF215T"};

    if(g_motify_poweroff.isEmpty())
        readPowerOffConfig();

    for(auto devNameTmp : devName)
    {
        if (g_motify_poweroff.contains(devNameTmp, Qt::CaseSensitive))
        {
            return true;
        }
    }
    return false;
}

bool UsdBaseClass::isJJW7200()
{
    static int ret = UBC_UNSET;
    char *pAck = NULL;
    char CmdAck[256] = "";
    FILE * pPipe;

    if (ret != UBC_UNSET) {
        return ret;
    }

    pPipe = popen("lspci | grep -i VGA |grep 7200","r");

    if (pPipe) {
        pAck = fgets(CmdAck, sizeof(CmdAck)-1, pPipe);
        if (strlen(CmdAck) > 3) {
            ret = true;
        } else{
            ret = false;
        }

        pclose(pPipe);
    } else {
        ret = false;
    }

    return ret;
}

int UsdBaseClass::getDPI()
{
    static int ret = 0;
    char *dpi = NULL;

    if (ret) {
        return ret;
    }

    dpi = XGetDefault(QX11Info::display(), "Xft", "dpi");
    if (dpi) {
        QString qDpi = QString::fromLatin1(dpi);

        if (qDpi == "192") {
            ret = 192;
        } else {
            ret = 96;
        }
    } else {
        ret = 96;
    }
    return ret;
}


double UsdBaseClass::getScale(double scaling)
{
    double scale = 0.0;
    if (scaling <= 2.15)
        scale = getScoreScale(scaling);
    else if (scaling <= 3.15)
        scale = getScoreScale(scaling - 1) + 1;
    else if (scaling <= 4.15)
        scale = getScoreScale(scaling - 2) + 2;
    else if (scaling <= 5.15)
        scale = getScoreScale(scaling - 3) + 3;
    else if (scaling <= 6.15)
        scale = getScoreScale(scaling -4 ) + 4;
    else
        scale = 6;// 根据目前大屏及8K屏幕，最高考虑6倍缩放

    return scale/2;
}

double UsdBaseClass::getScaleWithSize(int heightmm, int widthmm, int height, int width)
{
    double inch = 0.0;
    double scale = 0.0;
    double screenArea  = height * width;
    inch = sqrt(heightmm * heightmm + widthmm * widthmm) / 25.4;

    if (inch <= 10.00) {
        scale = qSqrt(screenArea) / qSqrt(1024 * 576);
    }
    else if (10.00 < inch && inch <= 15.00) { // 10 < inch <= 15 : 1366x768
        scale = qSqrt(screenArea) / qSqrt(1366 * 768);
    }
    else if (15.00 < inch && inch <= 20.00) { // 15 < inch <= 20 : 1600x900
        scale = qSqrt(screenArea) / qSqrt(1600 * 900);
    }
    else if (20.00 < inch && inch <= 30.00) { // 20 < inch <= 30 : 1920x1080
        scale = qSqrt(screenArea) / qSqrt(1920 * 1080);
    }
    else if (30 < inch && inch<= 60) { // 30 < inch <= 60 :
        scale = qSqrt(screenArea) / qSqrt(1600 * 900);
    }
    else { // inch > 60
        scale = qSqrt(screenArea) / qSqrt(1280 * 720);
    }
    return getScale(scale);
}

double UsdBaseClass::getScoreScale(double scaling)
{
    double scale = 0.0;
    if (scaling <= 1.15)
        scale = 1;
    else if (scaling <= 1.4)
        scale = 1.25;
    else if (scaling <= 1.65)
        scale = 1.5;
    else if (scaling <= 1.9)
        scale = 1.75;
    else
        scale = 2;

    return scale;
}

bool UsdBaseClass::isEdu()
{
    static int ret = UBC_UNSET;
    static QString projectCode = nullptr;
    QString deviceName = "-edu";
    if (ret != UBC_UNSET) {
        return ret;
    }
    if (nullptr == projectCode) {
        char *kdkProjectName= kdk_system_get_projectName();
        if (kdkProjectName == NULL) {
            ret = 0;
            return ret;
        }
        projectCode = QString::fromLatin1(kdkProjectName);
        projectCode = projectCode.toLower();
        USD_LOG(LOG_DEBUG,"projectCode:%s",projectCode.toLatin1().data());
    }
    if (projectCode.contains(deviceName)) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

void UsdBaseClass::writeUserConfigToLightDM(QString group, QString key, QVariant value, QString userName)
{
    QDir dirCheck;
    QString user = QDir::home().dirName();
    if (!userName.isEmpty()) {
        user = userName;
    }
    qDebug()<<key<<":"<<value;
    QString usdDir = QString("/var/lib/lightdm-data/%1/usd").arg(user);
    QString configDir = QString("/var/lib/lightdm-data/%1/usd/config").arg(user);
    QString configFile = QString("/var/lib/lightdm-data/%1/usd/config/ukui-settings-daemon.settings").arg(user);

    if (!dirCheck.exists(usdDir)) {
        dirCheck.mkdir(usdDir);

        QFile file(usdDir);
        file.setPermissions(QFileDevice::Permission(0x7777));
        file.close();
    }

    if (!dirCheck.exists(configDir)) {
        dirCheck.mkdir(configDir);
    }
    QFile file(configDir);
    file.setPermissions(QFileDevice::Permission(0x7777));
    file.close();

    QSettings *usdSettings = new QSettings(configFile, QSettings::IniFormat);
    USD_LOG(LOG_DEBUG,"ready save %s writable:%d!",configFile.toLatin1().data(), usdSettings->isWritable());
    usdSettings->beginGroup(group);
    usdSettings->setValue(key, value);
    usdSettings->endGroup();
    usdSettings->sync();
    usdSettings->deleteLater();
    QFile::setPermissions(configFile,QFileDevice::Permission(0x6666));
}

QVariant UsdBaseClass::readUserConfigToLightDM(QString group, QString key, QString userName)
{
    QVariant ret;
    QString user = QDir::home().dirName();
    if (!userName.isEmpty()) {
        user = userName;
    }
    QString configFile = QString("/var/lib/lightdm-data/%1/usd/config/ukui-settings-daemon.settings").arg(user);

    QSettings *usdSettings = new QSettings(configFile, QSettings::IniFormat);
    usdSettings->beginGroup(group);
    ret = usdSettings->value(key);
    usdSettings->endGroup();
    usdSettings->sync();
    usdSettings->deleteLater();

    return ret;
}

/*
 *Normally, brightness are controlled by OS, but the following laptop hardware has control over them
*/
bool UsdBaseClass::brightnessControlByHardware(int &step)
{
    static int ret = -1;
    static int hardwareStep;
    //:rnLXKT-ZXE-N70: n70,n80z,n79
    QList<QString> m_appointBrightness = {":rnLXKT-ZXE-N70:"};

    if (ret != -1) {
        step = hardwareStep;
        return ret;
    }

    if (g_motify_poweroff.isEmpty()) {
        readPowerOffConfig();
    }

    Q_FOREACH (const QString &str, m_appointBrightness) {
        if (g_motify_poweroff.contains(str)) {
            ret = 1;
            hardwareStep = 5;
            step = hardwareStep;
            return ret;
        }
    }

    ret = false;
    return ret;
}

/*
 *Normally, the flight mode is controlled by hardware, but the following laptop hardware has surrendered control
*/
bool UsdBaseClass::flightModeControlByHardware(int &mode)
{
    static int ret = -1;
    QList<QString> m_flightControlByHardware = {":rnLXKT-ZXE-N70:"};

    if (ret != -1) {
        mode = RfkillSwitch::instance()->getCurrentFlightMode();
        return ret;
    }

    if (g_motify_poweroff.isEmpty()) {
        readPowerOffConfig();
    }

    Q_FOREACH (const QString &str, m_flightControlByHardware) {
        if (g_motify_poweroff.contains(str)) {
            ret = false;
            break;
        }
    }

    mode = RfkillSwitch::instance()->getCurrentFlightMode();
    ret = ret == -1 ? true : ret;
    return ret;
}


/*
 *Normally, touchpads are controlled by OS, but the following laptop hardware has control over them
*/
bool UsdBaseClass::touchpadControlByHardware(int &state)
{
    static int ret = -1;
    QList<QString> m_touchpadControlByHardware = {":rnLXKT-ZXE-N70:"};

    if (ret == false) {
        return ret;
    }

    if (g_motify_poweroff.isEmpty()) {
        readPowerOffConfig();
    }

    Q_FOREACH (const QString &str, m_touchpadControlByHardware) {
        if (g_motify_poweroff.contains(str)) {
            bool readOK;
            QVariant touchpadState;
            touchpadState = UsdBaseClass::readInfoFromFile(EC_TOUCHPADSTATE);

            state = touchpadState.toUInt(&readOK);
            if (readOK) {
                state = 0;
            } else {
                state = touchpadState.toUInt();
            }

            ret = true;
            return ret;
        }
    }

    ret = false;
    return ret;
}

/*
 *Normally, power mode are controlled by OS, but the following laptop hardware has control over them
*/
bool UsdBaseClass::powerModeControlByHardware(int &mode)
{
    static int ret = -1;
    QList<QString> m_appointPowerMode = {":rnLXKT-ZXE-N70:"};

    if (ret == false) {
        return ret;
    }

    if (g_motify_poweroff.isEmpty()) {
        readPowerOffConfig();
    }

    Q_FOREACH (const QString &str, m_appointPowerMode) {
        if (g_motify_poweroff.contains(str)) {
            QVariant hardMode;
            hardMode = UsdBaseClass::readInfoFromFile(PERFORMANCE_MODE);
            /*TODO
               此类机型只有两种模式
            */
            switch (hardMode.toInt()) {
            case 1:
                mode = 0;//perfromace
                break;
            case 2:
                mode = 2;//eco
                break;
            default:
                break;
            }
            ret = true;
            return ret;
        }
    }

    ret = false;
    return ret;
}

QVariant UsdBaseClass::readInfoFromFile(QString filePath)
{
     QString strData = "";
     QFile file(filePath);
     if (!file.exists()) {
         return false;
     }

     if (file.open(QIODevice::ReadOnly)) {
         strData = QString::fromLocal8Bit(file.readAll());
         file.close();
     }

     return strData;
}
