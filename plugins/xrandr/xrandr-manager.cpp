/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2012 by Alejandro Fiestas Olivares <afiestas@kde.org>
 * Copyright 2016 by Sebastian Kügler <sebas@kde.org>
 * Copyright (c) 2018 Kai Uwe Broulik <kde@broulik.de>
 *                    Work sponsored by the LiMux project of
 *                    the city of Munich.
 * Copyright (C) 2020 KylinSoft Co., Ltd.
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
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>
#include <QX11Info>
#include <QtXml>
#include <QtConcurrent/QtConcurrent>

#include "xrandr-manager.h"
#include "touch-calibrate.h"
#include <QOrientationReading>
#include <memory>
#include <QDBusMessage>

extern "C"{
#include <X11/extensions/Xrandr.h>
#include "clib-syslog.h"
}

#define SETTINGS_XRANDR_SCHEMAS     "org.ukui.SettingsDaemon.plugins.xrandr"
#define XRANDR_ROTATION_KEY         "xrandr-rotations"
#define XRANDR_PRI_KEY              "primary"
#define XSETTINGS_SCHEMA            "org.ukui.SettingsDaemon.plugins.xsettings"
#define XSETTINGS_KEY_SCALING       "scaling-factor"

XrandrManager::XrandrManager():
    m_outputsChangedSignal(eScreenSignal::isNone),
    m_acitveTimer(new QTimer(this)),
    m_outputsInitTimer(new QTimer(this)),
    m_offUsbScreenTimer(new QTimer(this)),
    m_onUsbScreenTimer(new QTimer(this))
{
    KScreen::Log::instance();
    m_xrandrDbus = new xrandrDbus(this);
    m_xrandrSettings = new QGSettings(SETTINGS_XRANDR_SCHEMAS);

    new XrandrAdaptor(m_xrandrDbus);
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.registerService(DBUS_XRANDR_NAME)) {
        sessionBus.registerObject(DBUS_XRANDR_PATH,
                                  m_xrandrDbus,
                                  QDBusConnection::ExportAllContents);
    } else {
        USD_LOG(LOG_ERR, "register dbus error");
    }

    m_ukccDbus = new QDBusInterface("org.ukui.ukcc.session",
                                   "/",
                                   "org.ukui.ukcc.session.interface",
                                   QDBusConnection::sessionBus());

    m_outputModeEnum = QMetaEnum::fromType<UsdBaseClass::eScreenMode>();

    m_statusManagerDbus = new QDBusInterface(DBUS_STATUSMANAGER_NAME,
                                             DBUS_STATUSMANAGER_PATH,
                                             DBUS_STATUSMANAGER_INTERFACE,
                                             QDBusConnection::sessionBus(),
                                             this);

    if (m_statusManagerDbus->isValid()) {
        connect(m_statusManagerDbus, SIGNAL(mode_change_signal(bool)),this,SLOT(doTabletModeChanged(bool)));
        connect(m_statusManagerDbus, SIGNAL(rotations_change_signal(QString)),this,SLOT(doRotationChanged(QString)));
    } else {
        USD_LOG(LOG_ERR, "m_statusManagerDbus error !!!!!!");
    }

    m_onUsbScreenTimer->setSingleShot(true);
    m_offUsbScreenTimer->setSingleShot(true);

    connect(m_offUsbScreenTimer, &QTimer::timeout, this, [=](){
        std::unique_ptr<xrandrConfig> MonitoredConfig = m_outputsConfig->readFile(false);

        if (MonitoredConfig == nullptr ) {
            USD_LOG(LOG_DEBUG,"config a error");
            setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode));
            return;
        }

        m_outputsConfig = std::move(MonitoredConfig);
        applyConfig();
    });

    connect(m_onUsbScreenTimer, &QTimer::timeout, this, [=](){
         setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::extendScreenMode));
    });

    connect(m_xrandrDbus,&xrandrDbus::controlScreen,this,&XrandrManager::doCalibrate);

    bool ret = QDBusConnection::sessionBus().connect(QString("org.kde.KScreen"),
                                                     QString("/backend"),
                                                     QString("org.kde.kscreen.Backend"),
                                                     QString("configChanged"),
                                                     this,
                                                     SLOT(configChangedSlot(QVariantMap)));
}

//获取
void XrandrManager::configChangedSlot(QVariantMap config)
{
    if(isApplied) {
        isApplied = false;
        sendOutputsModeToDbus();
        calibrateTouchDevice();
    }
    else {
	isApplied = false;
    }
}

void XrandrManager::getInitialConfig()
{
    if (!m_isInitFinish) {
        USD_LOG(LOG_DEBUG,"start 1500 timer...");
        m_outputsInitTimer->start(1500);
    }

    connect(new KScreen::GetConfigOperation, &KScreen::GetConfigOperation::finished,
            this, [this](KScreen::ConfigOperation* op) {
        USD_LOG(LOG_DEBUG,"stop 1500 timer...");
        m_outputsInitTimer->stop();
        if (op->hasError()) {
            USD_LOG(LOG_DEBUG,"Error getting initial configuration：%s",op->errorString().toLatin1().data());
            return;
        }

        if (m_outputsConfig) {
            if (m_outputsConfig->data()) {
                KScreen::ConfigMonitor::instance()->removeConfig(m_outputsConfig->data());
                for (const KScreen::OutputPtr &output : m_outputsConfig->data()->outputs()) {
                    output->disconnect(this);
                }
                m_outputsConfig->data()->disconnect(this);
            }
            m_outputsConfig = nullptr;
        }

        m_outputsConfig = std::unique_ptr<xrandrConfig>(new xrandrConfig(qobject_cast<KScreen::GetConfigOperation*>(op)->config()));
        m_outputsConfig->setValidityFlags(KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen);
        m_isInitFinish = true;
        if (initAllOutputs()>1) {
            m_xrandrDbus->mScreenMode = discernScreenMode();
            m_outputsConfig->setScreenMode(m_outputModeEnum.valueToKey(m_xrandrDbus->mScreenMode));
        }

    });
}

XrandrManager::~XrandrManager()
{
    if (m_acitveTimer) {
        delete m_acitveTimer;
        m_acitveTimer = nullptr;
    }
    if (m_xrandrSettings) {
        delete m_xrandrSettings;
        m_xrandrSettings = nullptr;
    }
}

bool XrandrManager::start()
{
    USD_LOG(LOG_DEBUG,"Xrandr Manager Start");
    connect(m_acitveTimer, &QTimer::timeout, this,&XrandrManager::active);
    m_acitveTimer->start(0);
    return true;
}

void XrandrManager::stop()
{
     USD_LOG(LOG_DEBUG,"Xrandr Manager Stop");
}

