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

#ifndef XINPUTMANAGER_H
#define XINPUTMANAGER_H

#include <QObject>
#include <QThread>
#include "input-monitor.h"

extern "C"{
    #include "clib-syslog.h"
}

class QGSettings;
class XinputManager : public QObject
{
    Q_OBJECT
public:
    XinputManager(QObject *parent = nullptr);
    ~XinputManager();

    void start();
    void stop();

private Q_SLOTS:
    void deviceAdd(int id);
    void deviceRemove(int id);
private:
    void init();
private:
    QThread *m_monitorThread = nullptr;
    InputMonitor* m_inputMonitor = nullptr;
};

#endif // XINPUTMANAGER_H
