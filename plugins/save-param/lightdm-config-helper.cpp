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

#include <QRegularExpression>
#include "lightdm-config-helper.h"
#include "QGSettings/qgsettings.h"

LightDmConfigHelper::LightDmConfigHelper(QObject *parent)
{
    m_proc = new QProcess(this);
    connect(m_proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(doProcessResult(int,QProcess::ExitStatus)));
}

LightDmConfigHelper::~LightDmConfigHelper()
{

}

void LightDmConfigHelper::syncSettingsToLightDmDir()
{
    syncXsettingsToLightDm();
    syncMouseToLightDm();

    Q_EMIT applyFinish();
}

void LightDmConfigHelper::setConfigToXorg(OutputsConfig &outputsConfig, QString userName)
{
    bool ret;
    int  icursorSize;
    double scaleFactor = 0;

    QString cursorTheme = "";

    scaleFactor = UsdBaseClass::readUserConfigToLightDM("xsettings","scaling-factor",userName).toString().toDouble();
    icursorSize = UsdBaseClass::readUserConfigToLightDM("xsettings","cursor-size",userName).toString().toInt();
    cursorTheme = UsdBaseClass::readUserConfigToLightDM("xsettings","cursor-theme",userName).toString();

    icursorSize = icursorSize == 0 ? 24: icursorSize;//cursor size default value are 24
    cursorTheme = cursorTheme == "" ? "dark-sense":cursorTheme;// default cursor theme dark-sense

    USD_LOG(LOG_DEBUG,"icursorSize:%d cursorTheme:%s", icursorSize, cursorTheme.toLatin1().data());

    if (scaleFactor==0) {
        scaleFactor = getDefaultDpiScalingFactor(outputsConfig);
        UsdBaseClass::writeUserConfigToLightDM("xsettings","scaling-factor", scaleFactor, userName);
    }

    QString xParam = QString("Xft.dpi:\t%1\nXcursor.size:\t%2\nXcursor.theme:\t%3\n")
            .arg(scaleFactor*96)
            .arg(scaleFactor*icursorSize)
            .arg(cursorTheme);

    Display    *dpy;//why must reopen display
    dpy = XOpenDisplay(NULL);
    ret = XChangeProperty(dpy, RootWindow(dpy, 0), XA_RESOURCE_MANAGER, XA_STRING, 8,
                          PropModeReplace, (unsigned char *) xParam.toLatin1().data(), xParam.length());
    XCloseDisplay(dpy);
    USD_LOG(LOG_DEBUG,"ready set %s =%d",xParam.toLatin1().data(),ret);
}

double LightDmConfigHelper::getDefaultDpiScalingFactor(OutputsConfig &outputsConfig)
{
    double defaultScale = 999.0;

    for (int k = 0; k < outputsConfig.m_outputList.count(); k++) {
        double calScale = 0;
        int width = outputsConfig.m_outputList[k]->property(OUTPUT_WIDTH).toInt();
        int height = outputsConfig.m_outputList[k]->property(OUTPUT_HEIGHT).toInt();

        if (width <= 1920 * 1.1 || height <= 1200 * 1.1) {
            calScale = 1;
        } else if (width <= 2560*1.1 || height <=1500*1.1) {//Compatible with 16:10 resolution
            calScale = 1.5;
        } else {
            calScale = 2;
        }

        defaultScale = defaultScale > calScale ?  calScale : defaultScale;
    }

    return defaultScale == 999.0 ? 1 : defaultScale;
}

void LightDmConfigHelper::doProcessResult(int index, QProcess::ExitStatus exitstatus)
{
    QVariant value;
    if (index == 0) {

        if (m_processKey == SCALING_FACTOR_KEY) {
            value = QString(m_proc->readAllStandardOutput().data()).toDouble();
        } else if (m_processKey == CURSOR_SIZE_KEY) {
            value = QString(m_proc->readAllStandardOutput().data()).toInt();
        }else {
            QRegularExpression re("(?<=').+(?=\')");
            QRegularExpressionMatch reMatch;
            QString settingsValue(m_proc->readAllStandardOutput().data());
            reMatch = re.match(settingsValue);
            USD_LOG(LOG_DEBUG,"settingsValue :%s",reMatch.captured(0).toLatin1().data());
            value = reMatch.captured(0);
        }
        UsdBaseClass::writeUserConfigToLightDM(m_processModule, m_processKey, value, m_userName);
    } else {
        SYS_LOG(LOG_DEBUG,"index:%d,exitstatus:%d restlue:%s",index, exitstatus, m_proc->readAllStandardOutput().data());
    }
}

void LightDmConfigHelper::syncXsettingsToLightDm()
{
    readGsettingsInRoot("xsettings", m_userName, GSETTINGS_XSETTINGS_SCHEMA, SCALING_FACTOR_KEY);
}

void LightDmConfigHelper::syncMouseToLightDm()
{
    readGsettingsInRoot("xsettings", m_userName, GSETTINGS_MOUSE_SCHEMA, CURSOR_THEME_KEY);
    readGsettingsInRoot("xsettings", m_userName, GSETTINGS_MOUSE_SCHEMA, CURSOR_SIZE_KEY);
}

QString LightDmConfigHelper::readGsettingsInRoot(QString module,QString userName, QString schema, QString key)
{
     QString cmd = QString("sudo -u %3 gsettings get %1 %2").arg(schema).arg(key).arg(userName);
     SYS_LOG(LOG_DEBUG,"cmd:%s",cmd.toLatin1().data());
     m_processSchema = schema;
     m_processKey = key;
     m_processModule = module;
     m_proc->start(cmd);
     m_proc->waitForFinished(1000);
     return cmd;
}