/*
 * 通过ouput的pnpId和monitors.xml中的ventor以及接口类型（VGA,HDMI）进行匹配
 *
*/
bool XrandrManager::readMateToKscreen(char monitorsCount,QMap<QString, QString> &monitorsName)
{
    bool ret = false;
    int xmlErrColumn = 0;
    int xmlErrLine = 0;
    int xmlOutputCount = 0;//xml单个配置组合的屏幕数目
    int matchCount = 0;//硬件接口与ventor匹配的数目。

    QDomNode n;
    QDomElement root;
    QDomDocument doc;

    QString xmlErrMsg;
    QString homePath = getenv("HOME");
    QString monitorFile = homePath+"/.config/monitors.xml";

    OutputsConfig monitorsConfig;

    QFile file(monitorFile);

    if (monitorsCount < 1) {
        USD_LOG(LOG_DEBUG, "skip this function...");
        return false;
    }

    USD_LOG_SHOW_PARAM1(monitorsCount);
    if(!file.open(QIODevice::ReadOnly)) {
        USD_LOG(LOG_ERR,"%s can't be read...",monitorFile.toLatin1().data());
        return false;
    }

    if(!doc.setContent(&file,&xmlErrMsg,&xmlErrLine, &xmlErrColumn)) {
        USD_LOG(LOG_DEBUG,"read %s to doc failed errmsg:%s at %d.%d",monitorFile.toLatin1().data(),xmlErrMsg.toLatin1().data(),xmlErrLine,xmlErrColumn);
        file.close();
        return false;
    }

    m_mateFileTag.clear();
    m_IntDate.clear();

    root=doc.documentElement();
    n=root.firstChild();

    while(!n.isNull()) {
        matchCount = 0;
        xmlOutputCount = 0;

        if (n.isElement()) {
            QDomElement e =n.toElement();
            QDomNodeList list=e.childNodes();

            if (e.isElement() == false) {
               goto NEXT_NODE;//goto next config
            }

            /*a configuration have 4 outputs*/
            for (int i=0;i<list.count();i++) {
                UsdOuputProperty *mateOutput;
                QDomNode node=list.at(i);
                QDomNodeList e2 = node.childNodes();

                if (node.isElement()) {

                    if (node.toElement().tagName() == "clone") {
                        monitorsConfig.m_clone = node.toElement().text();
                        continue;
                    }
                    if (node.toElement().tagName() != "output") {
                        continue;
                    }
                    if (node.toElement().text().isEmpty()) {
                        continue;
                    }
                    if (false == monitorsName.keys().contains(node.toElement().attribute("name"))) {
                        USD_LOG_SHOW_PARAMS(node.toElement().attribute("name").toLatin1().data());
                        continue;
                    }
                    mateOutput = new UsdOuputProperty();
                    mateOutput->setProperty("name", node.toElement().attribute("name"));
                    for (int j=0;j<e2.count();j++) {
                        QDomNode node2 = e2.at(j);
                        mateOutput->setProperty(node2.toElement().tagName().toLatin1().data(),node2.toElement().text());
                    }
                    //多个屏幕组合，需要考虑包含的情况
                    if (monitorsName[mateOutput->property("name").toString()] == mateOutput->property("vendor").toString()) {
                        matchCount++;
                    }
                }
                xmlOutputCount++;
                monitorsConfig.m_outputList.append(mateOutput);
            }

            //monitors.xml内的其中一个configuration屏幕数目与识别数目一致，并且与接口识别出的vendor数目三者一致时才可进行设置。
            if (matchCount != monitorsCount || xmlOutputCount != matchCount) {
                if (monitorsConfig.m_outputList.count()>0) {
                    qDeleteAll(monitorsConfig.m_outputList);
                    monitorsConfig.m_outputList.clear();
                }
               goto NEXT_NODE;//goto next config
            }

            if (monitorsConfig.m_clone == "yes") {
                setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode));
                ret = true;
                goto FINISH;
            }

            for (const KScreen::OutputPtr &output: m_configPtr->outputs()) {
                if (false == output->isConnected()) {
                    continue;
                }

                for (int k = 0; k < monitorsConfig.m_outputList.count(); k++) {
                    if (output->name() != monitorsConfig.m_outputList[k]->property("name").toString()) {
                        continue;
                    }

                    bool modeSetOk = false;
                    int x;
                    int y;
                    int width;
                    int height;
                    int rate;
                    int rotationInt;

                    QString primary;
                    QString rotation;

                    width = getMateConfigParam(monitorsConfig.m_outputList[k], "width");
                    if (width < 0) {
                        return false;
                    }

                    height = getMateConfigParam(monitorsConfig.m_outputList[k], "height");
                    if (height < 0) {
                        return false;
                    }

                    x = getMateConfigParam(monitorsConfig.m_outputList[k], "x");
                    if (x < 0) {
                        return false;
                    }

                    y = getMateConfigParam(monitorsConfig.m_outputList[k], "y");
                    if (y < 0) {
                        return false;
                    }

                    rate = getMateConfigParam(monitorsConfig.m_outputList[k], "rate");
                    if (y < 0) {
                        return false;
                    }

                    primary = monitorsConfig.m_outputList[k]->property("primary").toString();
                    rotation = monitorsConfig.m_outputList[k]->property("rotation").toString();

                    if (primary == "yes") {
                        output->setPrimary(true);
                    }
                    else {
                        output->setPrimary(false);
                    }

                    if (rotation == "left") {
                        rotationInt = 2;
                    } else if (rotation == "upside_down") {
                        rotationInt = 4;
                    } else if  (rotation == "right") {
                        rotationInt = 8;
                    } else {
                        rotationInt = 1;
                    }

                    output->setRotation(static_cast<KScreen::Output::Rotation>(rotationInt));

                    Q_FOREACH(auto mode, output->modes()) {
                        if(mode->size().width() != width && mode->size().height() != height) {
                            continue;
                        }
                        if (fabs(mode->refreshRate() - rate) > 2) {
                            continue;
                        }
                        output->setCurrentModeId(mode->id());
                        output->setPos(QPoint(x,y));
                        modeSetOk = true;
                        break;
                    }

                    if (modeSetOk == false) {
                        ret = false;
                        goto FINISH;
                    }
                }
            }
            applyConfig();
            ret = true;
            goto FINISH;
        }
NEXT_NODE:
        n = n.nextSibling();
        qDeleteAll(monitorsConfig.m_outputList);
        monitorsConfig.m_outputList.clear();
    }

FINISH:
    qDeleteAll(monitorsConfig.m_outputList);
    monitorsConfig.m_outputList.clear();
    return ret;
}

bool XrandrManager::getInitialState()
{
    return m_isInitFinish;
}

int XrandrManager::getMateConfigParam(UsdOuputProperty *mateOutput, QString param)
{
    bool isOk;
    int ret;

    ret = mateOutput->property(param.toLatin1().data()).toInt(&isOk);

    if (false == isOk) {
        return -1;
    }
    return ret;
}

/*监听旋转键值回调 并设置旋转角度*/
void XrandrManager::doRotationChanged(const QString &rotation)
{
    int value = 0;
    QString angle_Value = rotation;

    if (angle_Value == "normal") {
        value = 1;
    } else if (angle_Value == "left") {
        value = 2;
    } else if (angle_Value == "upside-down") {
        value = 4;
    } else if  (angle_Value == "right") {
        value = 8;
    } else {
        USD_LOG(LOG_ERR,"Find a error !!!");
        return ;
    }

    const KScreen::OutputList outputs = m_outputsConfig->data()->outputs();
    for(auto output : outputs){
        if (!output->isConnected() || !output->isEnabled() || !output->currentMode()) {
            continue;
        }
        output->setRotation(static_cast<KScreen::Output::Rotation>(value));
        USD_LOG(LOG_DEBUG,"set %s rotaion:%s", output->name().toLatin1().data(), rotation.toLatin1().data());
    }
    applyConfig();
}


void XrandrManager::doOutputsConfigurationChanged()
{
    if (UsdBaseClass::isWayland()) {
        connect(new KScreen::GetConfigOperation, &KScreen::GetConfigOperation::finished,
                this, [this](KScreen::ConfigOperation* op) {
            m_outputsConfig = std::unique_ptr<xrandrConfig>(new xrandrConfig(qobject_cast<KScreen::GetConfigOperation*>(op)->config()));
            m_outputsConfig->setValidityFlags(KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen);
            Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                USD_LOG_SHOW_OUTPUT(output);
            }
        });
        return;
    }

    USD_LOG(LOG_DEBUG,"...");
}

void XrandrManager::calibrateTouchDevice()
{
    if (!m_isInitFinish || UsdBaseClass::isWayland()) {
        return;
    }
    static KScreen::ConfigPtr oldOutputs = nullptr;
    bool needSetMapTouchDevice = false;

    if (oldOutputs != nullptr) {
        Q_FOREACH(const KScreen::OutputPtr &oldOutput, oldOutputs->outputs()) {
            Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                if(output->isConnected() && oldOutput->name() == output->name()) {
                    if(oldOutput->currentModeId() != output->currentModeId() || oldOutput->pos() != output->pos()
                            || oldOutput->scale() != output->scale() || oldOutput->rotation() != output->rotation() ||
                            oldOutput->isPrimary() != output->isPrimary() || oldOutput->isEnabled() != output->isEnabled() ||
                            oldOutput->isConnected() != output->isConnected()) {
                        needSetMapTouchDevice = true;
			break;
                    }
		    if(output->rotation() != static_cast<KScreen::Output::Rotation>(1)) {
		        needSetMapTouchDevice = true;
			USD_LOG(LOG_DEBUG,"rotated，net set map touch:%d",needSetMapTouchDevice);
			break;
		    }
		    if(output->pos() != QPoint(0,0)) {
		        needSetMapTouchDevice = true;
			USD_LOG(LOG_DEBUG,"not original posi，net set map touch:%d",needSetMapTouchDevice);
			break;
		    }

                    if(oldOutput->currentMode().isNull() && output->currentMode().isNull()) {
                        continue;
                    }
                    if(oldOutput->currentMode().isNull() || output->currentMode().isNull()) {
                        needSetMapTouchDevice = true;
                        break;
                    } else {
                        if(oldOutput->currentMode()->size() != output->currentMode()->size()) {
                            needSetMapTouchDevice = true;
			    break;
                        }
                    }
                }
                if(oldOutput->currentMode().isNull() && output->currentMode().isNull()) {
                    break;
                }
                if(oldOutput->currentMode().isNull() || output->currentMode().isNull()) {
                    needSetMapTouchDevice = true;
                    break;
                } else {
                    if(oldOutput->currentMode()->size() != output->currentMode()->size()) {
                        needSetMapTouchDevice = true;
			break;
                    }
                }
            }
        }
    } else {
        needSetMapTouchDevice = true;
    }
    oldOutputs = m_outputsConfig->data()->clone();
    if (needSetMapTouchDevice) {
        QString path = QDir::homePath() + "/.config/touchcfg.ini";
        TouchCalibrate *calibrate = new TouchCalibrate(path);
        calibrate->calibrate();
        calibrate->deleteLater();
    }
}

