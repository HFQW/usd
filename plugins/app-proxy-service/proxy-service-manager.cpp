/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2022 KylinSoft Co., Ltd.
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

#include "proxy-service-manager.h"

QJsonObject readJsonFile(QString filePath)
{
    QJsonObject readObj = QJsonObject();
    if (filePath.isEmpty() || !QFile(filePath).exists()) {
        qDebug()<<Q_FUNC_INFO<<__LINE__<<filePath<<"is not exits!";
        return readObj;
    }

    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    QByteArray readBy = file.readAll();
    file.close();
    QJsonParseError error;
    QJsonDocument readDoc = QJsonDocument::fromJson(readBy, &error);
    if (!readDoc.isEmpty() && error.error == QJsonParseError::NoError) {
        readObj = readDoc.object();
    }

    return readObj;
}

QJsonObject dealJsonObj(const QStringList configList)
{
    QJsonObject configObj = QJsonObject();
    if (configList.isEmpty() && configList.count() < 3) {
        qWarning()<<Q_FUNC_INFO<<__LINE__<<"configList item less!";
        return configObj;
    }

    configObj.insert(PROTOJSON_KEY_TYPE, QJsonValue(configList.at(0)));
    configObj.insert(PROTOJSON_KEY_NAME, QJsonValue("default"));
    configObj.insert(PROTOJSON_KEY_SERVER, QJsonValue(configList.at(1)));
    QString prot = configList.at(2);
    configObj.insert(PROTOJSON_KEY_PORT, QJsonValue(prot.toInt()));
    switch (configList.count()) {
    case 4:
        configObj.insert(PROTOJSON_KEY_USRNAME, QJsonValue(configList.at(3)));
        configObj.insert(PROTOJSON_KEY_PASSWORD, QJsonValue(""));
        break;

    case 5:
        configObj.insert(PROTOJSON_KEY_USRNAME, QJsonValue(configList.at(3)));
        configObj.insert(PROTOJSON_KEY_PASSWORD, QJsonValue(configList.at(4)));
        break;

    default:
        configObj.insert(PROTOJSON_KEY_USRNAME, QJsonValue(""));
        configObj.insert(PROTOJSON_KEY_PASSWORD, QJsonValue(""));
        break;
    }
    configObj.insert(PROTOJSON_KEY_STATE, QJsonValue(true));
    return configObj;
}

void wirteJsonFile(QString filePath, const QJsonObject obj)
{
    if (filePath.isEmpty() || obj.isEmpty()) {
        qWarning()<<Q_FUNC_INFO<<__LINE__<<"function input filePath or obj is error!";
        return;
    }
    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    QJsonDocument writeDoc = QJsonDocument(obj);
    file.write(writeDoc.toJson());
    file.flush();
    file.close();
}

QStringList getAppProxyFromFile()
{
    QStringList appProxyList;
    QString jsonPath = QDir::homePath() + "/" + APPPROXY_FILE;
    QJsonObject readObj = readJsonFile(jsonPath);
    QJsonArray appArray = readObj.value("application").toArray();
    if (!appArray.isEmpty()) {
        for (auto appItem : appArray) {
            appProxyList.append(appItem.toString());
        }
    }
    return appProxyList;
}

void ProxyServiceManager::getProxyInfoList()
{
    m_proxyExecList.clear();
    m_proxyNameList.clear();
    for (auto desktop : m_proxyDesktopList) {
        if (m_appInfoMap.contains(desktop)) {
            QMap<QString, QString> map = m_appInfoMap.value(desktop);
            if (map.value(APPINFOMAP_KEY_KEYWORDS).contains("Android")) {
                m_proxyExecList.append(map.value(APPINFOMAP_KEY_COMMENT));
            } else {
                QStringList execlist = map.value(APPINFOMAP_KEY_EXEC).split(" ");
                m_proxyExecList.append(execlist.at(0));
            }
            m_proxyNameList.append(map.value(APPINFOMAP_KEY_ICON));
        }
    }
}

ThreadObject::ThreadObject(QObject *parent) : QObject(parent)
{
    qDBusRegisterMetaType<QMap<QString, QString>>();
}

