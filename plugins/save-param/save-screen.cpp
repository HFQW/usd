/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
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

#include <QScreen>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <kwindowsystem.h>
#include <QFile>
#include <QDir>
#include <QtXml>

#include "xrandroutput.h"
#include "save-screen.h"
#include "clib-syslog.h"
#include "usd_base_class.h"

typedef struct _transform {
    XTransform	    transform;
    const char	    *filter;
    int		    nparams;
    XFixed	    *params;
}Transform_t;


class OutputInfo {
public:
    XRROutputInfo *m_output;
    RRMode m_modeId;
    int x;
    int y;
    int height;
    int width;
    RROutput m_outputId;
};

/*
 * XRR接口中，crtc负责处理显示的方式，output只是一个显示器，output关联到crtc中才会按照预设的方式进行显示。。crtc在什么情况下会关联多个output呢？
 * XRRSetCrtc时，output是传递的数组，就代表其可以控制多个显示器。(需要测试其功能，没有连接的显示器的crtc是0)。。
 * XRRSetCrtcTransform用来处理scale。
 * 缩放时
 * output->transform.filter = "bilinear";
 * 不缩放时
 * output->transform.filter = "nearest";
*/


double getModeRefresh (const XRRModeInfo *mode_info)
{
    double rate;
    double vTotal = mode_info->vTotal;

    if (mode_info->modeFlags & RR_DoubleScan) {
    /* doublescan doubles the number of lines */
    vTotal *= 2;
    }

    if (mode_info->modeFlags & RR_Interlace) {
    /* interlace splits the frame into two fields */
    /* the field rate is what is typically reported by monitors */
    vTotal /= 2;
    }

    if (mode_info->hTotal && vTotal)
    rate = ((double) mode_info->dotClock /
        ((double) mode_info->hTotal * (double) vTotal));
    else
        rate = 0;
    return rate;
}
SaveScreenParam::SaveScreenParam(QObject *parent): QObject(parent)
{
    Q_UNUSED(parent);

    m_userName = "";

    m_isGet = false;
    m_isSet = false;
    m_isSameHash = false;
}