void XrandrManager::sendOutputsModeToDbus()
{
    const QStringList ukccModeList = {"first", "copy", "expand", "second"};
    int screenConnectedCount = 0;
    int screenMode = discernScreenMode();

    m_xrandrDbus->sendModeChangeSignal(screenMode);
    m_xrandrDbus->sendScreensParamChangeSignal(m_outputsConfig->getScreensParam());
    ///send screens mode to ukcc(ukui-control-center) by sjh 2021.11.08
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (true == output->isConnected()) {
            screenConnectedCount++;
        }
    }

    if (screenConnectedCount > 1) {
        m_ukccDbus->call("setScreenMode", ukccModeList[screenMode]);
    } else {
        m_ukccDbus->call("setScreenMode", ukccModeList[0]);
    }
}

void XrandrManager::applyConfig()
{
    if (UsdBaseClass::isWayland()) {
        connect(new KScreen::SetConfigOperation(m_outputsConfig->data()),
                &KScreen::SetConfigOperation::finished,
                this, [this]() {
            USD_LOG(LOG_ERR,"--|apply wayland success|--");
        });
        return;
    }

    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        USD_LOG_SHOW_OUTPUT(output);
    }
    if (m_outputsConfig->canBeApplied() || true) {
        m_isSetting = true;
        connect(new KScreen::SetConfigOperation(m_outputsConfig->data()),
                &KScreen::SetConfigOperation::finished,
                this, [this]() {
            QProcess subProcess;
            QString usdSaveParam = "save-param -g";

            USD_LOG(LOG_ERR,"--|apply success|--");
            calibrateTouchDevice();
            isApplied = true;

            m_outputsConfig->setScreenMode(m_outputModeEnum.valueToKey(discernScreenMode()));
            writeConfig();//

            Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
                if (output->isConnected()) {
                    USD_LOG(LOG_DEBUG,"save param in lightdm-data.");//can't recive finished signal from getConfigOperation when output connect are 0
                    subProcess.start(usdSaveParam);
                    subProcess.waitForFinished();
                    break;
                }
            }

            m_isSetting = false;
        });
    } else {
        USD_LOG(LOG_ERR,"--|can't be apply|--");
        m_isSetting = false;
    }
}

void XrandrManager::writeConfig()
{
    bool isNeedSave = true;
    uint connectOutput = 0;
    do{
        if (!UsdBaseClass::isJJW7200()) {
            USD_LOG(LOG_DEBUG,"skip jjw fake output1");
            break;
        }

        Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
            if (!output->isConnected()) {
                continue;
            }
            connectOutput++;
        }
        if (connectOutput != 1) {
            USD_LOG(LOG_DEBUG,"skip jjw fake output2:%d",connectOutput);
            break;
        }

        Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
            if (!output->isConnected()) {
                continue;
            }

//            if (output->name().contains("VGA",Qt::CaseInsensitive))
            {
                if (output->modes().count() < 3) {
                    isNeedSave = false;
                    USD_LOG(LOG_DEBUG,"skip jjw fake output!");
                }
                break;
            }
        }
    }while(0);

    if (isNeedSave) {
        QProcess subProcess;
        USD_LOG(LOG_DEBUG,"skip jjw fake output3");
        m_outputsConfig->writeFile(false);
        QString usdSaveParam = "save-param -g";
        USD_LOG(LOG_DEBUG,"save param in lightdm-data.");
        subProcess.start(usdSaveParam);
        subProcess.waitForFinished();
    }
}

//用于外置显卡，当外置显卡插拔时会有此事件产生
void XrandrManager::doOutputAdded(const KScreen::OutputPtr &output)
{
    if (UsdBaseClass::isWayland()) {
        USD_LOG(LOG_DEBUG, "is wayland..");
        return;
    }
    USD_LOG_SHOW_OUTPUT(output);
    if (!m_outputsConfig->data()->outputs().contains(output->id())) {
        m_outputsConfig->data()->addOutput(output->clone());
        connect(output.data(), &KScreen::Output::isConnectedChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }

            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
            if (senderOutput->isConnected()==false) {
                USD_LOG(LOG_DEBUG,"ready remove %d",senderOutput->id());
                if (m_outputsConfig->data()->outputs().contains(senderOutput->id())) {
                    USD_LOG(LOG_DEBUG,"remove %d",senderOutput->id());
                    m_outputsConfig->data()->removeOutput(senderOutput->id());
                }
            }
            m_offUsbScreenTimer->start(1500);
            Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
                USD_LOG_SHOW_OUTPUT(output);
            }

        },Qt::QueuedConnection);
    }
    m_xrandrDbus->sendScreenAddedSignal(output->name());
    m_onUsbScreenTimer->start(2500);
}


void XrandrManager::doOutputRemoved(int outputId)
{
     if (!m_outputsConfig->data()->outputs().contains(outputId)) {
        return;
     }

     Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
         if (output->id() == outputId) {
             USD_LOG_SHOW_OUTPUT(output);
             m_xrandrDbus->sendScreenRemovedSignal(output->name());
             break;
         }
     }

     m_outputsConfig->data()->removeOutput(outputId);
     std::unique_ptr<xrandrConfig> MonitoredConfig = m_outputsConfig->readFile(false);
     if (MonitoredConfig == nullptr ) {
         USD_LOG(LOG_DEBUG,"config a error");
         setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode));
         return;
     }
     m_outputsConfig = std::move(MonitoredConfig);
     applyConfig();
}

void XrandrManager::doPrimaryOutputChanged(const KScreen::OutputPtr &output)
{
    USD_LOG(LOG_DEBUG,".");
}


/*
 *
 *接入时没有配置文件的处理流程：
 *单屏：最优分辨率。
 *多屏幕：镜像模式。
 *
*/
void XrandrManager::outputConnectedWithoutConfigFile(KScreen::Output *newOutput, char outputCount)
{
    if (1 == outputCount) {//单屏接入时需要设置模式，主屏
        setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::firstScreenMode));
    } else {
        setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode));
    }

}

void XrandrManager::lightLastScreen()
{
    int enableCount = 0;
    int connectCount = 0;

    Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
        if (output->isConnected()){
            connectCount++;
        }
        if (output->isEnabled()){
            enableCount++;
        }
    }
    if (connectCount==1 && enableCount==0){
        Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
            if (output->isConnected()){
                output->setEnabled(true);
                break;
            }
        }
    }
}

int XrandrManager::getCurrentRotation()
{
    uint8_t ret = 1;
    QDBusMessage message = QDBusMessage::createMethodCall(DBUS_STATUSMANAGER_NAME,
                                                          DBUS_STATUSMANAGER_PATH,
                                                          DBUS_STATUSMANAGER_NAME,
                                                          DBUS_STATUSMANAGER_GET_ROTATION);

    QDBusMessage response = QDBusConnection::sessionBus().call(message);
    if (response.type() == QDBusMessage::ReplyMessage) {
        if (response.arguments().isEmpty() == false) {
            QString value = response.arguments().takeFirst().toString();
            USD_LOG(LOG_DEBUG, "get mode :%s", value.toLatin1().data());
            if (value == "normal") {
                ret = 1;
            } else if (value == "left") {
                 ret = 2;
            } else if (value == "upside-down") {
                  ret = 4;
            } else if  (value == "right") {
                   ret = 8;
            } else {
                USD_LOG(LOG_DEBUG,"Find a error !!! value%s",value.toLatin1().data());
                return ret = 1;
            }
        }
    }
    return ret;
}

/*
 *
 * -1:无接口
 * 0:PC模式
 * 1：tablet模式
 *
*/
int XrandrManager::getCurrentMode()
{
    QDBusMessage message = QDBusMessage::createMethodCall(DBUS_STATUSMANAGER_NAME,
                                                          DBUS_STATUSMANAGER_PATH,
                                                          DBUS_STATUSMANAGER_NAME,
                                                          DBUS_STATUSMANAGER_GET_MODE);

    QDBusMessage response = QDBusConnection::sessionBus().call(message);
    if (response.type() == QDBusMessage::ReplyMessage) {
        if(response.arguments().isEmpty() == false) {
            bool value = response.arguments().takeFirst().toBool();
            return value;
        }
    }
    return -1;
}