void ThreadObject::startConnect()
{
    m_proxyInterface = new QDBusInterface("com.kylin.system.proxy",
                                          "/com/kylin/system/proxy/App",
                                          "com.kylin.system.proxy.App",
                                          QDBusConnection::systemBus(), this);
}

void ThreadObject::setProxyState(bool state)
{
    if (m_procAddServerDbus == nullptr) {
        m_procAddServerDbus = new QDBusInterface("com.settings.daemon.qt.systemdbus",
                                                 "/procaddserver",
                                                 "com.settings.daemon.interface",
                                                 QDBusConnection::systemBus(), this);
    }
    if (m_procAddServerDbus->isValid()) {
        if (state) {
            m_procAddServerDbus->call(QDBus::NoBlock, "startListen", getpid());
            connect(m_procAddServerDbus, SIGNAL(procAdd(QMap<QString, QString>)), this, SLOT(onProcAdd(QMap<QString, QString>)), Qt::QueuedConnection);
        } else {
            m_procAddServerDbus->call(QDBus::NoBlock, "stopListen", getpid());
            m_procAddServerDbus->deleteLater();
            m_procAddServerDbus = nullptr;
        }
    }
}

void ThreadObject::onProcAdd(QMap<QString, QString> map)
{
    //应用代理去掉命令类的进程、PID小于等于1的进程、uid不等于当前用户的进程
    if (map.value(PROCINFOKEY_TYPE) == "sys" || map.value(PROCINFOKEY_PID).toInt() <= 1 || map.value(PROCINFOKEY_UID).toInt() != getuid()) {
        return;
    }

    //根据name进行匹配，忽略大小写
    if (!map.value(PROCINFOKEY_NAME).isEmpty() && m_proxyNameList.contains(map.value(PROCINFOKEY_NAME), Qt::CaseInsensitive)) {
        addProcDbus(map.value(PROCINFOKEY_PID).toInt());
        return;
    }

    //根据desktop进行匹配
    if (!map.value(PROCINFOKEY_DESKTOP).isEmpty()) {
        if (m_proxyDesktopList.contains(map.value(PROCINFOKEY_DESKTOP))) {
            addProcDbus(map.value(PROCINFOKEY_PID).toInt());
        } else {
            QStringList list = map.value(PROCINFOKEY_DESKTOP).split("/");
            QString desktop = list.at(list.size() - 1);
            for (QString name : m_proxyDesktopList) {
                if (name.contains(desktop)) {
                    addProcDbus(map.value(PROCINFOKEY_PID).toInt());
                    break;
                }
            }
        }
        return;
    }

    //根据exec进行匹配
    if (!map.value(PROCINFOKEY_CMDLINE).isEmpty()) {
        QString cmdline = map.value(PROCINFOKEY_CMDLINE);
        if (cmdline.isEmpty()) {
            return;
        }
        QStringList execlist = cmdline.split(" ");
        if (m_proxyExecList.contains(execlist.at(0), Qt::CaseInsensitive)) {
            addProcDbus(map.value(PROCINFOKEY_PID).toInt());
        }
    }

}

ProxyServiceManager::ProxyServiceManager(QObject *parent) : QObject(parent)
{
    qDBusRegisterMetaType<QStringList>();
    qDBusRegisterMetaType<QMap<QString, QStringList >>();
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.registerService("org.ukui.SettingsDaemon")) {
        sessionBus.registerObject("/org/ukui/SettingsDaemon/AppProxy",this,
                                  QDBusConnection::ExportAllContents);
    }
}

void ProxyServiceManager::init()
{
    m_time->stop();
    m_proxyInterface = new QDBusInterface("com.kylin.system.proxy",
                                          "/com/kylin/system/proxy/App",
                                          "com.kylin.system.proxy.App",
                                          QDBusConnection::systemBus(), this);

    m_thread = new QThread();
    m_threadObj = new ThreadObject();
    m_threadObj->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_threadObj, &QObject::deleteLater);

    connect(m_thread, &QThread::started, m_threadObj, &ThreadObject::startConnect);
    m_thread->start();

    initProxyState();
}

