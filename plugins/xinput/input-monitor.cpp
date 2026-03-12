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
 *
 * Authors: sundagao <sundagao@kylinos.cn>
 */
#include "input-monitor.h"
#include <QTimer>

extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include "clib-syslog.h"
}

int eventSift(XIHierarchyEvent *event, int flag)
{
    int id = 0;
    for (int i = 0; i < event->num_info; ++i) {
        if (event->info[i].flags & flag) {
            id = event->info[i].deviceid;
        }
    }
    return id;
}

InputMonitor::InputMonitor(QObject *parent)
{

}

void InputMonitor::hierarchyChangedEvent(void *data)
{
    XIHierarchyEvent* event = static_cast<XIHierarchyEvent *>(data);
    if (event->flags & XISlaveAdded) {
        Q_EMIT deviceAdd(eventSift(event,XISlaveAdded));

    } else if (event->flags & XISlaveRemoved) {
        Q_EMIT deviceRemove(eventSift(event,XISlaveRemoved));

    } else if (event->flags & XIDeviceEnabled) {

    } else if (event->flags & XIDeviceDisabled) {

    } else if (event->flags & XISlaveAttached) {

    } else if (event->flags & XISlaveDetached) {

    } else if (event->flags & XIMasterAdded) {

    } else if (event->flags & XIMasterRemoved) {

    }
}

void InputMonitor::listeningStart()
{
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        USD_LOG(LOG_WARNING,"listeningStart is faild , because open display error");
        return;
    }
    int xi_opcode ,event ,error;
    if (!XQueryExtension(display, "XInputExtension" , &xi_opcode , &event, &error)) {
        USD_LOG(LOG_WARNING,"X Input extension not available");
        return;
    }
    XIEventMask m;
    m.deviceid = XIAllDevices;
    m.mask_len = XIMaskLen(XI_LASTEVENT);
    m.mask = (unsigned char*)calloc(m.mask_len, sizeof(char));
    XISetMask(m.mask, XI_HierarchyChanged);
    Window win = DefaultRootWindow(display);
    XISelectEvents(display, win, &m, 1);
    XSync(display, False);
    free(m.mask);
    while (true) {
        if (m_stop) {
            USD_LOG(LOG_DEBUG,"input montior has stoped .");
            break;
        }
        XEvent ev;
        XGenericEventCookie *cookie = (XGenericEventCookie*)&ev.xcookie;
        XNextEvent(display, (XEvent*)&ev);
        if (XGetEventData(display, cookie) &&
            cookie->type == GenericEvent &&
            cookie->extension == xi_opcode) {
            switch (cookie->evtype)
            {
                case XI_HierarchyChanged:
                    hierarchyChangedEvent(cookie->data);
                    break;
                default:
                    break;
            }
        }
        XFreeEventData(display, cookie);
    }
    XDestroyWindow(display, win);
}

void InputMonitor::startMonitor()
{
    QTimer::singleShot(0, this, &InputMonitor::listeningStart);
}

void InputMonitor::stopMontior()
{
    m_stop = true;
}

