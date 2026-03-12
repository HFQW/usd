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

#include "touch-calibrate.h"

extern "C"{
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/Xrandr.h>
#include <xorg/xserver-properties.h>
#include <gudev/gudev.h>
#include "clib-syslog.h"
}


//尺寸检测
bool TouchCalibrate::checkMatch(double output_width,  double output_height,
                double input_width, double input_height)
{
    double w_diff, h_diff;
    w_diff = ABS(1 - (output_width / input_width));
    h_diff = ABS(1 - (output_height / input_height));

    SYS_LOG(LOG_DEBUG,"w_diff--------%f,h_diff----------%f", w_diff, h_diff);
    if (w_diff < MAX_SIZE_MATCH_DIFF && h_diff < MAX_SIZE_MATCH_DIFF) {
        return true;
    }
    return false;
}

//获取设备节点
QString TouchCalibrate::getDeviceNode(int id)
{
    QString node;
    Atom  prop;
    Atom act_type;
    int  act_format;
    unsigned long nitems, bytes_after;
    unsigned char *data;
    prop = XInternAtom(m_display, XI_PROP_DEVICE_NODE, False);
    if (!prop) {
        return node;
    }

    if (XIGetProperty(m_display, id, prop, 0, 1000, False,
                      AnyPropertyType, &act_type, &act_format, &nitems, &bytes_after, &data) == Success) {
         node = QString::fromLatin1(reinterpret_cast<char*>(data));
         XFree(data);
    }
    return node;
}
//获取触摸设备物理尺寸
void TouchCalibrate::getTouchSize(const char* node, int& width, int& height)
{
    const char *udevSubsystems[] = {"input", NULL};
    GUdevClient *udevClient = g_udev_client_new(udevSubsystems);
    if (!udevClient) {
        SYS_LOG(LOG_DEBUG, " Failed to new udev client.");
        return;
    }
    GUdevDevice *udevDevice = g_udev_client_query_by_device_file(udevClient, node);

    if (g_udev_device_has_property(udevDevice, "ID_INPUT_WIDTH_MM")) {
        width = g_udev_device_get_property_as_uint64(udevDevice, "ID_INPUT_WIDTH_MM");
    }
    if (g_udev_device_has_property(udevDevice, "ID_INPUT_HEIGHT_MM")) {
        height = g_udev_device_get_property_as_uint64(udevDevice, "ID_INPUT_HEIGHT_MM");
    }
    g_clear_object(&udevClient);
}

TouchCalibrate::TouchCalibrate(const QString& path, QObject *parent)
    : QObject(parent)
    , m_display(XOpenDisplay(NULL))
    , m_touchConfigPath(path)
{

}

TouchCalibrate::~TouchCalibrate()
{
    if (m_display) {
        XCloseDisplay(m_display);
    }
    m_screenInfoMap.clear();
    m_touchScreenMap.clear();
    m_tabletMap.clear();
    m_touchConfigList.clear();
}

bool TouchCalibrate::initDisplay()
{

}