void ProxyServiceManager::initAppInfoMapTemp()
{
    QStringList desktopfpList = getDesktopFilePath();
    for (QString fd : m_appInfoMap.keys()) {
        if (!desktopfpList.contains(fd)) {
            m_appInfoMap.remove(fd);
        }
    }

    for (QString desktopfp : desktopfpList) {
        if (!m_appInfoMap.contains(desktopfp)) {
            QMap<QString, QString> infoMap = getDesktopFileInfo(desktopfp);
            m_appInfoMap.insert(desktopfp, infoMap);
        }
    }
}

QMap<QString, QString> ProxyServiceManager::getDesktopFileInfo(QString desktopfp)
{
    QMap<QString, QString> filemsg;
    if (desktopfp.isEmpty()) {
        qWarning()<<Q_FUNC_INFO<<"desktop path is empty!";
        return filemsg;
    }
    GError **error = nullptr;
    GKeyFileFlags flags = G_KEY_FILE_NONE;
    GKeyFile *keyfile = g_key_file_new();
    QByteArray fpbyte = desktopfp.toLocal8Bit();
    char *filepath = fpbyte.data();
    g_key_file_load_from_file(keyfile, filepath, flags, error);
    char *name = g_key_file_get_string(keyfile, "Desktop Entry", APPINFOMAP_KEY_NAME, nullptr);
    QString namestr = QString::fromLocal8Bit(name);
    char *localname = g_key_file_get_locale_string(keyfile, "Desktop Entry", APPINFOMAP_KEY_NAME, nullptr, nullptr);
    QString localnamestr = QString::fromLocal8Bit(localname);
    char *icon = g_key_file_get_string(keyfile, "Desktop Entry", APPINFOMAP_KEY_ICON, nullptr);
    QString iconstr = QString::fromLocal8Bit(icon);
    char *exec = g_key_file_get_string(keyfile, "Desktop Entry", APPINFOMAP_KEY_EXEC, nullptr);
    QString execstr = QString::fromLocal8Bit(exec);
    char *keywords = g_key_file_get_string(keyfile, "Desktop Entry", APPINFOMAP_KEY_KEYWORDS, nullptr);
    QString keywordsstr = QString::fromLocal8Bit(keywords);
    char *comment = g_key_file_get_string(keyfile, "Desktop Entry", APPINFOMAP_KEY_COMMENT, nullptr);
    QString commentstr = QString::fromLocal8Bit(comment);

    filemsg.insert(APPINFOMAP_KEY_NAME, namestr);
    filemsg.insert(APPINFOMAP_KEY_LOCALNAME, localnamestr);
    filemsg.insert(APPINFOMAP_KEY_ICON, iconstr);
    filemsg.insert(APPINFOMAP_KEY_KEYWORDS, keywordsstr);
    filemsg.insert(APPINFOMAP_KEY_EXEC, execstr);
    filemsg.insert(APPINFOMAP_KEY_COMMENT, commentstr);
    g_key_file_free(keyfile);
    return filemsg;
}

void ProxyServiceManager::initProxyState()
{
    QString jsonPath = QDir::homePath() + "/" + PROXYCONF_FILE;
    QJsonObject readObj = readJsonFile(jsonPath);
    if (!readObj.value(PROTOJSON_KEY_TYPE).toString().isNull()
        && !readObj.value(PROTOJSON_KEY_SERVER).toString().isNull()
        && !readObj.value(PROTOJSON_KEY_PORT).isNull()
        && readObj.value(PROTOJSON_KEY_STATE).toBool()) {
        startProxy(readObj);

        initAppInfoMapTemp();
        m_proxyDesktopList = getAppProxyFromFile();
        getProxyInfoList();
        m_threadObj->setDesktopList(m_proxyDesktopList);
        m_threadObj->setExecList(m_proxyExecList);
        m_threadObj->setNameList(m_proxyNameList);
    } else {
        stopProxy();
    }
}

void ProxyServiceManager::start()
{
    qDebug()<<"ProxyServiceManager ---------------start";
    m_time = new QTimer(this);
    connect(m_time, SIGNAL(timeout()), this, SLOT(init()));
    m_time->start(10);
}

void ProxyServiceManager::stop()
{
    qDebug()<<"ProxyServiceManager ---------------stop";
}

