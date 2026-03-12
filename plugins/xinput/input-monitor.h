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
#ifndef INPUTMONITOR_H
#define INPUTMONITOR_H

#include <QObject>
#include<QX11Info>

class InputMonitor : public QObject
{
    Q_OBJECT
public:
    void stopMontior();
public:
    explicit InputMonitor(QObject *parent = nullptr);
private:
    void hierarchyChangedEvent(void *data);
public Q_SLOTS:
    void startMonitor();

private Q_SLOTS:
    void listeningStart();
Q_SIGNALS:

    void deviceAdd(int id);

    void deviceRemove(int id);
private:
    bool m_stop = false;

};

#endif // INPUTMONITOR_H