void XrandrManager::doOutputChanged(KScreen::Output *senderOutput)
{
    char outputConnectCount = 0;
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (output->name()==senderOutput->name() && output->hash() != senderOutput->hash()) {
            senderOutput->setEnabled(senderOutput->isConnected());
            m_outputsConfig->data()->removeOutput(output->id());
            m_outputsConfig->data()->addOutput(senderOutput->clone());
            USD_LOG(LOG_DEBUG,"%s hash %s change %s",output->name().toLatin1().data(), output->hash().toLatin1().data(), senderOutput->hash().toLatin1().data());
            break;
        }
    }
    Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
        if (output->name()==senderOutput->name()) {//这里只设置connect，enbale由配置设置。
            output->setEnabled(senderOutput->isConnected());
            output->setConnected(senderOutput->isConnected());
            output->setModes(senderOutput->modes());
            output->setPreferredModes(senderOutput->preferredModes());
       }
        if (output->isConnected()) {
            outputConnectCount++;
        }
    }

    if (UsdBaseClass::isTablet() && getCurrentMode()) {//平板项目并且是平板模式
        int ret = getCurrentMode();
        USD_LOG(LOG_DEBUG,"table mode need't use config file");
        if (0 < ret) {
            //tablet模式
              setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode));
        } else {
            //PC模式
              setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::extendScreenMode));
        }
    } else {//非tablet项目无此接口
        if (false == m_outputsConfig->fileExists()) {
            if (senderOutput->isConnected()) {
                senderOutput->setEnabled(senderOutput->isConnected());
            }
            outputConnectedWithoutConfigFile(senderOutput, outputConnectCount);
        } else {
            if (outputConnectCount) {
                std::unique_ptr<xrandrConfig> MonitoredConfig  = m_outputsConfig->readFile(false);
                if (MonitoredConfig!=nullptr) {
                    std::unique_ptr<xrandrConfig> MonitoredConfig  = m_outputsConfig->readFile(false);
                    m_outputsConfig = std::move(MonitoredConfig);
                     USD_LOG(LOG_DEBUG,"read config file success! ");
                    this->isDefaultStatus = false;
                    Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                        //发现插进来的显示器是关的，应当设置为开启
                        if (output->name()==senderOutput->name() && senderOutput->isConnected() && !output->isEnabled() ) {
                            this->isDefaultStatus = true;
                            setOutputsModeToExtend();
                            this->isDefaultStatus = false;
                            return;
                        }
                    }
                } else {
                    USD_LOG(LOG_DEBUG,"read config file error! ");
                    if (outputConnectCount>1) {
                        return setOutputsModeToClone(false);
                    } else if(1 == outputConnectCount){
                              return setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::firstScreenMode));
                    }
                }
            }
        }
    }

    applyConfig();
    if (UsdBaseClass::isJJW7200()) {
        QTimer::singleShot(3*1000, this, [this](){
            applyConfig();
            USD_LOG(LOG_DEBUG,"signalShot......");
        });
    }
}

void XrandrManager::doOutputModesChanged()
{
//TODO: just handle modesChanges signal for cloud desktop
/*
 * make sure the size in Kscreen config is smaller than max screen size
 * if ouputname != nullptr set this ouput mode is preffer mode,
*/
/*
 * 确保kscreen中的size小于screen的最大尺寸，
 * 如果ouputname不为空，调整output的最佳分辨率，重新进行设置，并计算匹配size，
 * 如果不符合标准则调整另一个屏幕的大小。
 * 如果output为空，则屏幕均使用最佳分辨率，如果无最佳分辨率就用最大最适合。如果第三个屏幕无法接入，则不进行处理。。
 * 目前只按照两个屏幕进行处理。
 * 有时模式改变会一次性给出两个屏幕的信号，就需要在这里同时处理多块屏幕!。
 * 第一步：找到坐标为0，0的屏幕。
 * 第二步：横排所有屏幕
*/
    int newPosX = 0;
    int findOutputCounect = 0;
    //找出pos(0.0)的屏幕大小
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (!output->isConnected() || !output->isEnabled()) {
            continue;
        }
        //不能在这里获取size大小，特殊情况下currentMode->size会报错
        if (output->pos() == QPoint(0,0)) {
            findOutputCounect++;
            if (m_modesChangeOutputs.contains(output->name()) &&
                    output->modes().contains(output->preferredModeId())) {
                output->setCurrentModeId(output->preferredModeId());
            }
            newPosX += output->currentMode()->size().width();
            break;
        }
    }

    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (!output->isConnected() || !output->isEnabled()) {
            continue;
        }
        if (output->pos() != QPoint(0,0)) {
            output->setPos(QPoint(newPosX,0));
            if (m_modesChangeOutputs.contains(output->name()) &&
                    output->modes().contains(output->preferredModeId())) {
                 output->setCurrentModeId(output->preferredModeId());
            }
            newPosX += output->currentMode()->size().width();
        }
    }
    applyConfig();
}

//处理来自控制面板的操作,保存配置
void XrandrManager::doSaveConfigTimeOut()
{
    int enableScreenCount = 0;
    m_screenSignalTimer->stop();

    if (m_outputsChangedSignal & eScreenSignal::isModesChanged && !(m_outputsChangedSignal & eScreenSignal::isConnectedChanged)) {
        USD_LOG(LOG_DEBUG,".");
        doOutputModesChanged();
        m_modesChangeOutputs.clear();
        m_outputsChangedSignal = eScreenSignal::isNone;
        return;
    }

    if (m_outputsChangedSignal&(eScreenSignal::isConnectedChanged)) {
        USD_LOG(LOG_DEBUG,"skip save config");
        m_applyConfigWhenSave = false;
        m_outputsChangedSignal = eScreenSignal::isNone;
        return;
    }

    m_outputsChangedSignal = eScreenSignal::isNone;
    if (false == m_applyConfigWhenSave) {
        Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
            if (output->isEnabled()) {
                enableScreenCount++;
            }
        }

        if (0 == enableScreenCount) {//When user disable last one connected screen USD must enable the screen.
            m_applyConfigWhenSave = true;
            m_screenSignalTimer->start(SAVE_CONFIG_TIME*5);
            return;
        }
    }

    if (m_applyConfigWhenSave) {
        m_applyConfigWhenSave = false;
        setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::firstScreenMode));
    } else {
        Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
            USD_LOG_SHOW_OUTPUT(output);
        }
        m_outputsConfig->setScreenMode(m_outputModeEnum.valueToKey(discernScreenMode()));
        writeConfig();

//        SetTouchscreenCursorRotation();//When other app chenge screen'param usd must remap touch device
        calibrateTouchDevice();
        sendOutputsModeToDbus();
    }
}

QString XrandrManager::getOutputsInfo()
{
    return m_outputsConfig->getScreensParam();
}