void ThreadObject::addProcDbus(const qint32 pid)
{
    if (!m_proxyInterface->isValid()) {
        qWarning()<<Q_FUNC_INFO<<__LINE__<<"m_proxyInterface dbus is not valid!";
        return;
    }

    m_proxyInterface->call("AddProc", pid);
}

void ProxyServiceManager::startProxyDbus(const QJsonObject obj)
{
    if (obj.isEmpty()) {
        qWarning()<<Q_FUNC_INFO<<__LINE__<<"obj is error!";
        return;
    }

    if (!m_proxyInterface->isValid()) {
        qDebug()<<Q_FUNC_INFO<<__LINE__<<"m_proxyInterface dbus is not valid!";
        return;
    }

    QString proto = obj.value(PROTOJSON_KEY_TYPE).toString();
    m_proxyInterface->call("StartProxy", proto, "default", false);
}

void ProxyServiceManager::addProxyDbus(const QJsonObject obj)
{
    if (obj.isEmpty()) {
        qWarning()<<Q_FUNC_INFO<<__LINE__<<"obj is error!";
        return;
    }

    if (!m_proxyInterface->isValid()) {
        qDebug()<<Q_FUNC_INFO<<__LINE__<<"m_proxyInterface dbus is not valid!";
        return;
    }

    QJsonObject confObj = obj;
    confObj.remove(PROTOJSON_KEY_STATE);

    QString proto = confObj.value(PROTOJSON_KEY_TYPE).toString();
    QByteArray objArray = QJsonDocument(confObj).toJson();
    m_proxyInterface->call("AddProxy", proto, "default", objArray);
}

void ProxyServiceManager::clearProxyDbus()
{
    if (!m_proxyInterface->isValid()) {
        qDebug()<<Q_FUNC_INFO<<__LINE__<<"m_proxyInterface dbus is not valid!";
        return;
    }

    m_proxyInterface->call("ClearProxy");
}

void ProxyServiceManager::startProxy(const QJsonObject obj)
{
    clearProxyDbus();
    addProxyDbus(obj);
    startProxyDbus(obj);
    m_threadObj->setProxyState(true);
    setProxyState(true);
}

void ProxyServiceManager::stopProxyDbus()
{
    if (!m_proxyInterface->isValid()) {
        qDebug()<<Q_FUNC_INFO<<__LINE__<<"m_proxyInterface dbus is not valid!";
        return;
    }

    m_proxyInterface->call("StopProxy");
}

void ProxyServiceManager::stopProxy()
{
    stopProxyDbus();
    QString jsonPath = QDir::homePath() + "/" + PROXYCONF_FILE;
    QJsonObject protoObj = readJsonFile(jsonPath);
    protoObj.insert(PROTOJSON_KEY_STATE, QJsonValue(false));
    wirteJsonFile(jsonPath, protoObj);
    m_threadObj->setProxyState(false);
    setProxyState(false);
}


QStringList ProxyServiceManager::getProxyConfig()
{
    QStringList proxyConfList;
    proxyConfList.clear();

    QString jsonPath = QDir::homePath() + "/" + PROXYCONF_FILE;
    QJsonObject readObj = readJsonFile(jsonPath);
    proxyConfList.append(readObj.value(PROTOJSON_KEY_TYPE).toString());
    proxyConfList.append(readObj.value(PROTOJSON_KEY_SERVER).toString());
    QString portStr = QString::number(readObj.value(PROTOJSON_KEY_PORT).toInt());
    proxyConfList.append(portStr);
    proxyConfList.append(readObj.value(PROTOJSON_KEY_USRNAME).toString());
    proxyConfList.append(readObj.value(PROTOJSON_KEY_PASSWORD).toString());

    return proxyConfList;
}

void ProxyServiceManager::setProxyConfig(const QStringList configList)
{
    QString jsonPath = QDir::homePath() + "/" + PROXYCONF_FILE;
    QJsonObject readObj = readJsonFile(jsonPath);
    QJsonObject configObj = dealJsonObj(configList);
    startProxy(configObj);
    if (configObj != readObj) {
        wirteJsonFile(jsonPath, configObj);
    }
}

