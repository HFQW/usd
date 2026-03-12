/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2019 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "main.h"

#include "save-screen.h"
#include "clib-syslog.h"
#include "lightdm-config-helper.h"
#include "calibrate-touch-device.h"


#define XSETTINGS_SCHEMA    "org.ukui.SettingsDaemon.plugins.xsettings"
#define MOUSE_SCHEMA        "org.ukui.peripherals-mouse"
#define SCALING_KEY         "scaling-factor"
#define CURSOR_SIZE         "cursor-size"
#define CURSOR_THEME        "cursor-theme"

bool g_isGet = false;
bool g_isSet = false;


int QAPP(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("save-param");
    QApplication::setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("main", "Qt"));
    parser.addHelpOption();  // 添加帮助选项 （"-h" 或 "--help"）
    parser.addVersionOption();  // 添加版本选项 ("-v" 或 "--version")
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    QCommandLineOption setOption(QStringList() << "s" << "save",
                                       QApplication::translate("main", "Get the gsettings parameters of the current user and save them to the lightdm user directory"),
                                       QApplication::translate("main", "save"), "");
    parser.addOption(setOption);
    QCommandLineOption getOption(QStringList() << "g" << "get",
                                       QApplication::translate("main", "Get the display parameters of the current user and save them to the user directory of lightdm"));

    parser.addOption(getOption);

    QCommandLineOption userOption(QStringList() << "u" << "user",
                                       QApplication::translate("main", "Get the display parameters saved by the user in the lightdm personal folder and set them"),
                                       QApplication::translate("main", "user"), "");

    parser.addOption(userOption);

    QCommandLineOption cloneOption(QStringList() << "c" << "clone",
                                       QApplication::translate("main", "Set the screen to clone mode"));

    parser.addOption(cloneOption);

    parser.setApplicationDescription(QApplication::translate("main", "Qt"));  // 设置应用程序描述信息


    QCommandLineOption showOption(QStringList() << "p" << "print",
                                       QApplication::translate("main", "show the display parameters saved by the user in the lightdm personal folder and set them"),
                                       QApplication::translate("main", "user"), "");

    QCommandLineOption queryOption(QStringList() << "q" << "query",
                                       QApplication::translate("main", "query gamma and so on."));



    parser.addOption(queryOption);


    parser.addOption(showOption);
    parser.process(app);

    SaveScreenParam saveParam;
    LightDmConfigHelper configHelper;
    CalibrateTouchDevice calibrateDevice;


    QObject::connect(&calibrateDevice, &CalibrateTouchDevice::calibrateFinished, [=](){
            SYS_LOG(LOG_DEBUG,"get calibrateFinished signal.....");
            exit(0);
    });

    QObject::connect(&saveParam, &SaveScreenParam::getConfigFinished, [=](){
            USD_LOG(LOG_DEBUG,"get getConfigFinished signal.....");
            exit(0);
    });

    if(parser.isSet(setOption)) {
        QString user = parser.value(setOption);
        saveParam.setUserName(user);
        SYS_LOG(LOG_DEBUG,"ready sync user:%s gsettings", user.toLatin1().data());
        configHelper.setName(user);
        configHelper.syncSettingsToLightDmDir();
    } else if (parser.isSet(getOption)) {

        USD_LOG(LOG_DEBUG,"00");
        saveParam.setIsGet(true);
        saveParam.getConfig();
    } else if (parser.isSet(userOption)){

        QString user = parser.value(userOption);
        saveParam.setUserName(user);
        saveParam.setUserConfigParam();
        OutputsConfig m_kscreenConfigParam = saveParam.getOutputConfig();
        configHelper.setConfigToXorg(m_kscreenConfigParam, user);
        calibrateDevice.setUserName(user);
        SYS_LOG(LOG_DEBUG,"goingd down");
        calibrateDevice.calibration();
    } else if (parser.isSet(showOption)){

        QString user = parser.value(userOption);
        saveParam.setUserName(user);
        QString print = saveParam.printUserConfigParam();
        SYS_LOG(LOG_DEBUG, "%s",print.toLatin1().data());
    } else if (parser.isSet(queryOption)){
        QString user = parser.value(queryOption);
        saveParam.query();
    }

    QTimer::singleShot(5*1000, [=](){
        USD_LOG(LOG_DEBUG,"can't get finished signal must exit..");//
        exit(0);
    });

    return app.exec();
}


