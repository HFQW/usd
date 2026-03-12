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

#ifndef LIGHTDMCONFIGHELPER_H
#define LIGHTDMCONFIGHELPER_H
#include <QProcess>
#include "xrandroutput.h"
#include "xrandr-config.h"
#include "usd_base_class.h"
#include "usd_global_define.h"



extern "C"{
//#include <glib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
}
#include <QObject>

class LightDmConfigHelper:public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString m_userName READ getName WRITE setName)
public:
    LightDmConfigHelper(QObject *parent = nullptr);
    ~LightDmConfigHelper();

    void syncSettingsToLightDmDir();
    void setConfigToXorg(OutputsConfig &outputsConfig, QString userName);

    QString getName(){
        return m_userName;
    }

    void setName(QString userName) {
        m_userName = userName;
    }
    double getDefaultDpiScalingFactor(OutputsConfig &outputsConfig);

public Q_SLOTS:
    void doProcessResult(int index, QProcess::ExitStatus exitstatus);
Q_SIGNALS:
    void applyFinish();
private:
    void syncXsettingsToLightDm();
    void syncMouseToLightDm();
    QString readGsettingsInRoot(QString module, QString userName,QString schema, QString key);
    QString m_userName;
    QProcess *m_proc;

    QString m_ProcRet;
    QString m_processModule;
    QString m_processSchema;
    QString m_processKey;
};

#endif // LIGHTDMCONFIGHELPER_H