void ProxyServiceManager::delValueFromArray(QJsonArray *array, const QJsonValue item)
{
    if (array != nullptr && !array->isEmpty()) {
        for (int i = 0; i < array->count(); i++) {
            if (array->at(i) == item) {
                array->removeAt(i);
                return;
            }
        }
    }
}

void ProxyServiceManager::setProxyFile(QString desktopfp, bool create)
{
    QString jsonPath = QDir::homePath() + "/" + APPPROXY_FILE;
    QJsonObject readObj = readJsonFile(jsonPath);
    QJsonObject writeObj = readObj;

    if (readObj.isEmpty()) {
        if (create) {
            QJsonArray appArray = QJsonArray();
            appArray.append(QJsonValue(desktopfp));
            writeObj.insert(JSON_KEY_APPLICATION, appArray);
            qDebug()<<Q_FUNC_INFO<<__LINE__<<desktopfp<<" add to proxy list";
        } else {
            qDebug()<<Q_FUNC_INFO<<__LINE__<<jsonPath<<"is error!";
        }
    } else {
        QJsonArray appArray = readObj.value(JSON_KEY_APPLICATION).toArray();
        QJsonValue arrayItem = QJsonValue(desktopfp);
        if (create && !appArray.contains(arrayItem)) {
            appArray.append(arrayItem);
        }
        if (!create && appArray.contains(arrayItem)) {
            delValueFromArray(&appArray, arrayItem);
        }
        writeObj.insert(JSON_KEY_APPLICATION, appArray);
    }

    if (writeObj != readObj) {
        wirteJsonFile(jsonPath, writeObj);
    }
}

void ProxyServiceManager::addAppIntoProxy(QString desktopfp)
{
    if (desktopfp.isEmpty()) {
        qWarning()<<Q_FUNC_INFO<<__LINE__<<"desktopfp is Empty!";
        return;
    }

    setProxyFile(desktopfp, true);
    m_proxyDesktopList = getAppProxyFromFile();
    getProxyInfoList();
    m_threadObj->setDesktopList(m_proxyDesktopList);
    m_threadObj->setExecList(m_proxyExecList);
    m_threadObj->setNameList(m_proxyNameList);
}

void ProxyServiceManager::delAppIntoProxy(QString desktopfp)
{
    if (desktopfp.isEmpty()) {
        qWarning()<<Q_FUNC_INFO<<__LINE__<<"desktopfp is Empty!";
        return;
    }

    setProxyFile(desktopfp, false);
    m_proxyDesktopList = getAppProxyFromFile();
    getProxyInfoList();
    m_threadObj->setDesktopList(m_proxyDesktopList);
    m_threadObj->setExecList(m_proxyExecList);
    m_threadObj->setNameList(m_proxyNameList);
}

void ProxyServiceManager::setProxyStateDbus(bool state)
{
    if (state) {
        QString jsonPath = QDir::homePath() + "/" + PROXYCONF_FILE;
        QJsonObject proxyObj = readJsonFile(jsonPath);
        proxyObj.insert(PROTOJSON_KEY_STATE, QJsonValue(true));
        wirteJsonFile(jsonPath, proxyObj);
        startProxy(proxyObj);
    } else {
        stopProxy();
    }
}

bool ProxyServiceManager::getProxyStateDbus()
{
    QString jsonPath = QDir::homePath() + "/" + PROXYCONF_FILE;
    QJsonObject readObj = readJsonFile(jsonPath);
    return readObj.value(PROTOJSON_KEY_STATE).toBool();
}

QMap<QString, QStringList> ProxyServiceManager::getAppProxy()
{
    initAppInfoMapTemp();
    m_proxyDesktopList = getAppProxyFromFile();
    getProxyInfoList();

    QStringList customAppList = getCustomizedAppList(CUSTOMAPP_FILE);
    QMap<QString, QStringList> appPathMap;
    for (auto desktopfp : m_appInfoMap.keys()) {
        QMap<QString, QString> map = m_appInfoMap.value(desktopfp);
        QString name = map.value(APPINFOMAP_KEY_LOCALNAME);
        QString icon = map.value(APPINFOMAP_KEY_ICON);

        //定制软件判断，若文档是空的，则认为没有开定制功能
        if (!customAppList.isEmpty() && !customAppList.contains(desktopfp)) {
            continue;
        }

        QStringList appInfoList;
        appInfoList.append(name);
        appInfoList.append(icon);
        if (!m_proxyDesktopList.isEmpty() && m_proxyDesktopList.contains(desktopfp)) {
            appInfoList.append("true");
        } else {
            appInfoList.append("false");
        }
        appPathMap.insert(desktopfp, appInfoList);
    }

    return appPathMap;
}