int XrandrManager::initAllOutputs()
{
    char connectedOutputCount = 0;
    QMap<QString, QString> outputsList;
    if (m_configPtr) {
        KScreen::ConfigMonitor::instance()->removeConfig(m_configPtr);
        for (const KScreen::OutputPtr &output : m_configPtr->outputs()) {
            output->disconnect(this);
        }
        m_configPtr->disconnect(this);
    }

    m_configPtr = std::move(m_outputsConfig->data());

    for (const KScreen::OutputPtr &output: m_configPtr->outputs()) {
        if (output->isConnected()){
            connectedOutputCount++;
            outputsList.insert(output->name(),output->edid()->pnpId());
        }
        connect(output.data(), &KScreen::Output::isConnectedChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            m_outputsChangedSignal |= eScreenSignal::isConnectedChanged;
            USD_LOG(LOG_DEBUG,"%s isConnectedChanged connect:%d",senderOutput->name().toLatin1().data(), senderOutput->isConnected());
            doOutputChanged(senderOutput);
        },Qt::QueuedConnection);

        connect(output.data(), &KScreen::Output::outputChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            m_outputsChangedSignal |= eScreenSignal::isOutputChanged;
            USD_LOG(LOG_DEBUG,"%s outputchanged connect:%d",senderOutput->name().toLatin1().data(), senderOutput->isConnected());
            m_screenSignalTimer->stop();
//            if (UsdBaseClass::isJJW7200()){
//                USD_LOG(LOG_DEBUG,"catch a jjw7200..");
//                doOutputChanged(senderOutput);
//            }
        },Qt::QueuedConnection);

        connect(output.data(), &KScreen::Output::isPrimaryChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            m_outputsChangedSignal |= eScreenSignal::isPrimaryChanged;
            USD_LOG(LOG_DEBUG,"PrimaryChanged:%s",senderOutput->name().toLatin1().data());

            Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                if (output->name() == senderOutput->name()) {
                    output->setPrimary(senderOutput->isPrimary());
                    break;
                }
            }
            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
        });

        connect(output.data(), &KScreen::Output::posChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }

            if (m_outputsChangedSignal & (eScreenSignal::isConnectedChanged|eScreenSignal::isOutputChanged)) {
                return;
            }

            m_outputsChangedSignal |= eScreenSignal::isPosChanged;
            USD_LOG(LOG_DEBUG,"posChanged:%s",senderOutput->name().toLatin1().data());
            Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                if (output->name() == senderOutput->name()) {
                    output->setPos(senderOutput->pos());
                    break;
                }
            }
            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
        },Qt::QueuedConnection);

        connect(output.data(), &KScreen::Output::sizeChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            m_outputsChangedSignal |= eScreenSignal::isSizeChanged;
            USD_LOG(LOG_DEBUG,"sizeChanged:%s",senderOutput->name().toLatin1().data());
            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
        },Qt::QueuedConnection);

        connect(output.data(), &KScreen::Output::modesChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            bool prefferedModeHadChanged = false;
            if (UsdBaseClass::isJJW7200()){
                 Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                     if (output->name()==senderOutput->name()) {
                         output->setEnabled(senderOutput->isConnected());
                         output->setConnected(senderOutput->isConnected());
                         output->setModes(senderOutput->modes());
                         break;
                     }
                 }

                USD_LOG(LOG_DEBUG,"catch a jjw7200 in modes changed..");//jjw断开最后一个显卡不会出发isconnectedchange信号，只会触发modeschanged信号。
                doOutputChanged(senderOutput);
                return;
            }

            if (!(m_outputsChangedSignal & eScreenSignal::isConnectedChanged)) {
                Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                    if (output->name()==senderOutput->name()) {//这里只设置connect，enbale由配置设置。
                        if (output->preferredModeId() == nullptr) {
                             USD_LOG(LOG_DEBUG,"%s prefferedMode is none", senderOutput->name());
                             return;
                        }
                        output->setEnabled(senderOutput->isConnected());
                        output->setConnected(senderOutput->isConnected());
                        if (output->preferredModeId() != senderOutput->preferredModeId()) {
                            output->setModes(senderOutput->modes());
                            USD_LOG(LOG_DEBUG,"old mode id:%s", output->preferredModeId().toLatin1().data());
                            output->setPreferredModes(senderOutput->preferredModes());
                            USD_LOG(LOG_DEBUG,"new mode id:%s", output->preferredModeId().toLatin1().data());
                            prefferedModeHadChanged = true;
                        }
                        break;
                    }
                }

                if (prefferedModeHadChanged) {
                    m_modesChangeOutputs.append(senderOutput->name());
                    m_outputsChangedSignal |= eScreenSignal::isModesChanged;
                }
            }
            USD_LOG(LOG_DEBUG,"%s modesChanged",senderOutput->name().toLatin1().data());
            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
        },Qt::QueuedConnection);

        connect(output.data(), &KScreen::Output::clonesChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            m_outputsChangedSignal |= eScreenSignal::isClonesChanged;
            USD_LOG(LOG_DEBUG,"clonesChanged:%s",senderOutput->name().toLatin1().data());
            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
        },Qt::QueuedConnection);

        connect(output.data(), &KScreen::Output::rotationChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            m_outputsChangedSignal |= eScreenSignal::isRotationChanged;
            USD_LOG(LOG_DEBUG,"rotationChanged:%s",senderOutput->name().toLatin1().data());
            Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                if (output->name() == senderOutput->name()) {
                    output->setRotation(senderOutput->rotation());
                    break;
                }
            }

            USD_LOG(LOG_DEBUG,"rotationChanged:%s",senderOutput->name().toLatin1().data());
            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
        },Qt::QueuedConnection);

        connect(output.data(), &KScreen::Output::currentModeIdChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            USD_LOG(LOG_DEBUG,"currentModeIdChanged:%s",senderOutput->name().toLatin1().data());
            m_outputsChangedSignal |= eScreenSignal::isCurrentModeIdChanged;
            Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                if (output->name() == senderOutput->name()) {
                    output->setCurrentModeId(senderOutput->currentModeId());
                    output->setEnabled(senderOutput->isEnabled());
                    break;
                }
            }

            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
        },Qt::QueuedConnection);

        connect(output.data(), &KScreen::Output::isEnabledChanged, this, [this](){
            KScreen::Output *senderOutput = static_cast<KScreen::Output*> (sender());
            if (senderOutput == nullptr || UsdBaseClass::isWayland()) {
                USD_LOG(LOG_DEBUG,"had a bug..");
                return;
            }
            m_xrandrDbus->sendScreenStateChangedSignal(senderOutput->name(), senderOutput->isEnabled());
            if (m_isSetting) {
                USD_LOG(LOG_ERR,"skip enable Changed signal until applyConfig over");
                return;
            }

            USD_LOG(LOG_DEBUG,"%s isEnabledChanged %d ",senderOutput->name().toLatin1().data(),senderOutput->isEnabled());
            m_outputsChangedSignal |= eScreenSignal::isEnabledChanged;

            Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                if (output->name() == senderOutput->name()) {
                    if (output->isConnected() == senderOutput->isConnected()) {
                        output->setEnabled(senderOutput->isEnabled());
                        break;
                    }
                }
            }

            m_screenSignalTimer->start(SAVE_CONFIG_TIME);
        },Qt::QueuedConnection);
    }

    KScreen::ConfigMonitor::instance()->addConfig(m_configPtr);
    //connect(mConfig.data(), &KScreen::Config::outputAdded,
    //        this, &XrandrManager::outputAddedHandle);

    if (UsdBaseClass::isWayland()) {
        return connectedOutputCount;
    }

    connect(m_configPtr.data(), SIGNAL(outputAdded(KScreen::OutputPtr)),
            this, SLOT(doOutputAdded(KScreen::OutputPtr)));

    connect(m_configPtr.data(), SIGNAL(outputRemoved(int)),
            this, SLOT(doOutputRemoved(int)));

    connect(m_configPtr.data(), &KScreen::Config::primaryOutputChanged,
            this, &XrandrManager::doPrimaryOutputChanged);

    connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged,
            this, &XrandrManager::doOutputsConfigurationChanged, Qt::UniqueConnection);


    if (m_xrandrSettings->keys().contains("hadmate2kscreen")) {//兼容mate配置
        if (m_xrandrSettings->get("hadmate2kscreen").toBool() == false) {
            m_xrandrSettings->set("hadmate2kscreen", true);
            if (readMateToKscreen(connectedOutputCount, outputsList)) {
                USD_LOG(LOG_DEBUG,"convert mate ok...");
                return connectedOutputCount;
            }
        }
    }



    if (!m_outputsConfig->fileExists() && connectedOutputCount > 0) {
        m_outputsConfig->setScreenMode(m_outputModeEnum.valueToKey(discernScreenMode()));
        m_outputsConfig->writeFile(false);
    }


    if (m_outputsConfig->fileExists()) {
        USD_LOG(LOG_DEBUG,"read config:%s.",m_outputsConfig->filePath().toLatin1().data());

        if (UsdBaseClass::isTablet()) {
            for (const KScreen::OutputPtr &output: m_configPtr->outputs()) {
                if (output->isConnected() && output->isEnabled()) {
                    output->setRotation(static_cast<KScreen::Output::Rotation>(getCurrentRotation()));
                }
            }
        } else {
            int needSetParamWhenStartup = false;
            std::unique_ptr<xrandrConfig> monitoredConfig = m_outputsConfig->readFile(false);

            if (monitoredConfig == nullptr ) {
                USD_LOG(LOG_DEBUG,"config a error");
                setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode));
                return connectedOutputCount;
            }

            m_outputsConfig = std::move(monitoredConfig);
            Q_FOREACH(const KScreen::OutputPtr &oldOutput, m_configPtr->outputs()) {
                if (!oldOutput->isConnected()) {
                    continue;
                }

                if (needSetParamWhenStartup) {
                    break;
                }

                Q_FOREACH(const KScreen::OutputPtr &output,m_outputsConfig->data()->outputs()) {
                    if(oldOutput->name() == output->name()) {
                        USD_LOG_SHOW_PARAMS(oldOutput->name().toLatin1().data());
                        USD_LOG_SHOW_OUTPUT(output);
                        USD_LOG_SHOW_OUTPUT(oldOutput);
                        if(oldOutput->size() != output->size() || oldOutput->pos() != output->pos()
                                || oldOutput->scale() != output->scale() || oldOutput->rotation() != output->rotation() ||
                                oldOutput->isPrimary() != output->isPrimary() || oldOutput->isEnabled() != output->isEnabled()) {
                            needSetParamWhenStartup = true;
                            break;
                        }
                        if(oldOutput->currentMode().isNull() && output->currentMode().isNull()) {
                            break;
                        }
                        if(oldOutput->currentMode().isNull() || output->currentMode().isNull()) {
                            needSetParamWhenStartup = true;
                            break;
                        } else {
                            if(oldOutput->currentMode()->size() != output->currentMode()->size()) {
                                needSetParamWhenStartup = true;
                            }
                        }
                    }
                }
            }

            if (needSetParamWhenStartup) {
                applyConfig();
            } else {
                calibrateTouchDevice();
            }
        }
    } else {
        setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode));
    }
    return connectedOutputCount;
}

bool XrandrManager::checkPrimaryOutputsIsSetable()
{
    int connecedScreenCount = 0;
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()){
        if (output->isConnected()){
            connecedScreenCount++;
        }
    }

    if (connecedScreenCount < 2) {
        USD_LOG(LOG_DEBUG, "skip set command cuz ouputs count :%d connected:%d",m_outputsConfig->data()->outputs().count(), connecedScreenCount);
        return false;
    }

    if (nullptr == m_outputsConfig->data()->primaryOutput()){
        USD_LOG(LOG_DEBUG,"can't find primary screen.");
        Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
            if (output->isConnected()) {
                output->setPrimary(true);
                output->setEnabled(true);
                USD_LOG(LOG_DEBUG,"set %s as primary screen.",output->name().toLatin1().data());
                break;
            }
        }
    }
    return true;
}

