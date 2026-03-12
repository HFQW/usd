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
#include "gamma-manager-plugin.h"

PluginInterface *GammaManagerPlugin::m_Instance = nullptr;
ManagerInterface *GammaManagerPlugin::m_pManager = nullptr;


GammaManagerPlugin::GammaManagerPlugin()
{
    USD_LOG(LOG_DEBUG,"initializing");
    if (UsdBaseClass::isWayland() && nullptr == m_pManager) {
        m_pManager = GammaManagerWayland::GammaManagerWaylandNew();
    } else if(nullptr == m_pManager) {
        m_pManager = GammaManager::GammaManagerNew();
    }
}

GammaManagerPlugin::~GammaManagerPlugin()
{
    if (m_pManager != nullptr) {
        delete m_pManager;
    }
}

void GammaManagerPlugin::activate()
{
    m_pManager->Start();
}

void GammaManagerPlugin::deactivate()
{
    m_pManager->Stop();
}

PluginInterface *GammaManagerPlugin::getInstance()
{
    if (nullptr == m_Instance){
        m_Instance = new GammaManagerPlugin();
    }
    return m_Instance;
}


PluginInterface *createSettingsPlugin()
{
    return GammaManagerPlugin::getInstance();
}
