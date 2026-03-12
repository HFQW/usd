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

#include "camera-helper.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDebug>

extern "C"
{
#include <clib-syslog.h>
}

static const QString usbDevicesPath = QStringLiteral("/sys/bus/usb/devices/");
static const QString configValue = QStringLiteral("/sys/bus/usb/devices/%1/bConfigurationValue");

#define DEVICE_CLASS "/bDeviceClass"        // 固定值 0xef 239
#define DEVICE_PROTOCOL "/bDeviceProtocol"  // 0x01
#define DEVICE_SUB_CLASS "/bDeviceSubClass" // 0x02

inline int byteToInt(QByteArray data)
{
    return QString(data.data()).toInt(nullptr, 16);
}

CameraHelper::CameraHelper(QObject *parent) : QObject(parent)
{

}

QByteArray CameraHelper::getFileData(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    QByteArray data = file.readAll();
    file.close();
    return data;
}

void CameraHelper::writeFileData(QString name, QByteArray data)
{
    QString path = configValue.arg(name);
    QFile file(path.arg(name));
    if (!file.open(QIODevice::WriteOnly)) {
        SYS_LOG(LOG_WARNING,"write camera bConfigurationValue faild .");
        return;
    }
    qDebug() << "file path:" << path << "value:" << data;
    file.write(data);
    file.flush();
    file.close();
}

QStringList CameraHelper::getCameraBusPathName()
{
    QStringList busPathList;
    QDir dir(usbDevicesPath);
    if (!dir.exists()){
        return QStringList();
    }
    dir.setFilter(QDir::Dirs);
    dir.setSorting(QDir::Name);
    QFileInfoList devicesInfoList = dir.entryInfoList();

    for(QFileInfo deviceInfo : devicesInfoList){

        if (deviceInfo.fileName() == "." || deviceInfo.fileName() == "..") {
            continue;
        } else if (deviceInfo.fileName().contains(":")) {
            continue;
        } else if (deviceInfo.fileName().startsWith("usb")) {
            continue;
        }
        QByteArray deviceClass = getFileData(deviceInfo.absoluteFilePath() + QStringLiteral(DEVICE_CLASS));
        SYS_LOG(LOG_DEBUG,"device : %s bDeviceClass is %d",deviceInfo.fileName().toLatin1().data(),byteToInt(deviceClass));

        if (byteToInt(deviceClass) != 239) {
            continue;
        }
        QByteArray protocol = getFileData(deviceInfo.absoluteFilePath() + QStringLiteral(DEVICE_PROTOCOL));
        if (byteToInt(protocol) != 1) {
            continue;
        }
        QByteArray subClass = getFileData(deviceInfo.absoluteFilePath() + QStringLiteral(DEVICE_SUB_CLASS));
        if (byteToInt(subClass) != 2) {
            continue;
        }
        busPathList << deviceInfo.fileName();
    }
    return busPathList;
}

int CameraHelper::getCameraEnable()
{
    QStringList nameList = getCameraBusPathName();
    if (nameList.isEmpty()) {
        SYS_LOG(LOG_WARNING, "get camera name list faild .");
        return -1;
    }
    QString path = configValue.arg(nameList.at(0));
    QByteArray data = getFileData(path);
    return QString::fromLatin1(data.data()).toInt();
}

void CameraHelper::enableCamera(bool value)
{
    const QStringList nameList = getCameraBusPathName();

    if (nameList.isEmpty()) {
        SYS_LOG(LOG_WARNING, "get camera name list is faild .");
        return;
    }

    for (const QString& name : nameList) {
        writeFileData(name, value ? "1" : "0");
    }
}


QString CameraHelper::getIntegratedCameraBusinfo()
{
    QString path = QString("/sys/bus/usb/devices/");
    QDir dir(path);
    if (!dir.exists()) {
        return "";
    }
    dir.setFilter(QDir::Dirs);
    dir.setSorting(QDir::Name);
    QFileInfoList fileinfoList = dir.entryInfoList();

    QStringList busInfoList;
    for(QFileInfo fileinfo : fileinfoList){
        if (fileinfo.fileName() == "." || fileinfo.fileName() == ".."){
            continue;
        }

        if (fileinfo.fileName().contains(":")){
            continue;
        }

        if (fileinfo.fileName().startsWith("usb")){
            continue;
        }

        QDir subdir(fileinfo.absoluteFilePath());
        subdir.setFilter(QDir::Files);
        QFileInfoList fileinfoList2 = subdir.entryInfoList();
        for (QFileInfo fileinfo2 : fileinfoList2){
            if (fileinfo2.fileName() == "product"){
                QFile pfile(fileinfo2.absoluteFilePath());
                if (!pfile.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;
                QTextStream pstream(&pfile);
                QString output = pstream.readAll();
                if (output.contains("integrated camera", Qt::CaseInsensitive)){
                    return fileinfo.fileName();
                }
                if (output.contains("icspring camera", Qt::CaseInsensitive)){
                    return fileinfo.fileName();
                }
            }
        }
    }
    return "";
}