bool XrandrManager::readAndApplyOutputsModeFromConfig(UsdBaseClass::eScreenMode eMode)
{
     if (UsdBaseClass::isTablet()) {
         return false;
     }

    m_outputsConfig->setScreenMode(m_outputModeEnum.valueToKey(eMode));
    if (m_outputsConfig->fileScreenModeExists(m_outputModeEnum.valueToKey(eMode))) {
        std::unique_ptr<xrandrConfig> MonitoredConfig = m_outputsConfig->readFile(true);
        if (MonitoredConfig == nullptr) {
            USD_LOG(LOG_DEBUG,"config a error");
            return false;
        } else {
            m_outputsConfig = std::move(MonitoredConfig);
            if (checkSettable(eMode)) {
                applyConfig();
                return true;
            }
        }
    }
    return false;
}

bool XrandrManager::checkSettable(UsdBaseClass::eScreenMode eMode)
{
    QList<QRect> listQRect;
    int x=0;
    int y=0;
    bool isSameRect = true;

    if(discernScreenMode()!= eMode) {
        return false;
    }

    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()){
        if (!output->isConnected() || !output->isEnabled()){
            continue;
        }
        listQRect<<output->geometry();
        x += output->geometry().x();
        y += output->geometry().y();
    }

    for (int i = 0; i < listQRect.size()-1; i++){
        if (listQRect.at(i) != listQRect.at(i+1)) {
            isSameRect = false;
        }
    }

    if (eMode == UsdBaseClass::eScreenMode::cloneScreenMode) {
        if (!isSameRect) {
            return false;
        }
    } else if (eMode == UsdBaseClass::eScreenMode::extendScreenMode) {
        if (isSameRect || (x==y && x==0)) {
            return false;
        }
    }
    return true;
}

void XrandrManager::doTabletModeChanged(const bool tablemode)
{
    int screenConnectedCount = 0;
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (true == output->isConnected()) {
            screenConnectedCount++;
        }
    }

    if (screenConnectedCount<2) {
        return;
    }
    if (tablemode) {
        setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode));
    } else {
//        setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::extendScreenMode));
    }
    USD_LOG(LOG_DEBUG,"recv mode changed:%d", tablemode);
}

void XrandrManager::setOutputsModeToClone(bool needReadFormConfig)
{
    int bigestResolution = 0;
    bool hadFindFirstScreen = false;

    QString primaryModeId;
    QString secondaryModeId;
    QString secondScreen;

    QSize primarySize(0,0);
    float primaryRefreshRate = 0;
    float secondaryRefreshRate = 0;

    KScreen::OutputPtr primaryOutput;// = mMonitoredConfig->data()->primaryOutput();

    if (false == checkPrimaryOutputsIsSetable()) {
        return;
    }
    
    if(!isDefaultStatus) {
        if (needReadFormConfig) {
            if (readAndApplyOutputsModeFromConfig(UsdBaseClass::eScreenMode::cloneScreenMode)) {
                return;
            }
        }
    }

    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (false == output->isConnected()) {
            continue;
        }

        output->setEnabled(true);
        output->setPos(QPoint(0,0));
        output->setRotation(static_cast<KScreen::Output::Rotation>(1));
        if (false == hadFindFirstScreen) {
            hadFindFirstScreen = true;
            primaryOutput = output;
            continue;
        }
        output->setPos(QPoint(0,0));

        secondaryRefreshRate = 0;

        secondScreen = output->name().toLatin1().data();
        //遍历模式找出最大分辨率的克隆模式
        Q_FOREACH(auto primaryMode, primaryOutput->modes()) {
            Q_FOREACH(auto newOutputMode, output->modes()) {
                primaryOutput->setPos(QPoint(0,0));
                bigestResolution = primarySize.width()*primarySize.height();


                if (primaryMode->size() == newOutputMode->size()) {

                    if (bigestResolution < primaryMode->size().width() * primaryMode->size().height()) {
                        
                        primarySize = primaryMode->size();
                        primaryRefreshRate = primaryMode->refreshRate();
                        primaryOutput->setCurrentModeId(primaryMode->id());
                    
                        secondaryRefreshRate = newOutputMode->refreshRate();
                        output->setCurrentModeId(newOutputMode->id());

                    } else if (bigestResolution ==  primaryMode->size().width() * primaryMode->size().height()) {
                        // USD_LOG(LOG_DEBUG,"相同分辨率下,preferredModeId:[%s],modeID:[%s],",primaryOutput->preferredModeId().toLatin1().data(),newOutputMode->id().toLatin1().data());
                     
                        if (primaryRefreshRate < primaryMode->refreshRate()) {
                            primaryRefreshRate = primaryMode->refreshRate();
                            primaryOutput->setCurrentModeId(primaryMode->id());
                        }

                        if (secondaryRefreshRate < newOutputMode->refreshRate()) {
                            secondaryRefreshRate = newOutputMode->refreshRate();
                            output->setCurrentModeId(newOutputMode->id());
                            USD_LOG(LOG_DEBUG," Clone: set mode:[%s]",newOutputMode->id().toLatin1().data());
                        }

                        // USD_LOG_SHOW_OUTPUT(output);
                    }
                }
            }
        }

        if (UsdBaseClass::isTablet()) {
            output->setRotation(static_cast<KScreen::Output::Rotation>(getCurrentRotation()));
            primaryOutput->setRotation(static_cast<KScreen::Output::Rotation>(getCurrentRotation()));
        }
        USD_LOG_SHOW_OUTPUT(output);
    }

    if (0 == bigestResolution) {
       setOutputsMode(m_outputModeEnum.key(UsdBaseClass::eScreenMode::extendScreenMode));
    } else {
       applyConfig();
    }
}

void XrandrManager::setOutputEnable(QString outputName,bool isEnabled)
{
    
    USD_LOG(LOG_DEBUG," ---------- 设置 output enable----------");

     //假设是扩展模式下
    int dstX = 0;
    int dstY = 0;
    std::unique_ptr<xrandrConfig> MonitoredConfig  = m_outputsConfig->readFile(false);
    USD_LOG(LOG_DEBUG, "Output->name:%s",outputName.toLatin1().data());
    if (MonitoredConfig!=nullptr) {   //如果多屏开启时，文件被用户删了，此时是读不到的
        int curtPosX = 0,curtPosY = 0;
        if(isEnabled) {
            //广度优先
            Q_FOREACH(const KScreen::OutputPtr &output,MonitoredConfig->data()->outputs()) {
                if(!output->isConnected() || !output->isEnabled()) {
                    continue;
                }
                curtPosX = output->pos().x() + output->currentMode()->size().width();
                curtPosY = output->pos().y();
               
                if(dstX < curtPosX) {
                    dstX = curtPosX;
                    dstY = curtPosY;        //这时候获取的dstY可能是Y方向屏幕的某一个
                }
                else if(dstX == curtPosX && dstY < curtPosY) {  //存在相同的最大X
                        dstY = curtPosY;  
                }
            }
            
	        Q_FOREACH(const KScreen::OutputPtr &output, MonitoredConfig->data()->outputs()) {
                if(output->name().toLatin1().data() == outputName) {

                    if(output->isConnected() && output->isEnabled()!= isEnabled) {
                        float refreshRate = 0.0;
                        int screenSize = 0;
                        output->setRotation(static_cast<KScreen::Output::Rotation>(1));
                        Q_FOREACH(auto Mode, output->modes()){
                            if (Mode->size().width()*Mode->size().height() < screenSize){
                                continue;
                            } else if (Mode->size().width()*Mode->size().height() == screenSize) {
                                if (Mode->refreshRate() <= refreshRate) {
                                    continue;
                                }
                            }

                            refreshRate = Mode->refreshRate();
                            screenSize = Mode->size().width()*Mode->size().height();
                            output->setCurrentModeId(Mode->id());
                        }

                        output->setPos(QPoint(dstX,dstY));
                        output->setEnabled(true);  
          
                        applyConfig();
		            }
                    break;
                }
            }
        }
        else {
            //镜像模式下，不支持对单个屏幕操作
            //因此，考虑（广义）扩展就行
            bool hadFindPrimay = false;
            Q_FOREACH(const KScreen::OutputPtr &output,MonitoredConfig->data()->outputs()) {
                if(!output->isConnected() || !output->isEnabled())
                    continue;
                if(output->name().toLatin1().data() == outputName) {
                    output->setEnabled(false);
                    continue;
                }

                if (!hadFindPrimay) {
                    output->setPrimary(true);
                    hadFindPrimay = true;
                } else {
                    if (output->isPrimary()){
                        continue;
                    }
                }
               
                output->setPos(QPoint(dstX,0));
                dstX += output->currentMode()->size().width();
            }

        }

        m_outputsConfig = std::move(MonitoredConfig);
        m_outputsConfig->writeFile(false);
        m_outputsConfig->readFile(false);
        applyConfig();
        return;
    }

}

bool XrandrManager::getOutputEnable(QString outputName)
{
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if(output->name().toLatin1().data() == outputName) {
            if(output->isConnected()) {
                return output->isEnabled();
            }
            return false;
        }
        return false;
    }
}