SaveScreenParam::~SaveScreenParam()
{
    if (m_pDpy != nullptr) {
        XCloseDisplay(m_pDpy);
    }
    if (m_pScreenRes != nullptr) {
        XRRFreeScreenResources(m_pScreenRes);
    }
    USD_LOG(LOG_DEBUG,".....");
}
#ifdef MODULE_NAME
void SaveScreenParam::getConfig(){
    QObject::connect(new KScreen::GetConfigOperation(), &KScreen::GetConfigOperation::finished,
                     [&](KScreen::ConfigOperation *op) {

        if (m_MonitoredConfig) {
            if (m_MonitoredConfig->data()) {
                KScreen::ConfigMonitor::instance()->removeConfig(m_MonitoredConfig->data());
                for (const KScreen::OutputPtr &output : m_MonitoredConfig->data()->outputs()) {
                    output->disconnect(this);
                }
                m_MonitoredConfig->data()->disconnect(this);
            }
            m_MonitoredConfig = nullptr;
        }

        m_MonitoredConfig = std::unique_ptr<xrandrConfig>(new xrandrConfig(qobject_cast<KScreen::GetConfigOperation*>(op)->config()));
        m_MonitoredConfig->setValidityFlags(KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen);

        if (false == m_userName.isEmpty()) {
            m_MonitoredConfig->setUserName(m_userName);
        }
        else if (isSet()) {

        } else if (isGet()) {
            m_MonitoredConfig->writeFileForLightDM(false);
            Q_EMIT getConfigFinished();
        } else {
            m_MonitoredConfig->writeFile(false);
            exit(0);
        }
    });
}
#endif
//先判断是否，可设置为镜像，如果不可设置为镜像则需要设置为拓展。
void SaveScreenParam::setWithoutConfig(OutputsConfig *outputsConfig)
{
    int primaryId;
    bool isSetClone = true;

    primaryId = XRRGetOutputPrimary(m_pDpy, m_rootWindow);
    for (int koutput = 0; koutput < m_pScreenRes->noutput; koutput++) {//已经拓展模式设置完毕
        double scale = 1.0;
        XRROutputInfo *outputInfo = XRRGetOutputInfo (m_pDpy, m_pScreenRes, m_pScreenRes->outputs[koutput]);
        if (outputInfo->connection != RR_Connected) {
            XRRFreeOutputInfo(outputInfo);
            continue;
        }
        UsdOuputProperty *outputProperty;
        outputProperty = new UsdOuputProperty();
        if ((0==primaryId && 0==koutput )|| (primaryId == (int)m_pScreenRes->outputs[koutput])) {
            outputProperty->setProperty(OUTPUT_PRIMARY, 1);
        } else {
            outputProperty->setProperty(OUTPUT_PRIMARY, 0);
        }
        outputProperty->setProperty(OUTPUT_SPACE, koutput);
        outputProperty->setProperty(OUTPUT_NAME, outputInfo->name);
        outputProperty->setProperty(CRTC_ID, (int)outputInfo->crtc);
        outputProperty->setProperty(OUTPUT_ID, (int)m_pScreenRes->outputs[koutput]);
        outputProperty->setProperty("npreferred",(int)outputInfo->npreferred);
        outputProperty->setProperty(OUTPUT_WIDTHMM, (int)outputInfo->mm_width);
        outputProperty->setProperty(OUTPUT_HEIGHTMM, (int)outputInfo->mm_height);

        if (0 == outputInfo->npreferred) {
            outputProperty->setProperty(MODE_ID, (int)outputInfo->modes[0]);//识别不出最佳模式
        } else {
            outputProperty->setProperty(MODE_ID, (int)outputInfo->modes[0]);
        }
        USD_LOG(LOG_DEBUG,"%s->npreferred:%d",outputInfo->name, outputInfo->npreferred);
        for (int kmode = 0; kmode < outputInfo->nmode; kmode++) {
            UsdOutputMode mode;
            mode.m_modeId = outputInfo->modes[kmode];
            for (int kresMode = 0; kresMode < m_pScreenRes->nmode; kresMode++) {
                XRRModeInfo *resMode = &m_pScreenRes->modes[kresMode];
                if (mode.m_modeId == resMode->id) {
                    mode.m_height = resMode->height;
                    mode.m_width = resMode->width;
                    mode.m_rate =  getModeRefresh(resMode);
                    break;
                }
            }
//            USD_LOG(LOG_DEBUG,"%s:addmode %dx%d", outputProperty->property(OUTPUT_NAME).toString().toLatin1().data(), mode.m_height, mode.m_width);
            outputProperty->addMode(mode);
        }


        outputProperty->setProperty(OUTPUT_ROTATION, 1);
        outputProperty->setProperty(MODE_ID, outputProperty->m_modes[0].m_modeId);//for XRR interface preffered mode is 0
        outputProperty->setProperty(OUTPUT_WIDTH, outputProperty->m_modes[0].m_width);
        outputProperty->setProperty(OUTPUT_HEIGHT, outputProperty->m_modes[0].m_height);
        outputProperty->setProperty(OUTPUT_RATE, outputProperty->m_modes[0].m_rate);


        outputProperty->setProperty(OUTPUT_SCALE, 1);
//        if (outputsConfig->m_dpi == "96") {

//        } else {
//            scale = UsdBaseClass::getScaleWithSize(outputInfo->mm_height, outputInfo->mm_width,
//                                     outputProperty->m_modes[0].m_height, outputProperty->m_modes[0].m_width);
//            outputProperty->setProperty(OUTPUT_SCALE, scale);//
//        }
        outputsConfig->m_screenWidth += outputProperty->m_modes[0].m_width / scale;
        if (outputProperty->m_modes[0].m_height  / scale> outputsConfig->m_screenHeight) {
            outputsConfig->m_screenHeight = outputProperty->m_modes[0].m_height / scale;
        }

        outputsConfig->m_outputList.append(outputProperty);
        XRRFreeOutputInfo(outputInfo);
    }

    if (outputsConfig->m_outputList.count()<2) {
        return;
    }

    //find all output clonemode
   /* for (int koutput = 1; koutput < outputsConfig->m_outputList.count(); koutput++)*/ {
        QList<UsdOutputMode> qlistMode;

        //第一个屏幕支持3840x2160 1920x1080, 720x480.
        //第二支持1920x1080,720x480.
        //第三个支持2560x1440， 1920x1080,720x480.
//        if (koutput==1) {
//            qlistMode = outputsConfig->m_outputList[0]->m_modes;//前两个屏幕从modes里面挑选共有分辨率到cloneModes
//        } else {
//            qlistMode = outputsConfig->m_outputList[0]->m_cloneModes;//第三个屏幕之后从交集内取。
//        }
//        USD_LOG(LOG_DEBUG,";;;===..>>%d,%s,%s",outputsConfig->m_outputList.count(),outputsConfig->m_outputList[0]->property(OUTPUT_NAME).toString().toLatin1().data()
//                ,outputsConfig->m_outputList[1]->property(OUTPUT_NAME).toString().toLatin1().data());
        for (int k0 = 0; k0 < outputsConfig->m_outputList.count(); k0++) {
            for (int k1 = 0; k1 < outputsConfig->m_outputList.count(); k1++) {
                if (k1 == k0) {
                    continue;
                }
                for(int m0 = 0; m0 < outputsConfig->m_outputList[k0]->m_modes.count(); m0++) {
                    bool hadFind = false;
                    for(int m1 = 0; m1 < outputsConfig->m_outputList[k1]->m_modes.count(); m1++) {

                        if (outputsConfig->m_outputList[k0]->m_modes[m0].m_height == 0 ||outputsConfig->m_outputList[k0]->m_modes[m0].m_width == 0) {
                            continue;
                        }
//                        USD_LOG(LOG_DEBUG,"ready check:%s(%dx%d):%s(%dx%d)",outputsConfig->m_outputList[k0]->property(OUTPUT_NAME).toString().toLatin1().data(),
//                                outputsConfig->m_outputList[k0]->m_modes[m0].m_height,
//                                outputsConfig->m_outputList[k0]->m_modes[m0].m_width,
//                                outputsConfig->m_outputList[k1]->property(OUTPUT_NAME).toString().toLatin1().data(),
//                                outputsConfig->m_outputList[k1]->m_modes[m1].m_height,
//                                outputsConfig->m_outputList[k1]->m_modes[m1].m_width);

                        if (outputsConfig->m_outputList[k0]->m_modes[m0].m_height == outputsConfig->m_outputList[k1]->m_modes[m1].m_height
                                && outputsConfig->m_outputList[k0]->m_modes[m0].m_width == outputsConfig->m_outputList[k1]->m_modes[m1].m_width) {
                            hadFind = true;
                            break;
                        }
                    }
                    if (!hadFind) {
//                        USD_LOG(LOG_DEBUG,"%s:clear:%dx%d",outputsConfig->m_outputList[k0]->property(OUTPUT_NAME).toString().toLatin1().data(),
//                                outputsConfig->m_outputList[k0]->m_modes[m0].m_height,
//                                outputsConfig->m_outputList[k0]->m_modes[m0].m_width);
                        outputsConfig->m_outputList[k0]->m_modes[m0].m_height = 0;
                        outputsConfig->m_outputList[k0]->m_modes[m0].m_width = 0;
                    }


                }
            }
        }
        int lastScreenNum = outputsConfig->m_outputList.count()-1;
        for(int m0 = 0; m0 < outputsConfig->m_outputList[lastScreenNum]->m_modes.count(); m0++) {
            bool hadFind = false;
            for(int m1 = 0; m1 < outputsConfig->m_outputList[0]->m_modes.count(); m1++) {

                if (outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_height == 0 ||outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_width == 0) {
                    continue;
                }
//                USD_LOG(LOG_DEBUG,"ready check:%s(%dx%d):%s(%dx%d)",outputsConfig->m_outputList[lastScreenNum]->property(OUTPUT_NAME).toString().toLatin1().data(),
//                        outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_height,
//                        outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_width,
//                        outputsConfig->m_outputList[0]->property(OUTPUT_NAME).toString().toLatin1().data(),
//                        outputsConfig->m_outputList[0]->m_modes[m1].m_height,
//                        outputsConfig->m_outputList[0]->m_modes[m1].m_width);
                if (outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_height == outputsConfig->m_outputList[0]->m_modes[m1].m_height
                        && outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_width == outputsConfig->m_outputList[0]->m_modes[m1].m_width) {
                    hadFind = true;
                    break;
                }
            }
            if (!hadFind) {
                USD_LOG(LOG_DEBUG,"%s:clear:%dx%d",outputsConfig->m_outputList[lastScreenNum]->property(OUTPUT_NAME).toString().toLatin1().data(),
                        outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_height,
                        outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_width);
                outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_height = 0;
                outputsConfig->m_outputList[lastScreenNum]->m_modes[m0].m_width = 0;
            }
        }


        if(0 == outputsConfig->m_outputList[0]->m_modes.count()) {
            isSetClone = false;
            return;
        }
    }

    if (isSetClone) {
        for (int koutput = 0; koutput <outputsConfig->m_outputList.count(); koutput++) {
            int maxCloneWidth = 0;
            int maxCloneHeight = 0;
            double rate = 0.0;
            int modeId = 0;
            for (int kmode = 0;  kmode < outputsConfig->m_outputList[koutput]->m_modes.count(); kmode++) {
                if (outputsConfig->m_outputList[koutput]->m_modes[kmode].m_height + outputsConfig->m_outputList[koutput]->m_modes[kmode].m_width >
                        maxCloneHeight+maxCloneWidth) {
                    modeId = outputsConfig->m_outputList[koutput]->m_modes[kmode].m_modeId;
                    maxCloneHeight = outputsConfig->m_outputList[koutput]->m_modes[kmode].m_height;
                    maxCloneWidth = outputsConfig->m_outputList[koutput]->m_modes[kmode].m_width;
                    rate = outputsConfig->m_outputList[koutput]->m_modes[kmode].m_rate;
                    USD_LOG(LOG_DEBUG,"set%s ===>%d(%dx%d)",outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString().toLatin1().data(),
                           modeId, maxCloneHeight,maxCloneWidth);
                } else if (outputsConfig->m_outputList[koutput]->m_modes[kmode].m_height + outputsConfig->m_outputList[koutput]->m_modes[kmode].m_width ==
                           maxCloneHeight+maxCloneWidth && outputsConfig->m_outputList[koutput]->m_modes[kmode].m_rate > rate){
                    modeId = outputsConfig->m_outputList[koutput]->m_modes[kmode].m_modeId;
                    rate = outputsConfig->m_outputList[koutput]->m_modes[kmode].m_rate;
                    USD_LOG(LOG_DEBUG,"set%s ===>%d(%dx%d)",outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString().toLatin1().data(),
                           modeId, maxCloneHeight,maxCloneWidth);
                } else {
                    continue;
                }

                outputsConfig->m_outputList[koutput]->setProperty(MODE_ID, modeId);
                outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_HEIGHT, maxCloneHeight);
                outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_WIDTH, maxCloneWidth);
                outputsConfig->m_outputList[koutput]->setProperty("x",0);
                outputsConfig->m_outputList[koutput]->setProperty("y", 0);
                USD_LOG(LOG_DEBUG,"set%s ===>%d(%dx%d)",outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString().toLatin1().data(),
                       modeId, maxCloneHeight,maxCloneWidth);
                m_kscreenConfigParam.m_screenWidth = maxCloneWidth/outputsConfig->m_outputList[koutput]->property(OUTPUT_SCALE).toDouble();
                m_kscreenConfigParam.m_screenHeight = maxCloneHeight/outputsConfig->m_outputList[koutput]->property(OUTPUT_SCALE).toDouble();
            }
        }
    }