//获取屏幕列表
void TouchCalibrate::getScreenList()
{
    int event_base, error_base, major, minor;
    Window  root;
    XRRScreenResources *res;

    if (!XRRQueryExtension(m_display, &event_base, &error_base) ||
        !XRRQueryVersion(m_display, &major, &minor)) {
        SYS_LOG(LOG_ERR, "RandR extension missing.");
        return;
    }
    root = DefaultRootWindow(m_display);
    if (major >= 1 && minor >= 5) {
        res = XRRGetScreenResources(m_display, root);
        if (!res) {
            SYS_LOG(LOG_ERR, "get screen resources failed");
            return;
        }

        for (int o = 0; o < res->noutput; ++o) {
            XRROutputInfo *output = XRRGetOutputInfo(m_display, res, res->outputs[o]);
            if (!output){
                SYS_LOG(LOG_ERR, "could not get output.");
                continue;
            }
            if (output->connection == RR_Connected) {
                ScreenInfoPtr screen(new ScreenInfo);
                screen->name = QString::fromLatin1(output->name);
                screen->width = output->mm_width;
                screen->height = output->mm_height;
                m_screenInfoMap.insert(screen->name, screen);
                SYS_LOG(LOG_DEBUG, "%s  width : %d height : %d",
                        screen->name.toLatin1().data(), screen->width, screen->height);
            }
            XRRFreeOutputInfo(output);
        }
        XRRFreeScreenResources(res);
    }
}
// 获取触摸设备列表
void TouchCalibrate::getTouchDeviceList()
{
    int ndevices;
    XDeviceInfo* deviceList = XListInputDevices(m_display ,&ndevices);
    for (int i = 0 ; i < ndevices ; ++i) {
        XDeviceInfo device = deviceList[i];
        if (device.type == XInternAtom(m_display, XI_TOUCHSCREEN, False)) {
            QString node = getDeviceNode(device.id);
            if (!node.isEmpty()) {
                TouchDevicePtr dev(new TouchDevice);
                dev->id = device.id;
                dev->name = QString::fromLatin1(device.name);
                dev->node = node;
                getTouchSize(node.toLatin1().data(), dev->width, dev->height);
                SYS_LOG(LOG_DEBUG, "%s id : %d node: %s width : %d height : %d",
                        dev->name.toLatin1().data(), dev->id, dev->node.toLatin1().data(), dev->width, dev->height);
                m_touchScreenMap.insert(dev->name, dev);
            }
        } else if (device.type == XInternAtom(m_display, XI_TABLET, False)) {
            QString node = getDeviceNode(device.id);
            if (!node.isEmpty()) {
                TouchDevicePtr dev(new TouchDevice);
                dev->id = device.id;
                dev->name = QString::fromLatin1(device.name);
                dev->node = node;
                getTouchSize(node.toLatin1().data(), dev->width, dev->height);
                SYS_LOG(LOG_DEBUG, "%s id : %d node: %s width : %d height : %d",
                        dev->name.toLatin1().data(), dev->id, dev->node.toLatin1().data(), dev->width, dev->height);
                m_tabletMap.insert(dev->name, dev);
            }
        }
    }
    XFreeDeviceList(deviceList);
}

//获取触摸配置
void TouchCalibrate::getTouchConfigure()
{
    QFileInfo file(m_touchConfigPath);
    if(file.exists()) {
        QSettings *configSettings = new QSettings(m_touchConfigPath, QSettings::IniFormat);
        int mapNum = configSettings->value("/COUNT/num").toInt();
        if (mapNum < 1) {
            return;
        }
        for (int i = 0; i < mapNum ;++i) {
            QString mapName = QString("/MAP%1/%2");
            QString touchName = configSettings->value(mapName.arg(i+1).arg("name")).toString();
            if(touchName.isEmpty()) {
                continue;
            }

            QString scrname = configSettings->value(mapName.arg(i+1).arg("scrname")).toString();
            if(scrname.isEmpty()) {
                continue;
            }

            QString serial = configSettings->value(mapName.arg(i+1).arg("serial")).toString();

            // serial 不判空，暂不关注，无法通过序列号匹配屏幕
            TouchConfigPtr touchConfig(new TouchConfig);
            touchConfig->sTouchName = touchName;
            touchConfig->sMonitorName = scrname;
            touchConfig->sTouchSerial = serial;
            m_touchConfigList.append(touchConfig);
        }
        configSettings->deleteLater();
    }
}

bool TouchCalibrate::hasTouchDevice()
{
    getTouchDeviceList();
    int ret = m_touchScreenMap.count() + m_tabletMap.count();
    SYS_LOG(LOG_DEBUG," has touch device ??? ,ret = %d",ret) ;
    
    return ret;
}
//进行映射
void TouchCalibrate::calibrate()
{
    if (!m_display) {
        SYS_LOG(LOG_DEBUG, "Failed to get x display");
        return;
    }
    //获取设备
    getScreenList();
    getTouchDeviceList();
    getTouchConfigure();

    calibrateTouchScreen();
    calibrateTablet();
}