void XrandrManager::setOutputsModeToFirst(bool isFirstMode)
{
    int posX = 0;
    // int maxScreenSize = 0;
    bool hadFindFirstScreen = false;
    bool hadSetPrimary = false;
    float refreshRate = 0.0;

    if (false == checkPrimaryOutputsIsSetable()) {
        //return; //因为有用户需要在只有一个屏幕的情况下进行了打开，所以必须走如下流程。
    }
    if (isFirstMode){
        if (readAndApplyOutputsModeFromConfig(UsdBaseClass::eScreenMode::firstScreenMode)) {
            return;
        }
    } else {
        if (readAndApplyOutputsModeFromConfig(UsdBaseClass::eScreenMode::secondScreenMode)) {
            return;
        }
    }

     Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (output->isConnected()) {
            output->setEnabled(true);
        } else {
            output->setEnabled(false);
            continue;
        }
        //找到第一个屏幕（默认为内屏）
        if (hadFindFirstScreen) {
            output->setEnabled(!isFirstMode);
        } else {
            hadFindFirstScreen = true;
            output->setEnabled(isFirstMode);
        }

        if (output->isEnabled()) {
            if(hadSetPrimary) {
                output->setPrimary(false);
            } else {
                hadSetPrimary = true;
                output->setPrimary(true);
            }

            int maxWidth = 0;
            int maxHeight = 0;
            int maxScreenSize = 0;

            Q_FOREACH(auto Mode, output->modes()){

                if (Mode->size().width()*Mode->size().height() < maxScreenSize) {
                    continue;
                } else if (Mode->size().width()*Mode->size().height() == maxScreenSize) {
                    if (refreshRate < Mode->refreshRate()) {
                        refreshRate = Mode->refreshRate();
                        output->setCurrentModeId(Mode->id());
                        USD_LOG(LOG_DEBUG,"compare refreshRate and then use mode :%s %f",Mode->id().toLatin1().data(), Mode->refreshRate());
                    }
                    continue;
                }

                refreshRate = Mode->refreshRate();
                maxWidth = Mode->size().width();
                maxHeight = Mode->size().height();
                maxScreenSize = maxWidth * maxHeight;
                output->setCurrentModeId(Mode->id());
                USD_LOG_SHOW_PARAM1(maxScreenSize);
                USD_LOG(LOG_DEBUG,"compare size and then use mode :%s %f,size(%d,%d)",Mode->id().toLatin1().data(), refreshRate,maxWidth,maxHeight);
            }
            output->setPos(QPoint(posX,0));
            posX+= maxWidth;
            USD_LOG(LOG_DEBUG,"maxWidthByMode:%d , screenWidthByOutput:%d , posX = %d",maxWidth,output->size().width(),posX);
        }
        USD_LOG_SHOW_OUTPUT(output);
    }

    applyConfig();
}

void XrandrManager::setOutputsModeToExtend()
{
    int primaryX = 0;
    int screenSize = 0;
    int singleMaxWidth = 0;
    float refreshRate = 0.0;
    bool hadFindPrimay = false;

    if (false == checkPrimaryOutputsIsSetable()) {
        return;
    }

    if(!isDefaultStatus) { 
        if (readAndApplyOutputsModeFromConfig(UsdBaseClass::eScreenMode::extendScreenMode)) {
            return;
        }
    }


    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (!output->isConnected()){
            continue;
        }
        if (hadFindPrimay) {
            output->setPrimary(false);
            continue;
        }
        if (!output->name().contains("eDP")) {//考虑   pnZXECRB项目中内屏为 DisplayPort-0
            output->setPrimary(false);
            continue;
        }
        hadFindPrimay = true;
        output->setPrimary(true);
        output->setEnabled(true);
        output->setRotation(static_cast<KScreen::Output::Rotation>(1));
        screenSize = 0;
        refreshRate = 0.0;
        Q_FOREACH(auto Mode, output->modes()) {
            if (Mode->size().width()*Mode->size().height() < screenSize){
                continue;
            } else if (Mode->size().width()*Mode->size().height() == screenSize) {
                if (Mode->refreshRate() <= refreshRate) {
                    continue;
                }
            }
            refreshRate = Mode->refreshRate();
            screenSize = Mode->size().width()*Mode->size().height();
            output->setCurrentModeId(Mode->id());
            if (Mode->size().width() > singleMaxWidth) {
                singleMaxWidth = Mode->size().width();
            }
        }
        output->setPos(QPoint(0,0));
        primaryX += singleMaxWidth;
        USD_LOG_SHOW_OUTPUT(output);
    }

    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        screenSize = 0;
        refreshRate = 0.0;
        singleMaxWidth = 0;

        if (output->isConnected()){
            output->setEnabled(true);
        } else {
            output->setEnabled(false);
            continue;
        }

        if (!hadFindPrimay) {
            output->setPrimary(true);
            hadFindPrimay = true;
        } else {
            if (output->isPrimary()){
                continue;
            }
        }

        output->setEnabled(true);
        output->setRotation(static_cast<KScreen::Output::Rotation>(1));

        Q_FOREACH(auto Mode, output->modes()){
            if (Mode->size().width()*Mode->size().height() < screenSize){
                continue;
            } else if (Mode->size().width()*Mode->size().height() == screenSize) {
                if (Mode->refreshRate() <= refreshRate) {
                    continue;
                }
            }

            refreshRate = Mode->refreshRate();
            screenSize = Mode->size().width()*Mode->size().height();
            output->setCurrentModeId(Mode->id());
            if (Mode->size().width() > singleMaxWidth) {
                singleMaxWidth = Mode->size().width();
            }
        }
        if (UsdBaseClass::isTablet()) {
            output->setRotation(static_cast<KScreen::Output::Rotation>(getCurrentRotation()));
        }
        output->setPos(QPoint(primaryX,0));
        primaryX += singleMaxWidth;
        USD_LOG(LOG_DEBUG,"Extend: singleMaxWidth:%d, posX", singleMaxWidth,primaryX);
        USD_LOG_SHOW_OUTPUT(output);
    }
    applyConfig();
}

//该方法会在某些显卡上产生错误
void XrandrManager::setOutputsModeToExtendWithPreferredMode()
{
    int primaryX = 0;
    int screenSize = 0;
    int singleMaxWidth = 0;
    float refreshRate = 0.0;
    bool hadFindPrimay = false;
    int connectedScreens;

    if (false == checkPrimaryOutputsIsSetable()) {
        return;
    }

    if (readAndApplyOutputsModeFromConfig(UsdBaseClass::eScreenMode::extendScreenMode)) {
        return;
    }

    //先找eDP，
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {

        if (!output->isConnected()){
            continue;
        }
        if (hadFindPrimay) {
            output->setPrimary(false);
            continue;
        }
        if (!output->name().contains("eDP")) {//考虑   pnZXECRB项目中内屏为 DisplayPort-0
            output->setPrimary(false);
            continue;
        }

        hadFindPrimay = true;
        output->setPrimary(true);
        output->setEnabled(true);
        output->setRotation(static_cast<KScreen::Output::Rotation>(1));
        if (UsdBaseClass::isTablet()) {
            output->setRotation(static_cast<KScreen::Output::Rotation>(getCurrentRotation()));
        }
        output->setCurrentModeId(output->preferredModeId());
        singleMaxWidth = output->preferredMode()->size().width();
        output->setPos(QPoint(0,0));
        primaryX += singleMaxWidth;
        USD_LOG_SHOW_OUTPUT(output);
    }

    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {

        if (!output->isConnected()){
            continue;
        }

        if (!hadFindPrimay) {
            output->setPrimary(true);
            hadFindPrimay = true;
        } else {
            if (output->isPrimary()){
                continue;
            }
        }

        output->setEnabled(true);
        output->setRotation(static_cast<KScreen::Output::Rotation>(1));
        if (UsdBaseClass::isTablet()) {
            output->setRotation(static_cast<KScreen::Output::Rotation>(getCurrentRotation()));
        }
        output->setCurrentModeId(output->preferredModeId());
        singleMaxWidth = output->preferredMode()->size().width();
        output->setPos(QPoint(primaryX,0));
        primaryX += singleMaxWidth;
        USD_LOG_SHOW_OUTPUT(output);
    }
    applyConfig();
}



void XrandrManager::setOutputsParam(QString screensParam)
{
    USD_LOG(LOG_DEBUG,"param:%s", screensParam.toLatin1().data());
    std::unique_ptr<xrandrConfig> temp  = m_outputsConfig->readScreensConfigFromDbus(screensParam);
    if (nullptr != temp) {
        m_outputsConfig = std::move(temp);
    }
    applyConfig();
}

