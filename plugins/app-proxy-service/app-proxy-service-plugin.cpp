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
#include "app-proxy-service-plugin.h"
#include "clib-syslog.h"
#include <QDebug>

ProxyServiceManager* AppProxyServicePlugin::s_proxyServiceManager = nullptr;

AppProxyServicePlugin::AppProxyServicePlugin()
{
    if(!s_proxyServiceManager)
    {
        s_proxyServiceManager = new ProxyServiceManager;
    }
}

AppProxyServicePlugin::~AppProxyServicePlugin()
{
    if(s_proxyServiceManager){
        delete s_proxyServiceManager;
        s_proxyServiceManager = nullptr;
    }
}

PluginInterface* AppProxyServicePlugin::getInstance()
{
    static AppProxyServicePlugin instance;
    return &instance;
}

void AppProxyServicePlugin::activate()
{
    qDebug()<<"AppProxyServicePlugin ---------------activate";
    USD_LOG (LOG_DEBUG, "Activating %s plugin compilation time:[%s] [%s]",MODULE_NAME,__DATE__,__TIME__);
    if(s_proxyServiceManager){
        s_proxyServiceManager->start();
    }

}

void AppProxyServicePlugin::deactivate()
{
    qDebug()<<"AppProxyServicePlugin ---------------deactivate";
    USD_LOG (LOG_DEBUG, "Activating %s plugin compilation time:[%s] [%s]",MODULE_NAME,__DATE__,__TIME__);
    if(s_proxyServiceManager){
        s_proxyServiceManager->stop();
    }

}

PluginInterface* createSettingsPlugin()
{
    return AppProxyServicePlugin::getInstance();
}