//touchscreen
void TouchCalibrate::calibrateTouchScreen()
{
    //首先配置文件映射
    Q_FOREACH (const TouchConfigPtr& touchConfig, m_touchConfigList) {
        TouchDevicePtr touch = m_touchScreenMap.value(touchConfig->sTouchName);
        if (touch.data()) {
            ScreenInfoPtr screen = m_screenInfoMap.value(touchConfig->sMonitorName);
            if (screen.data()) {
                calibrateDevice(touch->id, screen->name);
                touch->isMapped = true;
                screen->isMapped = true;
            }
        }
    }
    //如果配置文件无法覆盖所有的屏幕组合，继续通过比较触摸设备和屏幕物理尺寸进行映射
    TouchDeviceMap::iterator it_tc = m_touchScreenMap.begin();
    while (it_tc != m_touchScreenMap.end()) {
        if (it_tc.value()->isMapped) {
            ++it_tc;
            continue;
        }
        ScreenInfoMap::iterator it_sc = m_screenInfoMap.begin();
        while (it_sc != m_screenInfoMap.end()) {
            if (it_sc.value()->isMapped) {
                ++it_sc;
                continue;
            }
            if (checkMatch(it_sc.value()->width, it_sc.value()->height, it_tc.value()->width, it_tc.value()->height)) {
                calibrateDevice(it_tc.value()->id, it_sc.value()->name);
                it_tc.value()->isMapped = true;
                it_sc.value()->isMapped = true;
            }
            ++it_sc;
        }
        ++it_tc;
    }

    //通过配置文件，尺寸比较后，仍然无法映射所有触摸设备与屏幕，则对剩下的设备进行随机映射
    //这里让一个touch设备去映射了多个screen设备 -> 按理应该是:一个screen设备被一个touch设备所映射
    it_tc = m_touchScreenMap.begin();
    while (it_tc != m_touchScreenMap.end()) {
        if (it_tc.value()->isMapped) {
            ++it_tc;
            continue;
        }
        ScreenInfoMap::iterator it_sc = m_screenInfoMap.begin();
        while (it_sc != m_screenInfoMap.end()) {
            if (it_sc.value()->isMapped) {
                ++it_sc;
                continue;
            }
	    //随机映射时，不进行映射到内屏
	    if(it_sc.value()->name.contains("eDP",Qt::CaseInsensitive) )
	    {
		++it_sc;
		continue;
	    }
            //此处不再比较触摸设备与屏幕大小，为映射过的设备和屏幕进行随机映射
            calibrateDevice(it_tc.value()->id, it_sc.value()->name);
	    ++it_sc;
        }
        ++it_tc;
    }
}

//tablet
void TouchCalibrate::calibrateTablet()
{
    //触摸笔映射，重置屏幕映射
    ScreenInfoMap::iterator it_sc = m_screenInfoMap.begin();
    while (it_sc != m_screenInfoMap.end()) {
        if (it_sc.value()->isMapped) {
            it_sc.value()->isMapped = false;
        }
        ++it_sc;
    }
    //触摸笔暂无配置文件，通过比较触摸设备和屏幕物理尺寸进行映射
    TouchDeviceMap::iterator it_tc = m_tabletMap.begin();
    while (it_tc != m_tabletMap.end()) {
        if (it_tc.value()->isMapped) {
            ++it_tc;
            continue;
        }
        it_sc = m_screenInfoMap.begin();
        while (it_sc != m_screenInfoMap.end()) {
            if (it_sc.value()->isMapped) {
                ++it_sc;
                continue;
            }
            if (checkMatch(it_sc.value()->width, it_sc.value()->height, it_tc.value()->width, it_tc.value()->height)) {
                calibrateDevice(it_tc.value()->id, it_sc.value()->name);
                it_tc.value()->isMapped = true;
                it_sc.value()->isMapped = true;
            }
            ++it_sc;
        }
        ++it_tc;
    }
    //尺寸比较后，仍然无法映射所有触摸设备与屏幕，则对剩下的设备进行随机映射
    it_tc = m_tabletMap.begin();
    while (it_tc != m_tabletMap.end()) {
        if (it_tc.value()->isMapped) {
            ++it_tc;
            continue;
        }
        it_sc = m_screenInfoMap.begin();
        while (it_sc != m_screenInfoMap.end()) {
            if (it_sc.value()->isMapped) {
                ++it_sc;
                continue;
            }
            //此处不再比较触摸设备与屏幕大小，未映射过的设备和屏幕进行随机映射
            calibrateDevice(it_tc.value()->id, it_sc.value()->name);
            ++it_sc;
        }
        ++it_tc;
    }
}

void TouchCalibrate::calibrateDevice(int id, const QString& output)
{
    QStringList arguments;
    arguments << "--map-to-output" << QString::number(id) << output;
    QProcess process;
    process.setProgram("xinput");
    process.setArguments(arguments);
    if (!process.startDetached()) {
        SYS_LOG(LOG_DEBUG, "xinput map to output failed");
    }
    SYS_LOG(LOG_DEBUG, "xinput touch device map to output [%d : %s]", id, output.toLatin1().data());
}
