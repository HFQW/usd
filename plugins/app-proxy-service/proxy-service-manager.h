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

#ifndef PROXYSERVICEMANAGER_H
#define PROXYSERVICEMANAGER_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <unistd.h>
#include <glib.h>
#include <QtCore>
#include <QJsonArray>
#include <QJsonObject>
#include <QtDBus/QDBusMetaType>
#include <KWindowSystem>
#include <QDebug>

#define APPPROXY_FILE ".config/application-proxy.json"
#define PROXYCONF_FILE ".config/proto-config.json"
#define CUSTOMAPP_FILE "/usr/share/custom_app/custom_app.json"

#define JSON_KEY_APPLICATION "application"
#define DESKTOP_FILE_PATH       "/usr/share/applications/"
#define USR_SHARE_APP_CURRENT   "/usr/share/applications/."
#define USR_SHARE_APP_UPER      "/usr/share/applications/.."

#define DESKTOP_EXEC_KEY "Exec="
#define ANDROID_FILE_PATH       "/.local/share/applications/"
#define ANDROID_APP_CURRENT     "/.local/share/applications/."
#define ANDROID_APP_UPER        "/.local/share/applications/.."

#define PROTOJSON_KEY_TYPE "type"
#define PROTOJSON_KEY_SERVER "Server"
#define PROTOJSON_KEY_PORT "Port"
#define PROTOJSON_KEY_USRNAME "UserName"
#define PROTOJSON_KEY_PASSWORD "Password"
#define PROTOJSON_KEY_NAME "name"
#define PROTOJSON_KEY_STATE "state"

#define APPINFOMAP_KEY_NAME "Name"
#define APPINFOMAP_KEY_LOCALNAME "Localname"
#define APPINFOMAP_KEY_ICON "Icon"
#define APPINFOMAP_KEY_EXEC "Exec"
#define APPINFOMAP_KEY_KEYWORDS "Keywords"
#define APPINFOMAP_KEY_COMMENT "Comment"

#define PROCINFOKEY_TYPE "type"
#define PROCINFOKEY_PID "pid"
#define PROCINFOKEY_CMDLINE "cmdline"
#define PROCINFOKEY_UID "uid"
#define PROCINFOKEY_NAME "name"
#define PROCINFOKEY_STATE "state"
#define PROCINFOKEY_TGID "tgid"
#define PROCINFOKEY_DESKTOP "desktop"

class ThreadObject : public QObject
{
    Q_OBJECT
public:
    explicit ThreadObject(QObject *parent = nullptr);

    void setProxyState(bool state);

    inline void setDesktopList(QStringList list) {
        m_proxyDesktopList = list;
    }

    inline void setExecList(QStringList list) {
        m_proxyExecList = list;
    }

    inline void setNameList(QStringList list) {
        m_proxyNameList = list;
    }

public Q_SLOTS:
    void startConnect();

private:

    void addProcDbus(const qint32 pid);

private:
    QDBusInterface  *m_proxyInterface = nullptr;
    QDBusInterface  *m_procAddServerDbus = nullptr;

    QStringList m_proxyDesktopList;
    QStringList m_proxyExecList;
    QStringList m_proxyNameList;

private Q_SLOTS:
    void onProcAdd(QMap<QString, QString> map);
};

class ProxyServiceManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.ukui.SettingsDaemon.AppProxy")
public:
    explicit ProxyServiceManager(QObject *parent = nullptr);
    void ProxyServiceManagerNew();
    void start();
    void stop();

private:
    void initProxyState();
    void startProxyDbus(const QJsonObject obj);
    void addProxyDbus(const QJsonObject obj);
    void clearProxyDbus();
    void startProxy(const QJsonObject obj);
    void stopProxyDbus();
    void stopProxy();
    void delValueFromArray(QJsonArray *array, const QJsonValue item);
    void setProxyFile(QString desktopfp, bool create);

    QStringList getDesktopFilePath();

    QStringList getCustomizedAppList(QString filePath);
    void recursiveSearchFile(const QString &_filePath);
#ifdef ENABLE_ANDROIDAPP
    void getAndroidApp();
#endif

    inline bool getProxyState() {
        return m_proxyState;
    }
    inline void setProxyState(bool state) {
        m_proxyState = state;
    }

    void initAppInfoMapTemp();
    QMap<QString, QString> getDesktopFileInfo(QString desktopfp);
    void getProxyInfoList();

private:
    QDBusInterface  *m_proxyInterface = nullptr;
    QDBusInterface  *m_procAddServerDbus = nullptr;

    QStringList m_filePathList;
    QStringList m_androidDesktopfnList;

    GError **m_error = nullptr;
    GKeyFileFlags m_flags = G_KEY_FILE_NONE;
    GKeyFile *m_keyfile = nullptr;

    bool m_proxyState = false;

    QTimer *m_time;
    QThread *m_thread;
    ThreadObject *m_threadObj;

    QMap<QString, QMap<QString, QString>> m_appInfoMap;

    QStringList m_proxyDesktopList;
    QStringList m_proxyExecList;
    QStringList m_proxyNameList;

public Q_SLOTS:
    QStringList getProxyConfig();
    void setProxyConfig(const QStringList configList);
    QMap<QString, QStringList> getAppProxy();
    void addAppIntoProxy(QString desktopfp);
    void delAppIntoProxy(QString desktopfp);
    void setProxyStateDbus(bool state);
    bool getProxyStateDbus();

private Q_SLOTS:
    void init();

};

#endif // PROXYSERVICEMANAGER_H
