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

#ifndef GAMMAMANAGERWAYLAND_H
#define GAMMAMANAGERWAYLAND_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QObject>
#include <QTimer>
#include "gamma-color-info.h"
#include "plugin-manager-interface.h"
#include "clib-syslog.h"
#include "gamma-manager-gtkconfig.h"
#include "gamma-manager-define.h"
#include "QGSettings/qgsettings.h"

class GammaManagerWayland: public ManagerInterface
{
    Q_OBJECT
public:
    GammaManagerWayland();
    GammaManagerWayland(GammaManagerWayland&)=delete;
    GammaManagerWayland&operator=(const GammaManagerWayland&)=delete;
public:
    ~GammaManagerWayland();
    static GammaManagerWayland *GammaManagerWaylandNew();

    virtual bool Start();
    virtual void Stop();
public Q_SLOTS:

    /**
     * @brief doQtSettingsChanged
     * @param setKey
     */
    void doQtSettingsChanged(QString setKey);

    /**
     * @brief doColorSettingsChanged
     * @param setKey
     */
    void doColorSettingsChanged(QString setKey);
private:

    /**
     * @brief syncColorSetToKwin
     */
    void syncColorSetToKwin();


    /**
     * @brief isFracDayBetween
     * @param value
     * @param start
     * @param end
     * @return
     */
    bool isFracDayBetween (double  value, double start, double end);


    /**
     * @brief inNightMode
     * @return
     */
    bool inNightMode();
    /**
     * @brief isDarkMode
     * @param key
     * @return
     */
    bool isDarkMode(QString key);
private:
    QGSettings *m_pColorSettings;
    QGSettings *m_pQtSettings;
    QGSettings *m_pGtkSettings;
    UkuiGtkConfig *m_pukuiGtkConfig;

    bool m_darkModeChangedBySelf;
    static GammaManagerWayland *m_gammaWaylandManager;

};

#endif // GAMMAMANAGERWAYLAND_H