#if 0
    //可设置为镜像模式
    if (isSetClone) {
        int maxCloneWidth = 0;
        int maxCloneHeight = 0;
        double rate = 0.0;
        int modeId = 0;
        for (int koutput = outputsConfig->m_outputList.count()-1; koutput >= 0; koutput--) {
            rate = 0.0;
            if (outputsConfig->m_dpi == "96") {
                outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_SCALE, 1);
            } else {
//                int mmheight = outputsConfig->m_outputList[koutput]->property(OUTPUT_HEIGHTMM).toInt();
//                int mmwidht =  outputsConfig->m_outputList[koutput]->property(OUTPUT_WIDTHMM).toInt();
//                int height =  outputsConfig->m_outputList[koutput]->property(OUTPUT_HEIGHT).toInt();
//                int widht =  outputsConfig->m_outputList[koutput]->property(OUTPUT_WIDTH).toInt();

//                outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_SCALE, getScaleWithSize(mmheight, mmwidht,
//                                                                     height, widht));//
                 outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_SCALE, 0.5);
            }
            if (koutput == outputsConfig->m_outputList.count()-1) {
                for (int kmode = 0;  kmode < outputsConfig->m_outputList[koutput]->m_cloneModes.count(); kmode++) {
                    if (outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_height + outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_width >=
                            maxCloneHeight+maxCloneWidth) {
                        USD_LOG(LOG_DEBUG,"%s:%dx%d ===>%dx%d",outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString().toLatin1().data(),maxCloneHeight,maxCloneWidth,
                                outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_height,
                                outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_width);
                        modeId = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_modeId;
                        maxCloneHeight = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_height;
                        maxCloneWidth = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_width;
                        rate = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_rate;
                    } else if (outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_height + outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_width ==
                               maxCloneHeight+maxCloneWidth) {
                        if (outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_rate > rate) {
                            modeId = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_modeId;
                            rate = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_rate;
                        }
                    }
                }
                outputsConfig->m_outputList[koutput]->setProperty(MODE_ID, modeId);
                outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_HEIGHT, maxCloneHeight);
                outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_WIDTH, maxCloneWidth);
                outputsConfig->m_outputList[koutput]->setProperty("x",0);
                outputsConfig->m_outputList[koutput]->setProperty("y", 0);
            } else {
                for (int kmode = 0;  kmode < outputsConfig->m_outputList[koutput]->m_cloneModes.count(); kmode++) {
                    if (outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_height * outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_width ==
                            maxCloneHeight*maxCloneWidth && outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_rate > rate) {
                        modeId = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_modeId;
                        maxCloneHeight = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_height;
                        maxCloneWidth = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_width;
                        rate = outputsConfig->m_outputList[koutput]->m_cloneModes[kmode].m_rate;

                        outputsConfig->m_outputList[koutput]->setProperty(MODE_ID, modeId);
                        outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_HEIGHT, maxCloneHeight);
                        outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_WIDTH, maxCloneWidth);
                        outputsConfig->m_outputList[koutput]->setProperty("x",0);
                        outputsConfig->m_outputList[koutput]->setProperty("y", 0);
                    }
                }
            }
        }
        return;
    }

//    //设置为拓展模式
//    for (int koutput = outputsConfig->m_outputList.count() - 1; koutput >= 0; koutput--) {
//        int modeId = outputsConfig->m_outputList[koutput]->m_modes[0].m_modeId;
//        int width = outputsConfig->m_outputList[koutput]->m_modes[0].m_width;
//        int height = outputsConfig->m_outputList[koutput]->m_modes[0].m_height;
//        int widthmm = outputsConfig->m_outputList[koutput]->property(OUTPUT_WIDTHMM).toInt();
//        int heightmm = outputsConfig->m_outputList[koutput]->property(OUTPUT_HEIGHTMM).toInt();
//        double scale = UsdBaseClass::getScaleWithSize(heightmm, widthmm, height, width);

//        outputsConfig->m_outputList[koutput]->setProperty(MODE_ID, modeId);
//        outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_HEIGHT, height / scale);
//        outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_WIDTH, width / scale);
//        outputsConfig->m_outputList[koutput]->setProperty("x", outputsConfig->m_screenWidth);
//        outputsConfig->m_outputList[koutput]->setProperty("y", 0);
//        outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_SCALE, scale);
//        outputsConfig->m_screenWidth += width / scale;
//    }
#endif
}



void SaveScreenParam::setUserConfigParam()
{
    if (false ==initXparam()) {
        exit(0);
    }

    readKscreenConfigAndSetItWithX(getKscreenConfigFullPathInLightDM());
//    exit(0);
}

void SaveScreenParam::setUserName(QString str)
{
    m_userName = str;
}

void SaveScreenParam::setScreenSize()
{
    int screenInt = DefaultScreen (m_pDpy);
    if (m_kscreenConfigParam.m_screenWidth != DisplayWidth(m_pDpy, screenInt) ||
            m_kscreenConfigParam.m_screenHeight != DisplayHeight(m_pDpy, screenInt)) {
        int fb_width_mm;
        int fb_height_mm;

        double dpi = (25.4 * DisplayHeight(m_pDpy, screenInt)) / DisplayHeightMM(m_pDpy,screenInt);
        fb_width_mm = (25.4 * m_kscreenConfigParam.m_screenWidth) /dpi;
        fb_height_mm = (25.4 * m_kscreenConfigParam.m_screenHeight) /dpi;
        //dpi = Dot Per Inch，一英寸是2.54cm即25.4mm
        XRRSetScreenSize(m_pDpy, m_rootWindow, m_kscreenConfigParam.m_screenWidth, m_kscreenConfigParam.m_screenHeight,
                        fb_width_mm, fb_height_mm);
        USD_LOG(LOG_DEBUG,"%dx%d %dx%dmm dpi%f,",m_kscreenConfigParam.m_screenWidth, m_kscreenConfigParam.m_screenHeight,
                fb_width_mm,fb_height_mm,dpi);
    }
}