int QCoreApp(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("save-param");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Qt"));
    parser.addHelpOption();  // 添加帮助选项 （"-h" 或 "--help"）
    parser.addVersionOption();  // 添加版本选项 ("-v" 或 "--version")
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    QCommandLineOption setOption(QStringList() << "s" << "save",
                                       QCoreApplication::translate("main", "Get the gsettings parameters of the current user and save them to the lightdm user directory"),
                                       QCoreApplication::translate("main", "save"), "");
    parser.addOption(setOption);

    QCommandLineOption getOption(QStringList() << "g" << "get",
                                       QCoreApplication::translate("main", "Get the display parameters of the current user and save them to the user directory of lightdm"));

    parser.addOption(getOption);

    QCommandLineOption userOption(QStringList() << "u" << "user",
                                       QCoreApplication::translate("main", "Get the display parameters saved by the user in the lightdm personal folder and set them"),
                                       QCoreApplication::translate("main", "user"), "");

    parser.addOption(userOption);

    QCommandLineOption cloneOption(QStringList() << "c" << "clone",
                                       QCoreApplication::translate("main", "Set the screen to clone mode"));

    parser.addOption(cloneOption);

    parser.setApplicationDescription(QCoreApplication::translate("main", "Qt"));  // 设置应用程序描述信息


    QCommandLineOption showOption(QStringList() << "p" << "print",
                                       QCoreApplication::translate("main", "show the display parameters saved by the user in the lightdm personal folder and set them"),
                                       QCoreApplication::translate("main", "user"), "");

    QCommandLineOption queryOption(QStringList() << "q" << "query",
                                       QCoreApplication::translate("main", "query gamma and so on."));



    parser.addOption(queryOption);


    parser.addOption(showOption);
    parser.process(app);

    SaveScreenParam saveParam;
    LightDmConfigHelper configHelper;

    QObject::connect(&configHelper, &LightDmConfigHelper::applyFinish, [=](){
            USD_LOG(LOG_DEBUG,"get applyFinish signal.....");
            exit(0);
    });

    if(parser.isSet(setOption)) {

        QString user = parser.value(setOption);
        saveParam.setUserName(user);
        SYS_LOG(LOG_DEBUG,"ready sync user:%s gsettings", user.toLatin1().data());
        configHelper.setName(user);
        configHelper.syncSettingsToLightDmDir();
    } else if (parser.isSet(getOption)) {

        saveParam.setIsGet(true);
        saveParam.getConfig();
    } else if (parser.isSet(userOption)){

        QString user = parser.value(userOption);
        saveParam.setUserName(user);
        saveParam.setUserConfigParam();
        OutputsConfig m_kscreenConfigParam = saveParam.getOutputConfig();
        configHelper.setConfigToXorg(m_kscreenConfigParam, user);
    } else if (parser.isSet(showOption)){

        QString user = parser.value(userOption);
        saveParam.setUserName(user);
        QString print = saveParam.printUserConfigParam();
    } else if (parser.isSet(queryOption)){

        QString user = parser.value(queryOption);
        saveParam.query();
    }

    QTimer::singleShot(5*1000, [=](){
        USD_LOG(LOG_DEBUG,"can't get finished signal must exit..");//
        exit(0);
    });

    return app.exec();
}

int main(int argc, char *argv[])
{
    bool isSetQGSettings = false;
    for (int var = 0; var < argc; ++var) {
        if (!strcmp(argv[var],"-s")){
            isSetQGSettings = true;
        }
    }

    if (isSetQGSettings) {
        USD_LOG(LOG_DEBUG, "use core");
        QCoreApp(argc, argv);//QCore无法获取显示器参数
    } else {
        QAPP(argc, argv);
    }

}
