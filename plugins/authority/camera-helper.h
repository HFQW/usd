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

#ifndef CAMERAHELPER_H
#define CAMERAHELPER_H

#include <QObject>

class CameraHelper : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","com.settings.daemon.interface")
public:
    explicit CameraHelper(QObject *parent = nullptr);

public Q_SLOTS:
    QStringList getCameraBusPathName();
    int getCameraEnable();
    void enableCamera(bool value);
    QString getIntegratedCameraBusinfo();
Q_SIGNALS:

private:
    void writeFileData(QString name, QByteArray data);
    QByteArray getFileData(const QString &filePath);
};

#endif // CAMERAHELPER_H