void SaveScreenParam::readKscreenConfigAndSetItWithX(QString kscreenConfigName)
{
    char *dpi;
    int width;
    int height;
    QString cmd;
    int screenInt;
    QString configFullPath = kscreenConfigName;
    dpi = XGetDefault(m_pDpy, "Xft", "dpi");
    m_kscreenConfigParam.m_dpi = "96";
    USD_LOG(LOG_DEBUG,"dpi:%s", dpi);

    if (dpi != nullptr) {
        m_kscreenConfigParam.m_dpi = QString::fromLatin1(dpi);
    }

    if (!QFile::exists(kscreenConfigName)) {
        setWithoutConfig(&m_kscreenConfigParam);
        SYS_LOG(LOG_DEBUG,"can't open %s's screen config>%s set it without config.",m_userName.toLatin1().data(),configFullPath.toLatin1().data());
    } else {
         SYS_LOG(LOG_DEBUG,"ready open %s's screen config>%s",m_userName.toLatin1().data(),configFullPath.toLatin1().data());
        if (false == readKscreenConfig(&m_kscreenConfigParam, kscreenConfigName)) {
            setWithoutConfig(&m_kscreenConfigParam);
            SYS_LOG(LOG_DEBUG,"can't open %s's screen config>%s",m_userName.toLatin1().data(),configFullPath.toLatin1().data());
        } else {
            if (m_kscreenConfigParam.m_outputList.count() == 1) {//lenovo LT1953wF 显示器最大分辨率1440x900。是否这类都这样？？
                QSize size = getMaxSize();
                USD_LOG(LOG_DEBUG,".");
                width = m_kscreenConfigParam.m_outputList[0]->property("width").toInt();
                height = m_kscreenConfigParam.m_outputList[0]->property("height").toInt();
                QString name = m_kscreenConfigParam.m_outputList[0]->property("name").toString();
                USD_LOG(LOG_DEBUG,"name%s:%dx%d",name.toLatin1().data(), width, height);
                if (width == 1280 && height == 1024 && size.width() == 1440 && size.height() == 900) {
                    cmd = QString("xrandr --output %1 --auto").arg(name);
//                    QString("xrandr --output %1 --mode 1280x1024").arg(name);
                    QProcess proc;
                    proc.execute(cmd);
                    proc.waitForFinished();
                    m_kscreenConfigParam.m_screenWidth = 1280;
                    m_kscreenConfigParam.m_screenHeight = 1024;
                    USD_LOG(LOG_DEBUG,"cmd:%s",cmd.toLatin1().data());
                    return;
                }
            }
        }
    }

    clearAndGetCrtcs(&m_kscreenConfigParam);
    clearAllCrtc();
//    ret = XGrabServer(m_pDpy);//Grab Xserver is required when setting screen parameters, but the behavior of grab in #151429 causes xserver suspend
//    USD_LOG(LOG_DEBUG,"xGrabServer:%d",ret);
    setCrtcConfig(&m_kscreenConfigParam);
    setScreenSize();
//    ret = XUngrabServer(m_pDpy);
    debugAllOutput(&m_kscreenConfigParam);
    screenInt = DefaultScreen (m_pDpy);
    if ((m_kscreenConfigParam.m_screenWidth != DisplayWidth(m_pDpy, screenInt) ||
            m_kscreenConfigParam.m_screenHeight != DisplayHeight(m_pDpy, screenInt))) {
        USD_LOG(LOG_DEBUG,"had a bug");//for #151429
        setCrtcConfig(&m_kscreenConfigParam);
        setScreenSize();
    }

    if ((m_kscreenConfigParam.m_screenWidth != DisplayWidth(m_pDpy, screenInt) ||
            m_kscreenConfigParam.m_screenHeight != DisplayHeight(m_pDpy, screenInt))){
        USD_LOG(LOG_DEBUG,"screen size reset fail....");
    }
    return ;
}

void SaveScreenParam::disableCrtc()
{
    int tempInt = 0;

    if (false == initXparam()) {
        return;
    }

    for (tempInt = 0; tempInt < m_pScreenRes->ncrtc; tempInt++) {
        int ret = 0;
        ret = XRRSetCrtcConfig (m_pDpy, m_pScreenRes, m_pScreenRes->crtcs[tempInt], CurrentTime,
                                0, 0, None, RR_Rotate_0, NULL, 0);
        if (ret != RRSetConfigSuccess) {
            SYS_LOG(LOG_ERR,"disable crtc:%d error! ");
        }
    }
}

void SaveScreenParam::readConfigAndSetBak()
{
    if (m_MonitoredConfig->lightdmFileExists()) {
        std::unique_ptr<xrandrConfig> monitoredConfig = m_MonitoredConfig->readFile(false);

        if (monitoredConfig == nullptr ) {
            USD_LOG(LOG_DEBUG,"config a error");
            exit(0);
            return;
        }

        m_MonitoredConfig = std::move(monitoredConfig);
        if (m_MonitoredConfig->canBeApplied()) {
            connect(new KScreen::SetConfigOperation(m_MonitoredConfig->data()),
                    &KScreen::SetConfigOperation::finished,
                    this, [this]() {
                USD_LOG(LOG_DEBUG,"set success。。");
                exit(0);
            });
        } else {
            USD_LOG(LOG_ERR,"--|can't be apply|--");
            exit(0);
        }
    } else {
         exit(0);
    }
}

QString SaveScreenParam::printUserConfigParam()
{
    if (false ==initXparam()) {
        exit(0);
    }
    QFile file;
    char *dpi;
    QString configFullPath = getKscreenConfigFullPathInLightDM();
    dpi = XGetDefault(m_pDpy, "Xft", "dpi");
    m_kscreenConfigParam.m_dpi = "96";

    if (dpi != nullptr) {
        m_kscreenConfigParam.m_dpi = QString::fromLatin1(dpi);
    }

    if (!QFile::exists(configFullPath)) {
        setWithoutConfig(&m_kscreenConfigParam);
        debugAllOutput(&m_kscreenConfigParam);
        SYS_LOG(LOG_ERR,"can't open %s's screen config>%s set it without config.",m_userName.toLatin1().data(),configFullPath.toLatin1().data());
    } else {
        if (false == readKscreenConfig(&m_kscreenConfigParam, configFullPath)) {
            setWithoutConfig(&m_kscreenConfigParam);
            SYS_LOG(LOG_ERR,"can't open %s's screen config>%s",m_userName.toLatin1().data(),configFullPath.toLatin1().data());
        }
    }

    return showAllOutputInJson(&m_kscreenConfigParam);
}

void SaveScreenParam::query()
{
    if (false ==initXparam()) {
        exit(0);
    }
    for (int koutput = 0; koutput < m_pScreenRes->noutput; koutput++) {
        XRROutputInfo	*outputInfo = XRRGetOutputInfo (m_pDpy, m_pScreenRes, m_pScreenRes->outputs[koutput]);
        XRRCrtcGamma *outputGamma;
        if (outputInfo->connection != RR_Connected || !outputInfo->crtc) {
            XRRFreeOutputInfo(outputInfo);
            continue;
        }
        outputGamma = XRRGetCrtcGamma(m_pDpy, outputInfo->crtc);
        USD_LOG(LOG_DEBUG,"%s(%d)r:%d,g:%d,b:%d,gamma",outputInfo->name,  outputInfo->crtc,
                *outputGamma->red, *outputGamma->green, *outputGamma->blue);
        XRRFreeGamma(outputGamma);
        XRRFreeOutputInfo(outputInfo);
    }
}

OutputsConfig SaveScreenParam::readOutputConfig()
{

    QString  kscreenConfigName;
    if (false ==initXparam()) {
        exit(0);
    }
    kscreenConfigName = getKscreenConfigFullPathInLightDM();

    if (!QFile::exists(kscreenConfigName)) {
        setWithoutConfig(&m_kscreenConfigParam);
        SYS_LOG(LOG_DEBUG,"can't open %s's screen config>%s set it without config.",m_userName.toLatin1().data(),kscreenConfigName.toLatin1().data());
    } else {
        SYS_LOG(LOG_DEBUG,"ready open %s's screen config>%s",m_userName.toLatin1().data(),kscreenConfigName.toLatin1().data());
        if (false == readKscreenConfig(&m_kscreenConfigParam, kscreenConfigName)) {
            setWithoutConfig(&m_kscreenConfigParam);
            SYS_LOG(LOG_DEBUG,"can't open %s's screen config>%s",m_userName.toLatin1().data(),kscreenConfigName.toLatin1().data());
        }
    }

    return m_kscreenConfigParam;
}

QSize SaveScreenParam::getMaxSize()
{
    QSize size;
    for (int koutput = 0; koutput < m_pScreenRes->noutput; koutput++) {
        XRROutputInfo *outputInfo = XRRGetOutputInfo (m_pDpy, m_pScreenRes, m_pScreenRes->outputs[koutput]);
        if (outputInfo->connection != RR_Connected) {
            XRRFreeOutputInfo(outputInfo);
            continue;
        }
        USD_LOG(LOG_DEBUG,"%s->npreferred:%d",outputInfo->name, outputInfo->npreferred);
        for (int kresMode = 0; kresMode < m_pScreenRes->nmode; kresMode++) {
            XRRModeInfo *resMode = &m_pScreenRes->modes[kresMode];
            USD_LOG(LOG_DEBUG,"%dx%d",resMode->width, resMode->height);
            if (resMode->id == outputInfo->modes[0]) {
                size.setWidth(resMode->width);
                size.setHeight(resMode->height);
                USD_LOG(LOG_DEBUG,"%dx%d",resMode->width, resMode->height);
                break;
            }
        }
        XRRFreeOutputInfo(outputInfo);
    }
    return size;
}