/*
 * 设置显示模式
*/
void XrandrManager::setOutputsMode(QString modeName)
{
    //检查当前屏幕数量，只有一个屏幕时不设置
    int screenConnectedCount = 0;
    int modeValue = m_outputModeEnum.keyToValue(modeName.toLatin1().data());
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (true == output->isConnected()) {
            screenConnectedCount++;
        }
    }

    if(screenConnectedCount == 0) {
        return;
    }

    if(screenConnectedCount <= 1) {
        if (modeValue == UsdBaseClass::eScreenMode::cloneScreenMode ||
                 modeValue == UsdBaseClass::eScreenMode::extendScreenMode) {
            modeValue = UsdBaseClass::eScreenMode::firstScreenMode;
        }
    }

    if (UsdBaseClass::isWayland()){
        QString cmd = "";
        if (m_outputsConfig->data()->outputs().count()<2) {
            return ;
        }
        switch (modeValue) {
        case UsdBaseClass::eScreenMode::cloneScreenMode:
            USD_LOG(LOG_DEBUG,"set mode to %s",modeName.toLatin1().data());
            cmd = "clone";
            break;
        case UsdBaseClass::eScreenMode::firstScreenMode:
            USD_LOG(LOG_DEBUG,"set mode to %s",modeName.toLatin1().data());
            cmd = "other";
            break;
        case UsdBaseClass::eScreenMode::secondScreenMode:
            USD_LOG(LOG_DEBUG,"set mode to %s",modeName.toLatin1().data());
            cmd = "first";
            break;
        case UsdBaseClass::eScreenMode::extendScreenMode:
            USD_LOG(LOG_DEBUG,"set mode to %s",modeName.toLatin1().data());
            cmd = "extend";
            break;
        default:
            USD_LOG(LOG_DEBUG,"set mode fail can't set to %s",modeName.toLatin1().data());
            return;
        }

        QStringList cmdlist;
        cmdlist<<"-m"<<cmd;
        QProcess::startDetached("kscreen-doctor",cmdlist);
//        USD_LOG(LOG_DEBUG,"cmdlist:%s",cmdlist.toLatin1().data());
        return;
    }

    switch (modeValue) {
    case UsdBaseClass::eScreenMode::cloneScreenMode:
        USD_LOG(LOG_DEBUG,"set mode to %s",modeName.toLatin1().data());
        setOutputsModeToClone();
        break;
    case UsdBaseClass::eScreenMode::firstScreenMode:
        USD_LOG(LOG_DEBUG,"set mode to %s",modeName.toLatin1().data());
        setOutputsModeToFirst(true);
        break;
    case UsdBaseClass::eScreenMode::secondScreenMode:
        USD_LOG(LOG_DEBUG,"set mode to %s",modeName.toLatin1().data());
        setOutputsModeToFirst(false);
        break;
    case UsdBaseClass::eScreenMode::extendScreenMode:
        USD_LOG(LOG_DEBUG,"set mode to %s",modeName.toLatin1().data());
        setOutputsModeToExtend();
        break;
    default:
        USD_LOG(LOG_DEBUG,"set mode fail can't set to %s",modeName.toLatin1().data());
        return;
    }

    //每次applyconfig时已经发了，没必要
//    sendOutputsModeToDbus();
}

/*
 * 识别当前显示的模式
*/
UsdBaseClass::eScreenMode XrandrManager::discernScreenMode()
{
#define  OnlyFirstScreen  ((quint8)0x01 << 0) 

    QPoint firstScreenQPoint;
    QSize firstScreenQsize;

    bool hadFindFirstScreen = false;
    bool isGeneralClone = true;   //广义克隆：两个副屏之间相同、第一屏和第三屏相同但和第二屏不同等情况
    uint8_t screenEnableStatus = 0x00;   

    int num = 0;
    Q_FOREACH(const KScreen::OutputPtr &output, m_outputsConfig->data()->outputs()) {
        if (output->isConnected()) {          
            
    	    if (output->isEnabled() && output->currentMode()!=nullptr) {
                    screenEnableStatus |= (0x01 << num);

                    if(!hadFindFirstScreen) {
                        firstScreenQsize = output->currentMode()->size();
                        firstScreenQPoint = output->pos();
                        hadFindFirstScreen = true;
                    }
                    else {
    		            //只要出现一次不相等，就没必要继续比较了
    		            if(!isGeneralClone)
                            continue;
                        if( firstScreenQsize != output->currentMode()->size() || firstScreenQPoint != output->pos()) {
                            isGeneralClone = false;
                            USD_LOG(LOG_DEBUG," --   isGeneralClone: %d \n",isGeneralClone);
    		            }
                    }
            } else {
                USD_LOG(LOG_DEBUG,"screenEnableStatus: %X \n",screenEnableStatus);
            }
        
            num ++;
        }

    }

    if(screenEnableStatus  == OnlyFirstScreen) {
        USD_LOG(LOG_DEBUG,"mode : firstScreenMode\n");
        return UsdBaseClass::eScreenMode::firstScreenMode;    
    }
    if( screenEnableStatus && (OnlyFirstScreen << 7 != (screenEnableStatus << 7) % 256)) {
        USD_LOG(LOG_DEBUG,"mode : otherScreenMode");
        return UsdBaseClass::eScreenMode::secondScreenMode;    
    }

    if (isGeneralClone) {
        USD_LOG(LOG_DEBUG,"mode : cloneScreenMode");
        return UsdBaseClass::eScreenMode::cloneScreenMode;
    }
  
    USD_LOG(LOG_DEBUG,"mode : extendScreenMode");
    return UsdBaseClass::eScreenMode::extendScreenMode;
}

void XrandrManager::doCalibrate(const QString screenMap)
{
    USD_LOG(LOG_DEBUG,"controlScreenMap ...");
    doRotationChanged(screenMap);
}

void XrandrManager::disableCrtc()
{
    int tempInt;

    Display	*m_pDpy;
    Window	m_rootWindow;
    XRRScreenResources  *m_pScreenRes;
    int m_screen;
    m_pDpy = XOpenDisplay (NULL);
    if (m_pDpy == NULL) {
        USD_LOG(LOG_DEBUG,"XOpenDisplay fail...");
        return ;
    }

    m_screen = DefaultScreen(m_pDpy);
    if (m_screen >= ScreenCount (m_pDpy)) {
        USD_LOG(LOG_DEBUG,"Invalid screen number %d (display has %d)",m_screen, ScreenCount(m_pDpy));
        return ;
    }

    m_rootWindow = RootWindow(m_pDpy, m_screen);
    m_pScreenRes = XRRGetScreenResources(m_pDpy, m_rootWindow);
    if (NULL == m_pScreenRes) {
        USD_LOG(LOG_DEBUG,"could not get screen resources",m_screen, ScreenCount(m_pDpy));
        return ;
    }
    if (m_pScreenRes->noutput == 0) {
        USD_LOG(LOG_DEBUG, "noutput is 0!!");
        return ;
    }
    USD_LOG(LOG_DEBUG,"initXparam success");
    for (tempInt = 0; tempInt < m_pScreenRes->ncrtc; tempInt++) {
        int ret = 0;
        ret = XRRSetCrtcConfig (m_pDpy, m_pScreenRes, m_pScreenRes->crtcs[tempInt], CurrentTime,
                                0, 0, None, RR_Rotate_0, NULL, 0);
        if (ret != RRSetConfigSuccess) {
            USD_LOG(LOG_ERR,"disable crtc:%d error! ");
        }
    }
    XCloseDisplay(m_pDpy);
    USD_LOG(LOG_DEBUG,"disable crtc  success");
}

/**
 * @brief XrandrManager::StartXrandrIdleCb
 * 开始时间回调函数
 */
void XrandrManager::active()
{
    m_acitveTimer->stop();
    m_screenSignalTimer = new QTimer(this);
    connect(m_screenSignalTimer, SIGNAL(timeout()), this, SLOT(doSaveConfigTimeOut()));
    USD_LOG(LOG_DEBUG,"StartXrandrIdleCb ok.");

    connect(m_outputsInitTimer,  SIGNAL(timeout()), this, SLOT(getInitialConfig()));
    if (UsdBaseClass::isWayland()) {
        USD_LOG(LOG_DEBUG,"wayland just use set get screen mode");
        return;
    }

    m_outputsInitTimer->start(10);
    connect(m_xrandrDbus, SIGNAL(setScreenModeSignal(QString)), this, SLOT(setOutputsMode(QString)));
    connect(m_xrandrDbus, SIGNAL(setScreensParamSignal(QString)), this, SLOT(setOutputsParam(QString)));

#if 0
    QDBusInterface *modeChangedSignalHandle = new QDBusInterface(DBUS_XRANDR_NAME,DBUS_XRANDR_PATH,DBUS_XRANDR_INTERFACE,QDBusConnection::sessionBus(),this);

    if (modeChangedSignalHandle->isValid()) {
        connect(modeChangedSignalHandle, SIGNAL(screenModeChanged(int)), this, SLOT(screenModeChangedSignal(int)));

    } else {
        USD_LOG(LOG_ERR, "modeChangedSignalHandle");
    }

    QDBusInterface *screensChangedSignalHandle = new QDBusInterface(DBUS_XRANDR_NAME,DBUS_XRANDR_PATH,DBUS_XRANDR_INTERFACE,QDBusConnection::sessionBus(),this);

     if (screensChangedSignalHandle->isValid()) {
         connect(screensChangedSignalHandle, SIGNAL(screensParamChanged(QString)), this, SLOT(screensParamChangedSignal(QString)));
         //USD_LOG(LOG_DEBUG, "..");
     } else {
         USD_LOG(LOG_ERR, "screensChangedSignalHandle");
     }
#endif

}