//获取定制软件列表
QStringList ProxyServiceManager::getCustomizedAppList(QString filePath)
{
    if (!QFile(filePath).exists()) {
        return QStringList();
    }
    QStringList customizedAppList;
    QJsonObject readObj = readJsonFile(filePath);
    QJsonArray appArray = readObj.value(JSON_KEY_APPLICATION).toArray();
    for (auto arrayItem : appArray) {
        customizedAppList.append(arrayItem.toString());
    }

    return customizedAppList;
}

//获取系统desktop文件路径
QStringList ProxyServiceManager::getDesktopFilePath()
{
    m_filePathList.clear();
    QString jsonPath = QDir::homePath() + "/.config/ukui-menu-security-config.json";
    QFile file(jsonPath);

    if (file.exists()) {
        file.open(QIODevice::ReadOnly);
        QByteArray readBy = file.readAll();
        QJsonParseError error;
        QJsonDocument readDoc = QJsonDocument::fromJson(readBy, &error);

        if (!readDoc.isNull() && error.error == QJsonParseError::NoError) {
            QJsonObject obj = readDoc.object().value("ukui-menu").toObject();

            if (obj.value("mode").toString() == "whitelist") {
                QJsonArray blArray = obj.value("whitelist").toArray();
                QJsonArray enArray = blArray.at(0).toObject().value("entries").toArray();

                for (int index = 0; index < enArray.size(); index++) {
                    QJsonObject obj = enArray.at(index).toObject();
                    m_filePathList.append(obj.value("path").toString());
                }

                return m_filePathList;
            } else if (obj.value("mode").toString() == "blacklist") {
#ifdef ENABLE_ANDROIDAPP
                getAndroidApp();
#endif
                recursiveSearchFile("/usr/share/applications/");
                recursiveSearchFile("/var/lib/snapd/desktop/applications/");
                recursiveSearchFile("/var/lib/flatpak/exports/share/applications/");
                QJsonArray blArray = obj.value("blacklist").toArray();
                QJsonArray enArray = blArray.at(0).toObject().value("entries").toArray();

                for (int index = 0; index < enArray.size(); index++) {
                    QJsonObject obj = enArray.at(index).toObject();
                    m_filePathList.removeAll(obj.value("path").toString());
                }
            } else {
#ifdef ENABLE_ANDROIDAPP
                getAndroidApp();
#endif
                recursiveSearchFile("/usr/share/applications/");
                recursiveSearchFile("/var/lib/snapd/desktop/applications/");
                recursiveSearchFile("/var/lib/flatpak/exports/share/applications/");
            }
        }

        file.close();
    } else {
#ifdef ENABLE_ANDROIDAPP
        getAndroidApp();
#endif
        recursiveSearchFile("/usr/share/applications/");
        recursiveSearchFile("/var/lib/snapd/desktop/applications/");
        recursiveSearchFile("/var/lib/flatpak/exports/share/applications/");
    }

    m_filePathList.removeAll("/usr/share/applications/software-properties-livepatch.desktop");
    m_filePathList.removeAll("/usr/share/applications/mate-color-select.desktop");
    m_filePathList.removeAll("/usr/share/applications/blueman-adapters.desktop");
    m_filePathList.removeAll("/usr/share/applications/mate-user-guide.desktop");
    m_filePathList.removeAll("/usr/share/applications/nm-connection-editor.desktop");
    m_filePathList.removeAll("/usr/share/applications/debian-uxterm.desktop");
    m_filePathList.removeAll("/usr/share/applications/debian-xterm.desktop");
    m_filePathList.removeAll("/usr/share/applications/im-config.desktop");
    m_filePathList.removeAll("/usr/share/applications/fcitx.desktop");
    m_filePathList.removeAll("/usr/share/applications/fcitx-configtool.desktop");
    m_filePathList.removeAll("/usr/share/applications/onboard-settings.desktop");
    m_filePathList.removeAll("/usr/share/applications/info.desktop");
    m_filePathList.removeAll("/usr/share/applications/ukui-power-preferences.desktop");
    m_filePathList.removeAll("/usr/share/applications/ukui-power-statistics.desktop");
    m_filePathList.removeAll("/usr/share/applications/software-properties-drivers.desktop");
    m_filePathList.removeAll("/usr/share/applications/software-properties-gtk.desktop");
    m_filePathList.removeAll("/usr/share/applications/gnome-session-properties.desktop");
    m_filePathList.removeAll("/usr/share/applications/org.gnome.font-viewer.desktop");
    m_filePathList.removeAll("/usr/share/applications/xdiagnose.desktop");
    m_filePathList.removeAll("/usr/share/applications/gnome-language-selector.desktop");
    m_filePathList.removeAll("/usr/share/applications/mate-notification-properties.desktop");
    m_filePathList.removeAll("/usr/share/applications/transmission-gtk.desktop");
    m_filePathList.removeAll("/usr/share/applications/mpv.desktop");
    m_filePathList.removeAll("/usr/share/applications/system-config-printer.desktop");
    m_filePathList.removeAll("/usr/share/applications/org.gnome.DejaDup.desktop");
    m_filePathList.removeAll("/usr/share/applications/yelp.desktop");
    //v10
    m_filePathList.removeAll("/usr/share/applications/mate-about.desktop");
    m_filePathList.removeAll("/usr/share/applications/time.desktop");
    m_filePathList.removeAll("/usr/share/applications/network.desktop");
    m_filePathList.removeAll("/usr/share/applications/shares.desktop");
    m_filePathList.removeAll("/usr/share/applications/mate-power-statistics.desktop");
    m_filePathList.removeAll("/usr/share/applications/display-im6.desktop");
    m_filePathList.removeAll("/usr/share/applications/display-im6.q16.desktop");
    m_filePathList.removeAll("/usr/share/applications/openjdk-8-policytool.desktop");
    m_filePathList.removeAll("/usr/share/applications/kylin-io-monitor.desktop");
    m_filePathList.removeAll("/usr/share/applications/wps-office-uninstall.desktop");
    m_filePathList.removeAll("/usr/share/applications/wps-office-misc.desktop");
    QStringList desktopList;

    for (int i = 0; i < m_filePathList.count(); ++i) {
        QString filepath = m_filePathList.at(i);
        int list_index = filepath.lastIndexOf('/');
        QString desktopName = filepath.right(filepath.length() - list_index - 1);

        if (desktopList.contains(desktopName)) {
            m_filePathList.removeAll(filepath);
            i--;
        } else {
            desktopList.append(desktopName);
        }
    }
    return m_filePathList;
}