void SaveScreenParam::getRootWindows()
{

}

void SaveScreenParam::getScreen()
{
    //    m_res = XRRGetScreenResourcesCurrent (m_dpy, root);
}

void SaveScreenParam::debugAllOutput(OutputsConfig *outputsConfig)
{
    USD_LOG(LOG_DEBUG,"find %d outputs",outputsConfig->m_outputList.count());
    for (int koutput = 0; koutput < outputsConfig->m_outputList.count(); koutput++) {
        if (outputsConfig->m_outputList[koutput]->property(OUTPUT_ENABLE).toString() == "false") {
            USD_LOG(LOG_DEBUG,"skip %s,disable",outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString().toLatin1().data());
            continue;
        }
        int modeId  = outputsConfig->m_outputList[koutput]->property(MODE_ID).toInt();
        int width = outputsConfig->m_outputList[koutput]->property(OUTPUT_WIDTH).toInt();
        int height = outputsConfig->m_outputList[koutput]->property(OUTPUT_HEIGHT).toInt();
        int x = outputsConfig->m_outputList[koutput]->property("x").toInt();
        int y = outputsConfig->m_outputList[koutput]->property("y").toInt();
        int crtc = outputsConfig->m_outputList[koutput]->property(CRTC_ID).toInt();
        int primary = outputsConfig->m_outputList[koutput]->property(OUTPUT_PRIMARY).toInt();
        double scale = outputsConfig->m_outputList[koutput]->property(OUTPUT_SCALE).toDouble();
        double rate = outputsConfig->m_outputList[koutput]->property(OUTPUT_RATE).toDouble();
        QString outputName = outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString();

        USD_LOG(LOG_DEBUG,"%s: usd mode(%d) %dx%d@%f at(%dx%d) in crtc(%d) scale:%f,primary:%d",outputName.toLatin1().data(), modeId, width, height, rate, x, y,
                crtc, scale,primary);
    }
}

int SaveScreenParam::initXparam()
{
    m_pDpy = XOpenDisplay (m_pDisplayName);
    if (m_pDpy == NULL) {
        SYS_LOG(LOG_DEBUG,"XOpenDisplay fail...");
        return false;
    }

    m_screen = DefaultScreen(m_pDpy);
    if (m_screen >= ScreenCount (m_pDpy)) {
        SYS_LOG(LOG_DEBUG,"Invalid screen number %d (display has %d)",m_screen, ScreenCount(m_pDpy));
        return false;
    }

    m_rootWindow = RootWindow(m_pDpy, m_screen);

    m_pScreenRes = XRRGetScreenResources(m_pDpy, m_rootWindow);
    if (NULL == m_pScreenRes) {
        SYS_LOG(LOG_DEBUG,"could not get screen resources",m_screen, ScreenCount(m_pDpy));
        return false;
    }

    if (m_pScreenRes->noutput == 0) {
        SYS_LOG(LOG_DEBUG, "noutput is 0!!");
        return false;
    }

    SYS_LOG(LOG_DEBUG,"initXparam success");

    for (int k = 0; k < m_pScreenRes->noutput; k++) {
        USD_LOG(LOG_DEBUG,"k:%d --->%d",k, m_pScreenRes->outputs[k]);
    }


    for (int k = 0; k < m_pScreenRes->ncrtc; k++) {
        USD_LOG(LOG_DEBUG,"crtc:%d --->%d",k, m_pScreenRes->crtcs[k]);
    }

    return true;
}

void SaveScreenParam::clearAndGetCrtcs(OutputsConfig *outputsConfig)
{
    QList<int> hadUseCrtcList;
    bool isNeedClearCrtcs = false;
    for (int koutput = 0; koutput < outputsConfig->m_outputList.count(); koutput++) {
        double scale = outputsConfig->m_outputList[koutput]->property(OUTPUT_SCALE).toDouble();
        for (int tempInt = 0; tempInt < m_pScreenRes->noutput; tempInt++) {
            XRROutputInfo	*outputInfo = XRRGetOutputInfo (m_pDpy, m_pScreenRes, m_pScreenRes->outputs[tempInt]);
            QString outputName = QString::fromLatin1(outputInfo->name);
            if (outputInfo->connection != RR_Connected ||
                    outputName != outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString()) {
                XRRFreeOutputInfo(outputInfo);
                continue;
            }
            if (outputsConfig->m_outputList[koutput]->property(OUTPUT_ENABLE).toString() == "false") {
                int ret;
                ret = XRRSetCrtcConfig (m_pDpy, m_pScreenRes, outputInfo->crtc, CurrentTime,
                                        0, 0, None, RR_Rotate_0, NULL, 0);
                if (ret != RRSetConfigSuccess) {
                    USD_LOG(LOG_ERR,"clear %s RRSetConfigFail cuz disable..",outputInfo->name);
                } else {
                    USD_LOG(LOG_DEBUG,"clear %s RRSetConfigSuccess cuz disable",outputInfo->name);
                }
                continue;
            }
            outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_ID, (int)m_pScreenRes->outputs[tempInt]);
            if (0 != outputInfo->crtc) {
                hadUseCrtcList.append(outputInfo->crtc);
                outputsConfig->m_outputList[koutput]->setProperty(CRTC_ID, (int)outputInfo->crtc);
                if (checkTransformat(outputInfo->crtc, scale)) {
                    clearCrtc(outputInfo->crtc);
                    outputsConfig->m_outputList[koutput]->setProperty(TRANSFORM_CHANGED, 1);
                }

                XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(m_pDpy, m_pScreenRes, (int)outputInfo->crtc);


                USD_LOG(LOG_DEBUG,"keep %s to crtcId %d rotatison:%d:%d",
                        outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString().toLatin1().data(),
                        (int)outputInfo->crtc, crtcInfo->rotation, outputsConfig->m_outputList[koutput]->property(OUTPUT_ROTATION).toInt());

                if(crtcInfo->rotation != outputsConfig->m_outputList[koutput]->property(OUTPUT_ROTATION).toInt()) {
                    outputsConfig->m_outputList[koutput]->setProperty(OUTPUT_CLEAR, true);
                }
                XRRFreeCrtcInfo(crtcInfo);
            } else {
     /*           if (m_pScreenRes->ncrtc == tempInt) {
                    USD_LOG(LOG_DEBUG,"ncrtc:%d, crtc:%d, tempInt:%d",m_pScreenRes->ncrtc, m_pScreenRes->crtcs[koutput], tempInt);
                    outputsConfig->m_outputList[koutput]->setProperty(CRTC_ID, (int)m_pScreenRes->crtcs[koutput]);
                } else*/
                {
                    int diff = 99999;
                    int crtcId = 0;
                    int outputSpace = outputsConfig->m_outputList[koutput]->property(OUTPUT_SPACE).toInt();
                    //0,1,2,3,4;;;;0,1,2,3,
                    if (outputSpace < m_pScreenRes->ncrtc) {
                        crtcId = m_pScreenRes->crtcs[outputSpace];
                        USD_LOG(LOG_DEBUG,"get changed crtc from 0 to crtcId %d", crtcId);
                    } else {
                        for (int kcrtc = 0; kcrtc < m_pScreenRes->ncrtc; kcrtc++) {
                            XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(m_pDpy, m_pScreenRes, m_pScreenRes->crtcs[kcrtc]);
                            if (0 == crtcInfo->noutput) {
                                if (!hadUseCrtcList.contains(m_pScreenRes->crtcs[kcrtc])) {
                                    if (m_pScreenRes->outputs[tempInt] - m_pScreenRes->crtcs[kcrtc]<diff) {
                                        crtcId = m_pScreenRes->crtcs[kcrtc];
                                        diff = m_pScreenRes->outputs[tempInt] - m_pScreenRes->crtcs[kcrtc];//处理usb screen
                                    }
                                }
                            }
                            XRRFreeCrtcInfo(crtcInfo);
                        }
                    }
                    hadUseCrtcList.append(crtcId);
                    outputsConfig->m_outputList[koutput]->setProperty(CRTC_ID, crtcId);
                    USD_LOG(LOG_DEBUG,"changed %s crtc(%d) to crtcId %d",
                            outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME).toString().toLatin1().data(),
                            outputSpace,
                            crtcId);
                }
            }
            XRRFreeOutputInfo(outputInfo);
        }
    }
}

