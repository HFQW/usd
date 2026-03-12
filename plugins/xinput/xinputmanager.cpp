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

#include "xinputmanager.h"
#include <QDBusInterface>

extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include "clib-syslog.h"
}

Atom properyToAtom(const char* property)
{
    return XInternAtom(QX11Info::display(), property, False);
}

XinputManager::XinputManager(QObject *parent):
    QObject(parent)
{
    init();

}

void XinputManager::init()
{
    m_monitorThread = new QThread(this);
    m_inputMonitor = new InputMonitor();
    m_inputMonitor->moveToThread(m_monitorThread);
    connect(m_monitorThread, &QThread::started, m_inputMonitor, &InputMonitor::startMonitor);
    connect(m_monitorThread, &QThread::finished, m_inputMonitor, &InputMonitor::deleteLater);

    connect(m_inputMonitor, &InputMonitor::deviceAdd, this, &XinputManager::deviceAdd);
    connect(m_inputMonitor, &InputMonitor::deviceRemove, this, &XinputManager::deviceRemove);
}

XinputManager::~XinputManager()
{
    stop();
}

void XinputManager::start()
{
    m_monitorThread->start();
}

void XinputManager::stop()
{
    if (m_monitorThread->isRunning()) {
        m_monitorThread->quit();
    }
}

void XinputManager::deviceAdd(int id)
{
    int ndevices;
    XDeviceInfo* deviceList = XListInputDevices(QX11Info::display(),&ndevices);
    for (int i = 0 ; i < ndevices ; ++i) {
        if (id == deviceList[i].id) {
            if (deviceList[i].type == properyToAtom(XI_TOUCHSCREEN) ||
                deviceList[i].type == properyToAtom(XI_TABLET)) {
                USD_LOG(LOG_DEBUG, "new touchscreen/tablet name is %s, id is %d ", deviceList[i].name, deviceList[i].id);
                //绝对坐标映射设备进行映射
                QDBusInterface *interface = new QDBusInterface("org.ukui.SettingsDaemon",
                                                               "/org/ukui/SettingsDaemon/xrandr",
                                                               "org.ukui.SettingsDaemon.xrandr",
                                                               QDBusConnection::sessionBus());
                if (interface && interface->isValid()) {
                    interface->call("setScreenMap");
                    interface->deleteLater();
                }
            }
        }
    }
    XFreeDeviceList(deviceList);
}

void XinputManager::deviceRemove(int id)
{
    // device remove
}