//文件递归查询
void ProxyServiceManager::recursiveSearchFile(const QString &_filePath)
{
    QDir dir(_filePath);

    if (!dir.exists()) {
        return;
    }

    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst);
    QFileInfoList list = dir.entryInfoList();
    list.removeAll(QFileInfo("/usr/share/applications/screensavers"));

    if (list.size() < 1) {
        return;
    }

    int i = 0;

    //递归算法的核心部分
    do {
        QFileInfo fileInfo = list.at(i);
        //如果是文件夹，递归
        bool isDir = fileInfo.isDir();

        if (isDir) {
            recursiveSearchFile(fileInfo.filePath());
        } else {
            //过滤后缀不是.desktop的文件
            QString filePathStr = fileInfo.filePath();

            if (!filePathStr.endsWith(".desktop")) {
                i++;
                continue;
            }

            QByteArray fpbyte = filePathStr.toLocal8Bit();
            char *filepath = fpbyte.data();

            if (0 != access(filepath, R_OK)) { //判断文件是否可读
                i++;
                continue;
            }

            m_keyfile = g_key_file_new();

            if (!g_key_file_load_from_file(m_keyfile, filepath, m_flags, m_error)) {
                return;
            }

            char *ret_0 = g_key_file_get_locale_string(m_keyfile, "Desktop Entry", "Categories", nullptr, nullptr);

            if (ret_0 != nullptr) {
                QString str = QString::fromLocal8Bit(ret_0);

                if (str.contains("Android")) {
                    g_key_file_free(m_keyfile);
                    i++;
                    continue;
                }
            }

            char *ret_1 = g_key_file_get_locale_string(m_keyfile, "Desktop Entry", "NoDisplay", nullptr, nullptr);

            if (ret_1 != nullptr) {
                QString str = QString::fromLocal8Bit(ret_1);

                if (str.contains("true")) {
                    g_key_file_free(m_keyfile);
                    i++;
                    continue;
                }
            }

            char *ret_2 = g_key_file_get_locale_string(m_keyfile, "Desktop Entry", "NotShowIn", nullptr, nullptr);

            if (ret_2 != nullptr) {
                QString str = QString::fromLocal8Bit(ret_2);

                if (str.contains("UKUI")) {
                    g_key_file_free(m_keyfile);
                    i++;
                    continue;
                }
            }

            //过滤LXQt、KDE
            char *ret = g_key_file_get_locale_string(m_keyfile, "Desktop Entry", "OnlyShowIn", nullptr, nullptr);

            if (ret != nullptr) {
                QString str = QString::fromLocal8Bit(ret);

                if (str.contains("LXQt") || str.contains("KDE")) {
                    g_key_file_free(m_keyfile);
                    i++;
                    continue;
                }
            }

            g_key_file_free(m_keyfile);
            m_filePathList.append(filePathStr);
        }

        i++;
    } while (i < list.size());
}