bool SaveScreenParam::checkTransformat(RRCrtc crtc, double scale)
{
    XRRCrtcTransformAttributes  *attr;
    XTransform  transform;
    if (XRRGetCrtcTransform (m_pDpy, crtc, &attr) && attr) {

        for (int x = 0; x < 3; x++) {
            for (int x1 = 0; x1 < 3; x1++) {
                transform.matrix[x][x1] = 0;
            }
        }

        transform.matrix[0][0] = XDoubleToFixed(1.0)/scale;
        transform.matrix[1][1] = XDoubleToFixed(1.0)/scale;
        transform.matrix[2][2] = XDoubleToFixed(1.0);
        XFree (attr);

        USD_LOG(LOG_DEBUG,"%d:%d,%d,%d,%d,%d,%d", crtc, transform.matrix[0][0], transform.matrix[1][1]
                ,transform.matrix[2][2],attr->currentTransform.matrix[0][0],attr->currentTransform.matrix[1][1],attr->currentTransform.matrix[2][2]);

        if (attr->currentTransform.matrix[0][0] == transform.matrix[0][0] &&
                attr->currentTransform.matrix[1][1] == transform.matrix[1][1] &&
                attr->currentTransform.matrix[2][2] == transform.matrix[2][2]) {
            return false;
        }

    }

    return true;
}

bool SaveScreenParam::clearCrtc(RRCrtc crtc)
{
    int ret;
    ret = XRRSetCrtcConfig (m_pDpy, m_pScreenRes, crtc, CurrentTime,
                            0, 0, None, RR_Rotate_0, NULL, 0);
    if (ret != RRSetConfigSuccess) {
        SYS_LOG(LOG_ERR,"disable crtc:%d error! ");
        return false;
    }
    USD_LOG(LOG_DEBUG,"CRTC:%d had been clear!", crtc);
    return true;
}

bool SaveScreenParam::clearAllCrtc()
{
     for (int klist = 0; klist < m_kscreenConfigParam.m_outputList.count(); klist++) {
         if(m_kscreenConfigParam.m_outputList[klist]->property(OUTPUT_CLEAR).toInt()) {
            clearCrtc(m_kscreenConfigParam.m_outputList[klist]->property(CRTC_ID).toInt());
         }
     }
     return true;
}

void SaveScreenParam::setCrtcConfig(OutputsConfig *outputsConfig)
{
    QList<int> hadUsedCrtc;
    for (int klist = 0; klist < outputsConfig->m_outputList.count(); klist++) {

        for (int tempInt = 0; tempInt < m_pScreenRes->noutput; tempInt++) {
            RRMode modeID;
            XRROutputInfo	*outputInfo = XRRGetOutputInfo (m_pDpy, m_pScreenRes, m_pScreenRes->outputs[tempInt]);
            QString outputName = QString::fromLatin1(outputInfo->name);
            if (outputInfo->connection != RR_Connected ||
                    outputName != outputsConfig->m_outputList[klist]->property(OUTPUT_NAME).toString()) {
                XRRFreeOutputInfo(outputInfo);
                continue;
            }
             USD_LOG(LOG_DEBUG,"ready set %s",outputInfo->name);
            if (outputsConfig->m_outputList[klist]->property(OUTPUT_ENABLE).toString() == "false") {
                continue;
            }

            if (outputsConfig->m_outputList[klist]->property(MODE_ID).toInt() == 0) {
                modeID = getModeId(outputInfo,  outputsConfig->m_outputList[klist]);
                outputsConfig->m_outputList[klist]->setProperty(MODE_ID, (int)modeID);
                USD_LOG(LOG_DEBUG,"ready set %s mode:%d",outputInfo->name,modeID);
            } else {
                modeID = outputsConfig->m_outputList[klist]->property(MODE_ID).toInt();
            }

            int x = outputsConfig->m_outputList[klist]->property("x").toInt();
            int y =  outputsConfig->m_outputList[klist]->property("y").toInt();
            if (outputsConfig->m_outputList.count() == 1) {
                x = 0;
                y = 0;
                USD_LOG(LOG_DEBUG,"change x,y to 0,0");
                XRRSetOutputPrimary(m_pDpy, m_rootWindow, m_pScreenRes->outputs[tempInt]);
            }
            int crtc = outputsConfig->m_outputList[klist]->property(CRTC_ID).toInt();
            int outputId =  outputsConfig->m_outputList[klist]->property(OUTPUT_ID).toInt();
            int rotationAngle = outputsConfig->m_outputList[klist]->property(OUTPUT_ROTATION).toInt();
            int transformatchanged = outputsConfig->m_outputList[klist]->property(TRANSFORM_CHANGED).toInt();
            double scale = outputsConfig->m_outputList[klist]->property(OUTPUT_SCALE).toDouble();

            int isPrimary = outputsConfig->m_outputList[klist]->property(OUTPUT_PRIMARY).toInt();

            if (isPrimary) {
                XRRSetOutputPrimary(m_pDpy, m_rootWindow, outputId);
            }

            if (transformatchanged) {
                XRRCrtcTransformAttributes  *attr;
                XTransform  transform;
                char *filter;
                XFixed *xfixed;
                if (XRRGetCrtcTransform (m_pDpy,crtc, &attr) && attr) {
                    for (int x = 0; x < 3; x++) {
                        for (int x1 = 0; x1 < 3; x1++) {
                            transform.matrix[x][x1] = 0;
                        }
                    }
                    transform.matrix[0][0] = XDoubleToFixed(1.0)/scale;
                    transform.matrix[1][1] = XDoubleToFixed(1.0)/scale;
                    transform.matrix[2][2] = XDoubleToFixed (1.0);

                    if (scale == 1.0) {
                           filter = "nearest";
                    } else {
                           filter = "bilinear";
                    }
                    USD_LOG(LOG_DEBUG,"%f,%d,%d,%d",scale,
                            transform.matrix[0][0],transform.matrix[1][1],
                            transform.matrix[2][2]);
                    XRRSetCrtcTransform(m_pDpy, crtc, &transform, filter, xfixed, 0);
                    XFree (attr);
                }
            }

            int ret = XRRSetCrtcConfig (m_pDpy, m_pScreenRes, crtc, CurrentTime,
                                    x, y, modeID, rotationAngle, (RROutput*)&outputId, 1);

            if (ret != RRSetConfigSuccess) {
                for (int resetCount = 0; resetCount < m_pScreenRes->ncrtc; resetCount++) {
                    XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(m_pDpy, m_pScreenRes, m_pScreenRes->crtcs[resetCount]);
                    if (crtcInfo->noutput == 0) {
                        crtc = m_pScreenRes->crtcs[resetCount];
                        int resetRet = XRRSetCrtcConfig (m_pDpy, m_pScreenRes, crtc, CurrentTime,
                                                         x, y, modeID, rotationAngle, (RROutput*)&outputId, 1);

                        if (resetRet != RRSetConfigSuccess) {
                             USD_LOG(LOG_ERR,"%s RRSetConfigFail：%d..",m_kscreenConfigParam.m_outputList[klist]->property(OUTPUT_NAME).toString().toLatin1().data()
                                     ,crtc);
                        } else {
                            XRRFreeCrtcInfo(crtcInfo);
                            USD_LOG(LOG_ERR,"%s RRSetConfigSuccess：%d..",m_kscreenConfigParam.m_outputList[klist]->property(OUTPUT_NAME).toString().toLatin1().data()
                                    ,crtc);
                            hadUsedCrtc.append(crtc);
                            USD_LOG(LOG_DEBUG,"%s(%d) usd mode:%d at %dx%d rotate:%d crtc:%d",outputInfo->name, outputId, modeID, x, y, rotationAngle,crtc);
                            break;
                        }
                    }
                    XRRFreeCrtcInfo(crtcInfo);
                 }
            } else {
                USD_LOG(LOG_DEBUG,"%s RRSetConfigSuccess",m_kscreenConfigParam.m_outputList[klist]->property(OUTPUT_NAME).toString().toLatin1().data());
                hadUsedCrtc.append(crtc);
            }
            XRRFreeOutputInfo(outputInfo);
        }
    }
}

