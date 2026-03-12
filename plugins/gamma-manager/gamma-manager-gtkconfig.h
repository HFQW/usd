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

#ifndef UKUIGTKCONFIG_H
#define UKUIGTKCONFIG_H

#include <QObject>
#include "QGSettings/qgsettings.h"
class UkuiGtkConfig : public QObject
{
    Q_OBJECT
public:
    UkuiGtkConfig(QObject *parent = nullptr);
    ~UkuiGtkConfig();

    /**
     * @brief connectGsettingSignal
     */
    void connectGsettingSignal();

    /**
     * @brief addImportStatementsToGtkCssUserFile
     */
    void addImportStatementsToGtkCssUserFile();

    /**
     * @brief modifyColorsCssFile
     * @param colorsDefinitions
     */
    void modifyColorsCssFile(QString colorsDefinitions);

    /**
     * @brief converRGBToHex
     * @param color
     * @return
     */
    QString converRGBToHex(QColor color);
public Q_SLOTS:
    void doGsettingsChanged(QString key);
private:
     QGSettings *m_colorGsettings;
     QGSettings *m_gtkThemeGsettings;
};

#endif // UKUIGTKCONFIG_H