#ifdef ENABLE_ANDROIDAPP
void ProxyServiceManager::getAndroidApp()
{
    m_androidDesktopfnList.clear();
    QVector<QStringList> androidVector;
    androidVector.clear();
    QString path = QDir::homePath() + "/.local/share/applications/";
    QDir dir(path);

    if (!dir.exists()) {
        return;
    }

    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst);
    QFileInfoList list = dir.entryInfoList();

    if (list.size() < 1) {
        return;
    }

    int i = 0;
    GKeyFile *keyfile = g_key_file_new();

    do {
        QFileInfo fileInfo = list.at(i);

        if (!fileInfo.isFile()) {
            i++;
            continue;
        }

        //过滤后缀不是.desktop的文件
        QString filePathStr = fileInfo.filePath();

        if (!filePathStr.endsWith(".desktop")) {
            i++;
            continue;
        }

        QByteArray fpbyte = filePathStr.toLocal8Bit();
        char *filepath = fpbyte.data();

        if (0 != access(filepath, R_OK)) { //判断文件是否可读
            i++;
            continue;
        }

        m_keyfile = g_key_file_new();

        if (!g_key_file_load_from_file(m_keyfile, filepath, m_flags, m_error)) {
            return;
        }

        char *ret_1 = g_key_file_get_locale_string(m_keyfile, "Desktop Entry", "NoDisplay", nullptr, nullptr);

        if (ret_1 != nullptr) {
            QString str = QString::fromLocal8Bit(ret_1);

            if (str.contains("true")) {
                g_key_file_free(m_keyfile);
                i++;
                continue;
            }
        }

        char *ret_2 = g_key_file_get_locale_string(m_keyfile, "Desktop Entry", "NotShowIn", nullptr, nullptr);

        if (ret_2 != nullptr) {
            QString str = QString::fromLocal8Bit(ret_2);

            if (str.contains("UKUI")) {
                g_key_file_free(m_keyfile);
                i++;
                continue;
            }
        }

        //过滤LXQt、KDE
        char *ret = g_key_file_get_locale_string(m_keyfile, "Desktop Entry", "OnlyShowIn", nullptr, nullptr);

        if (ret != nullptr) {
            QString str = QString::fromLocal8Bit(ret);

            if (str.contains("LXQt") || str.contains("KDE")) {
                g_key_file_free(m_keyfile);
                i++;
                continue;
            }
        }

        m_filePathList.append(filePathStr);
        m_androidDesktopfnList.append(fileInfo.fileName());
        i++;
    } while (i < list.size());

    g_key_file_free(keyfile);
}
#endif