QString SaveScreenParam::showAllOutputInJson(OutputsConfig *outputsConfig)
{
    QJsonArray arrary;
    QJsonDocument jdoc;

    for (int j = 0; j < outputsConfig->m_outputList.count(); j++) {
        QJsonObject sub;
        QString name = outputsConfig->m_outputList[j]->property(OUTPUT_NAME).toString();
        int width = outputsConfig->m_outputList[j]->property(OUTPUT_WIDTH).toInt();
        int height = outputsConfig->m_outputList[j]->property(OUTPUT_HEIGHT).toInt();
        int x = outputsConfig->m_outputList[j]->property("x").toInt();
        int y = outputsConfig->m_outputList[j]->property("y").toInt();
        double scale = outputsConfig->m_outputList[j]->property(OUTPUT_SCALE).toDouble();
        double rate = outputsConfig->m_outputList[j]->property(OUTPUT_RATE).toDouble();

        sub.insert("name",name);
        sub.insert("width",width);
        sub.insert("height",height);
        sub.insert("rate",rate);
        sub.insert("x",x);
        sub.insert("x",y);
        sub.insert("scale", scale);
        arrary.append(sub);

    }

    jdoc.setArray(arrary);
    return QString::fromLatin1(jdoc.toJson());
}

QString SaveScreenParam::getKscreenConfigFullPathInLightDM()
{
    int             actualFormat;
    int             tempInt;

    uchar           *prop;

    Atom            tempAtom;
    Atom            actualType;

    ulong           nitems;
    ulong           bytesAfter;
    QStringList     outputsHashList;

    tempAtom = XInternAtom (m_pDpy, "EDID", false);
    for (tempInt = 0; tempInt < m_pScreenRes->noutput; tempInt++) {
       OutputHashDictionary dic;
       XRRGetOutputProperty(m_pDpy, m_pScreenRes->outputs[tempInt], tempAtom,
                 0, 100, False, False,
                 AnyPropertyType,
                 &actualType, &actualFormat,
                 &nitems, &bytesAfter, &prop);

       QCryptographicHash hash(QCryptographicHash::Md5);
       XRROutputInfo	*outputInfo = XRRGetOutputInfo (m_pDpy, m_pScreenRes, m_pScreenRes->outputs[tempInt]);
       if (nitems == 0) {
           if (RR_Connected == outputInfo->connection) {
               USD_LOG(LOG_DEBUG,"%s %d edid is empty",outputInfo->name,outputInfo->connection);
               QString checksum = QString::fromLatin1(outputInfo->name);
               if (outputsHashList.contains(checksum)) {
                   m_isSameHash = true;
               }
               outputsHashList.append(checksum);
               dic.hash = checksum;
               dic.name = QString::fromLatin1(outputInfo->name);
               m_outputHashDic.append(dic);
               XRRFreeOutputInfo(outputInfo);
           }
           continue;
       }


       hash.addData(reinterpret_cast<const char *>(prop), nitems);
       QString checksum = QString::fromLatin1(hash.result().toHex());
       if (outputsHashList.contains(checksum)) {
           m_isSameHash = true;
       }
       outputsHashList.append(checksum);
       dic.hash = checksum;
       dic.name = QString::fromLocal8Bit(outputInfo->name);
       m_outputHashDic.append(dic);
       XRRFreeOutputInfo(outputInfo);
    }

    if (outputsHashList.count() == 0) {
        SYS_LOG(LOG_DEBUG,"outputsHashList is empty");
        return "";
    }
    std::sort(outputsHashList.begin(), outputsHashList.end());
    const auto configHash = QCryptographicHash::hash(outputsHashList.join(QString()).toLatin1(),
                                               QCryptographicHash::Md5);

    m_KscreenConfigFile = QString::fromLatin1(configHash.toHex());

    if (m_userName.isEmpty()) {
        return "";
    }

    return QString("/var/lib/lightdm-data/%1/usd/kscreen/%2").arg(m_userName).arg(QString::fromLatin1(configHash.toHex()));
}

bool SaveScreenParam::isUsdScale()
{
    QString dirName = QString("/var/lib/lightdm-data/%1/usd/kscreen/").arg(m_userName);

    QStringList kscreenConfigList;
    QDir kscreenDir(dirName);

    QFileInfoList kscreenConfigInfoList = kscreenDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    Q_FOREACH (auto kscreenConfig, kscreenConfigInfoList) {
        if (getScaleStatus(kscreenConfig.absoluteFilePath())) {
            return true;
        }
    }
    return false;
}

bool SaveScreenParam::getScaleStatus(QString str)
{

    return false;
}

double SaveScreenParam::getDefautScaleFactor(OutputsConfig *outputsConfig)
{
    double defaultScale = 999.0;

    for (int k = 0; k < outputsConfig->m_outputList.count(); k++) {
        double calScale = 0;
        int width = outputsConfig->m_outputList[k]->property(OUTPUT_WIDTH).toInt();
        int height = outputsConfig->m_outputList[k]->property(OUTPUT_HEIGHT).toInt();

        if (width <= 1920 && height <= 1080) {
            calScale = 1;
        } else if (width <= 2560 && height <=1500) {
            calScale = 1.5;
        } else {
            calScale = 2;
        }

        defaultScale = defaultScale > calScale ?  calScale : defaultScale;
    }

    return defaultScale == 999.0 ? 1 : defaultScale;
}

//所有显示器的模式都放在一起，需要甄别出需要的显示器，避免A取到B的模式。需要判断那个显示器支持那些模式
RRMode SaveScreenParam::getModeId(XRROutputInfo	*outputInfo,UsdOuputProperty *kscreenOutputParam)
{
    double rate;
    double vTotal;

    RRMode ret = 0;

    for (int m = 0; m < m_pScreenRes->nmode; m++) {
        XRRModeInfo *mode = &m_pScreenRes->modes[m];
        vTotal = mode->vTotal;
//        SYS_LOG(LOG_DEBUG,"start check mode:%s id:%d for %s", mode->name, mode->id, outputInfo->name);
        if (mode->modeFlags & RR_DoubleScan) {
            /* doublescan doubles the number of lines */
            vTotal *= 2;
        }

        if (mode->modeFlags & RR_Interlace) {
            /* interlace splits the frame into two fields */
            /* the field rate is what is typically reported by monitors */
            vTotal /= 2;
        }
        rate = mode->dotClock / ((double) mode->hTotal * (double) vTotal);
        if (mode->width == kscreenOutputParam->property(OUTPUT_WIDTH).toUInt()
                && mode->height == kscreenOutputParam->property(OUTPUT_HEIGHT).toUInt()) {
            double kscreenRate = kscreenOutputParam->property(OUTPUT_RATE).toDouble();
            if (qAbs(kscreenRate - rate) < 0.02) {
                for (int k = 0; k< outputInfo->nmode; k++) {
                    if (outputInfo->modes[k] == mode->id) {
//                        SYS_LOG(LOG_DEBUG,"find %s mode:%s id:%d refresh:%f",outputInfo->name, mode->name, mode->id, rate);
                        return mode->id;
                    }
                }
            } else {
//                SYS_LOG(LOG_DEBUG,"%dx%d %f!=%f",mode->width,mode->height,rate,kscreenRate);
            }
        }
    }

    return ret;
}

