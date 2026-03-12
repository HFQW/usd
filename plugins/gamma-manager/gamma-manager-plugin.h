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

#ifndef DEMOPLUGIN_H
#define DEMOPLUGIN_H
#include <QtCore/QtGlobal>
#include "gamma-manager.h"
#include "gamma-manager-wayland.h"
#include "plugin-interface.h"

class  GammaManagerPlugin: public PluginInterface
{
public:
    ~GammaManagerPlugin();
    static PluginInterface *getInstance();
    virtual void activate();
    virtual void deactivate();

private:
    GammaManagerPlugin();
    GammaManagerPlugin(GammaManagerPlugin&)=delete;

    static ManagerInterface *m_pManager;
//    static GammaManagerWayland *m_gammaManagerWayland;
    static PluginInterface *m_Instance;
};
extern "C" Q_DECL_EXPORT PluginInterface* createSettingsPlugin();
#endif // DEMOPLUGIN_H