bool SaveScreenParam::readKscreenConfig(OutputsConfig *outputsConfig, QString configFullPath)
{
    QFile file;
    QJsonDocument parser;
    QVariantList outputsInfo;

    file.setFileName(configFullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    outputsInfo = parser.fromJson(file.readAll()).toVariant().toList();
    for (const auto &variantInfo : outputsInfo) {
        UsdOuputProperty *outputProperty;
        outputProperty = new UsdOuputProperty();

        const QVariantMap info = variantInfo.toMap();
        const QVariantMap posInfo = info[QStringLiteral("pos")].toMap();
        const QVariantMap mateDataInfo = info[QStringLiteral("metadata")].toMap();
        const QVariantMap modeInfo = info[QStringLiteral("mode")].toMap();//接入双屏，关闭一屏幕时的处理？
        const QVariantMap sizeInfo = modeInfo[QStringLiteral("size")].toMap();
        USD_LOG(LOG_DEBUG, "find:%s,%d",mateDataInfo[QStringLiteral("name")].toString().toLatin1().data(),modeInfo.count());
        outputProperty->setProperty(OUTPUT_ENABLE, info[QStringLiteral("enabled")].toString());
        outputProperty->setProperty(OUTPUT_NAME, mateDataInfo[QStringLiteral("name")].toString());

        if (modeInfo.count() == 0) {
            outputsConfig->m_outputList.append(outputProperty);
            continue;
        }
        double scale = 0;
        int dpi = 0.0;
        bool dpiOK = 0;

        outputProperty->setProperty("x", posInfo[QStringLiteral("x")].toString());
        outputProperty->setProperty("y", posInfo[QStringLiteral("y")].toString());
        outputProperty->setProperty(OUTPUT_WIDTH, sizeInfo[QStringLiteral("width")].toInt());
        outputProperty->setProperty(OUTPUT_HEIGHT, sizeInfo[QStringLiteral("height")].toInt());

        outputProperty->setProperty(OUTPUT_PRIMARY, info[QStringLiteral("primary")].toInt());
        outputProperty->setProperty(OUTPUT_ROTATION, info[QStringLiteral("rotation")].toString());
        outputProperty->setProperty(OUTPUT_RATE, modeInfo[QStringLiteral("refresh")].toDouble());
        outputProperty->setProperty(OUTPUT_ENABLE, info[QStringLiteral("enabled")].toString());
        outputProperty->setProperty(OUTPUT_ID, info[QStringLiteral("id")].toString());
        outputProperty->setProperty("seted", false);
        if (!m_isSameHash) {
            for (const auto dic : m_outputHashDic) {
                if (dic.hash == info[QStringLiteral("id")].toString() && dic.name != mateDataInfo[QStringLiteral("name")].toString()) {
                    USD_LOG(LOG_DEBUG,"change name %s to %s", mateDataInfo[QStringLiteral("name")].toString().toLatin1().data(), dic.name.toLatin1().data());
                    outputProperty->setProperty(OUTPUT_NAME, dic.name);//把name改为与当前的hash值一致的设备名，避免usd屏幕插拔重启后名称改变。
                }
            }
        }

        outputProperty->setProperty(OUTPUT_SCALE, 1.0/*info[QStringLiteral("scale")].toDouble()*/);
        dpi = info.value(QStringLiteral("dpi"), 1).toInt(&dpiOK);
        scale = outputProperty->getscale();

//        if (scale == 1.0 && outputsConfig->m_dpi != "96" && !dpiOK) {//旧配置文件，没有dpi信息写入
//            scale = 0.5;
//            outputProperty->setProperty(OUTPUT_SCALE, 0.5);//存在A,B,C组合显示器，在组合C时设置了缩放导致dpi与后期无法匹配。
//            outputProperty->setProperty("x",  outputProperty->property("x").toInt()/0.5);
//            outputProperty->setProperty("y", outputProperty->property("y").toInt()/0.5);
//            outputProperty->setProperty(OUTPUT_WIDTH, outputProperty->property(OUTPUT_WIDTH));
//            outputProperty->setProperty(OUTPUT_HEIGHT, outputProperty->property(OUTPUT_HEIGHT));
//        } else if (scale == 1.0 && outputsConfig->m_dpi != "96" && dpiOK) {//新配置文件，有dpi信息写入
//            if (dpi == 96) {//实际dpi为192，配置文件为96，需要调整
//                scale = 0.5;
//                outputProperty->setProperty(OUTPUT_SCALE, 0.5);//存在A,B,C组合显示器，在组合C时设置了缩放导致dpi与后期无法匹配。
//                outputProperty->setProperty("x",  outputProperty->property("x").toInt()/0.5);
//                outputProperty->setProperty("y", outputProperty->property("y").toInt()/0.5);
//                outputProperty->setProperty(OUTPUT_WIDTH, outputProperty->property(OUTPUT_WIDTH));
//                outputProperty->setProperty(OUTPUT_HEIGHT, outputProperty->property(OUTPUT_HEIGHT));
//            }
//        }

        if (info[QStringLiteral("enabled")].toString() == "true") {
            if (outputProperty->property(OUTPUT_ROTATION).toString() == "8" ||
                    outputProperty->property(OUTPUT_ROTATION).toString() == "2") {

                if (outputProperty->property(OUTPUT_HEIGHT).toInt()*(1/scale) + outputProperty->property("x").toInt() > outputsConfig->m_screenWidth) {
                    outputsConfig->m_screenWidth = outputProperty->property(OUTPUT_HEIGHT).toInt()*(1/scale) + outputProperty->property("x").toInt();
                }

                if (outputProperty->property(OUTPUT_WIDTH).toInt()*(1/scale) + outputProperty->property("y").toInt() > outputsConfig->m_screenHeight) {
                    outputsConfig->m_screenHeight = outputProperty->property(OUTPUT_WIDTH).toInt()*(1/scale) + outputProperty->property("y").toInt();
                }
            }
            else{
                if (outputProperty->property(OUTPUT_WIDTH).toInt()*(1/scale) + outputProperty->property("x").toInt() > outputsConfig->m_screenWidth) {
                    outputsConfig->m_screenWidth = outputProperty->property(OUTPUT_WIDTH).toInt()*(1/scale) + outputProperty->property("x").toInt();
                }

                if (outputProperty->property(OUTPUT_HEIGHT).toInt()*(1/scale) + outputProperty->property("y").toInt() > outputsConfig->m_screenHeight) {
                    outputsConfig->m_screenHeight = outputProperty->property(OUTPUT_HEIGHT).toInt()*(1/scale) + outputProperty->property("y").toInt();
                }
            }
            outputsConfig->m_outputList.append(outputProperty);
        } else {
//            USD_LOG_SHOW_PARAMS(outputProperty->property(OUTPUT_ENABLE).toString().toLatin1().data());
        }
    }

        debugAllOutput(&m_kscreenConfigParam);

//    for (int tempInt = 0; tempInt < m_pScreenRes->noutput; tempInt++) {
//        XRROutputInfo	*outputInfo = XRRGetOutputInfo (m_pDpy, m_pScreenRes, m_pScreenRes->outputs[tempInt]);
//        QString outputName = QString::fromLatin1(outputInfo->name);
//        bool isOutputInConfig = false;
//        for (int koutput = 0; koutput < outputsConfig->m_outputList.count(); koutput++) {
//            if (outputName == outputsConfig->m_outputList[koutput]->property(OUTPUT_NAME)) {
//                isOutputInConfig = true;
//                break;
//            }
//        }

//        if(!isOutputInConfig) {//配置文件中不存在
//            UsdOuputProperty *outputProperty;
//            outputProperty = new UsdOuputProperty();
//            outputProperty->setProperty(OUTPUT_NAME, outputName);
//            outputProperty->setProperty(CRTC_ID, 0);
//            outputsConfig->m_outputList.append(outputProperty);
//        }
//        XRRFreeOutputInfo(outputInfo);
//    }



    return true;
}






