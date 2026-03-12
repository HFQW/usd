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
#include "volumewindow.h"
#include "ui_volumewindow.h"
#include <QPalette>
#include <QSize>
#include <QRect>
#include <QScreen>
#include <QX11Info>
#include <QDebug>
#include <QPainter>
#include <QBitmap>
#include <KWindowEffects>
#include <QPainterPath>
#include <QPainter>
#include <QGSettings/qgsettings.h>
#include "clib-syslog.h"
#include "usd_global_define.h"

#include "ukuistylehelper/ukuistylehelper.h"
#include "windowmanager/windowmanager.h"
#define DBUS_NAME       DBUS_XRANDR_NAME
#define DBUS_PATH       DBUS_XRANDR_PATH
#define DBUS_INTERFACE  DBUS_XRANDR_INTERFACE

#define QT_THEME_SCHEMA   "org.ukui.style"
#define ICON_SIZE 24

#define UKUI_SOUND "org.ukui.sound"
#define VOLUME_INCREASED "volume-increase"
#define VOLUME_INCREASED_VALUE "volume-increase-value"
#define MAX_VOLUME_NORMAL 100
#define MAX_VOLUM_INCREASED 125

const QString allIconName[] = {
    "audio-volume-muted-symbolic",
    "audio-volume-low-symbolic",
    "audio-volume-medium-symbolic",
    "audio-volume-high-symbolic",
    nullptr
};

VolumeWindow::VolumeWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VolumeWindow)
{
    ui->setupUi(this);
    initWindowInfo();
    initSoundSettings();
    mDbusXrandInter = new QDBusInterface(DBUS_NAME,
                                         DBUS_PATH,
                                         DBUS_INTERFACE,
                                         QDBusConnection::sessionBus(), this);
     if (!mDbusXrandInter->isValid()) {
        USD_LOG(LOG_DEBUG, "stderr:%s\n",qPrintable(QDBusConnection::sessionBus().lastError().message()));
    }
    //监听dbus变化  更改主屏幕时，会进行信号发送
//    connect(mDbusXrandInter, SIGNAL(screenPrimaryChanged(int,int,int,int)),
//            this, SLOT(priScreenChanged(int,int,int,int)));
    m_styleSettings = new QGSettings(QT_THEME_SCHEMA);
    connect(m_styleSettings,SIGNAL(changed(const QString&)),
            this,SLOT(onStyleChanged(const QString&)));
}

VolumeWindow::~VolumeWindow()
{
    delete ui;
    if (mBut)
        delete mBut;
    if (mVolumeBar)
        delete mVolumeBar;
    if (mTimer)
        delete mTimer;
}

int VolumeWindow::getScreenGeometry(QString methodName)
{
    int res = 0;
    QDBusMessage message = QDBusMessage::createMethodCall(DBUS_NAME,
                                                          DBUS_PATH,
                                                          DBUS_INTERFACE,
                                                          methodName);
    QDBusMessage response = QDBusConnection::sessionBus().call(message);
    if (response.type() == QDBusMessage::ReplyMessage)
    {
        if(response.arguments().isEmpty() == false) {
            int value = response.arguments().takeFirst().toInt();
            res = value;
        }
    } else {
        USD_LOG(LOG_DEBUG, "%s called failed", methodName.toLatin1().data());
    }
    return res;
}

/* 主屏幕变化监听函数 */
void VolumeWindow::priScreenChanged(int x, int y, int width, int height)
{
    int ax,ay;
    ax = x + (width * 0.01);
    ay = y + (width * 0.04);
    move(ax,ay);
    kdk::WindowManager::setGeometry(this->windowHandle(),QRect(ax,ay,this->width(),this->height()));
}


void VolumeWindow::geometryChangedHandle()
{
    int x = QApplication::primaryScreen()->geometry().x();
    int y = QApplication::primaryScreen()->geometry().y();
    int width = QApplication::primaryScreen()->size().width();
    int height = QApplication::primaryScreen()->size().height();

    priScreenChanged(x,y,width,height);
}

void VolumeWindow::onStyleChanged(const QString& style)
{
    if(style == "icon-theme-name") {
        QSize iconSize(ICON_SIZE,ICON_SIZE);
        QIcon::setThemeName(m_styleSettings->get("icon-theme-name").toString());
        mBut->setPixmap(drawLightColoredPixmap((QIcon::fromTheme(mIconName).pixmap(iconSize))
                                               ,m_styleSettings->get("style-name").toString()));
    } else if(style == "style-name") {
        if(!this->isHidden()) {
            hide();
            show();
        }
    }
}

void VolumeWindow::volumeIncreased(const QString &key)
{
    if (key == QStringLiteral(VOLUME_INCREASED)) {
        bool value = m_soundSettings->get(VOLUME_INCREASED).toBool();
        if (value) {
            if (m_soundSettings->keys().contains(QStringLiteral(VOLUME_INCREASED_VALUE))) {
                m_maxVolume = m_soundSettings->get(VOLUME_INCREASED_VALUE).toInt();
            } else {
                m_maxVolume = MAX_VOLUM_INCREASED;
            }
        } else {
            m_maxVolume = MAX_VOLUME_NORMAL;
            if (mVolume > m_maxVolume) {
                setVolumeLevel(m_maxVolume);
            }
        }
        setVolumeRange(m_maxVolume);
    }
}


void VolumeWindow::initSoundSettings()
{
    if(QGSettings::isSchemaInstalled(UKUI_SOUND)){
        m_soundSettings = new QGSettings(UKUI_SOUND);
        volumeIncreased(VOLUME_INCREASED);
        connect(m_soundSettings,SIGNAL(changed(const QString&)),this,SLOT(volumeIncreased(const QString&)),Qt::DirectConnection);
    }
}


void VolumeWindow::initWindowInfo()
{

    connect(QApplication::primaryScreen(), &QScreen::geometryChanged, this, &VolumeWindow::geometryChangedHandle);
    connect(static_cast<QApplication *>(QCoreApplication::instance()),
              &QApplication::primaryScreenChanged, this, &VolumeWindow::geometryChangedHandle);

    //窗口性质
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);

    setAttribute(Qt::WA_TranslucentBackground, true);

    setFixedSize(QSize(84,320));

    m_frame = new QFrame(this);
    mVolumeBar = new QProgressBar(m_frame);
    mVolumeBar->setProperty("needTranslucent", true);
    mBrightBar = new QProgressBar(m_frame);
    mBrightBar->setProperty("needTranslucent", true);
    mBut = new QLabel(m_frame);

    mTimer = new QTimer();
    connect(mTimer,SIGNAL(timeout()),this,SLOT(timeoutHandle()));

    geometryChangedHandle();//had move action
    setWidgetLayout();

    mVolume = 0;
    mVolumeMuted = false;
}

//上下留出10个空间,音量条与svg图片之间留出10个空间
void VolumeWindow::setWidgetLayout()
{
    //窗口性质
    m_frame->setFixedSize(QSize(64,300));
    m_frame->move(10,10);
    //button图片操作
    mBut->setFixedSize(QSize(31,24));
    mBut->setAlignment(Qt::AlignCenter);
    mBut->move(17,253);
    //音量条操作
    mVolumeBar->setOrientation(Qt::Vertical);
    mVolumeBar->setFixedSize(QSize(6,200));
    mVolumeBar->move(29,37);
    mVolumeBar->setTextVisible(false);
    mVolumeBar->hide();
    //亮度条操作
    mBrightBar->setOrientation(Qt::Vertical);
    mBrightBar->setFixedSize(QSize(6,200));
    mBrightBar->move(29,37);
    mBrightBar->setTextVisible(false);
    mBrightBar->hide();
}

int doubleToInt(double d)
{
    int I = d;
    if(d - I >= 0.5)
        return I+1;
    else
        return I;
}

QPixmap VolumeWindow::drawLightColoredPixmap(const QPixmap &source, const QString &style)
{

    int value = 255;
    if(style == "ukui-light")
    {
        value = 0;
    }

    QColor gray(255,255,255);
    QColor standard (0,0,0);
    QImage img = source.toImage();
    for (int x = 0; x < img.width(); x++) {
        for (int y = 0; y < img.height(); y++) {
            auto color = img.pixelColor(x, y);
            if (color.alpha() > 0) {
                if (qAbs(color.red()-gray.red())<20 && qAbs(color.green()-gray.green())<20 && qAbs(color.blue()-gray.blue())<20) {
                    color.setRed(value);
                    color.setGreen(value);
                    color.setBlue(value);
                    img.setPixelColor(x, y, color);
                }
                else {
                    color.setRed(value);
                    color.setGreen(value);
                    color.setBlue(value);
                    img.setPixelColor(x, y, color);
                }
            }
        }
    }
    return QPixmap::fromImage(img);
}

void VolumeWindow::dialogVolumeShow()
{
    show();
    geometryChangedHandle();
    mBrightBar->hide();
    mVolumeBar->show();
    mTimer->start(3000);
}

void VolumeWindow::dialogBrightShow()
{
    show();
    geometryChangedHandle();
    mVolumeBar->hide();
    mBrightBar->show();
    mTimer->start(3000);
}

void VolumeWindow::setVolumeMuted(bool muted)
{
    if(mVolumeMuted != muted) {
        mVolumeMuted = muted;
    }
    updateVolumeIcon();
}

void VolumeWindow::updateVolumeIcon()
{
    if (mVolumeMuted) {
        mIconName = allIconName[0];
    } else {
        qreal percentage = mVolume / 100.00;
        if(0 <= percentage && percentage < 0.01)
            mIconName = allIconName[0];
        else if(percentage <= 0.33)
            mIconName = allIconName[1];
        else if(percentage <= 0.66)
            mIconName = allIconName[2];
        else
            mIconName = allIconName[3];
    }
    QSize iconSize(ICON_SIZE,ICON_SIZE);
    mBut->setPixmap(drawLightColoredPixmap((QIcon::fromTheme(mIconName).pixmap(iconSize)),m_styleSettings->get("style-name").toString()));
}

void VolumeWindow::setVolumeLevel(int level)
{
    if(mVolume != level){
        mVolume = level;
        mVolumeBar->setValue(mVolume);
        updateVolumeIcon();
    }
}

void VolumeWindow::setVolumeRange(int maxValue)
{
    mVolumeBar->setRange(0,maxValue);
}

void VolumeWindow::updateBrightIcon()
{
    if (mbrightValue == 0) {
        mIconName = "ukui-light-0-symbolic";
    } else if (mbrightValue > 0 && mbrightValue <= 25) {
        mIconName = "ukui-light-25-symbolic";
    } else if (mbrightValue >25 && mbrightValue <= 50) {
        mIconName = "ukui-light-50-symbolic";
    } else if (mbrightValue >50 && mbrightValue <= 75) {
        mIconName = "ukui-light-75-symbolic";
    } else {
        mIconName = "ukui-light-100-symbolic";
    }
    QSize iconSize(ICON_SIZE,ICON_SIZE);
    mBut->setPixmap(drawLightColoredPixmap((QIcon::fromTheme(mIconName).pixmap(iconSize)),m_styleSettings->get("style-name").toString()));
}

void VolumeWindow::setBrightValue(int value)
{
    mbrightValue = value;
    mBrightBar->setValue(mbrightValue);
    updateBrightIcon();
}

void VolumeWindow::timeoutHandle()
{
    hide();
    mTimer->stop();
}

void VolumeWindow::showEvent(QShowEvent* e)
{
     QSize iconSize(ICON_SIZE,ICON_SIZE);
    /*适应主题颜色*/
    if(m_styleSettings->get("style-name").toString() == "ukui-light")
    {
        mVolumeBar->setStyleSheet("QProgressBar{border:none;border-radius:3px;background-color:rgba(0,0,0,0.2)}"
                            "QProgressBar::chunk{border-radius:3px;background:black}");
        mBrightBar->setStyleSheet("QProgressBar{border:none;border-radius:3px;background-color:rgba(0,0,0,0.2)}"
                            "QProgressBar::chunk{border-radius:3px;background:black}");
        setPalette(QPalette(QColor("#F5F5F5")));//设置窗口背景

    }
    else
    {
        mVolumeBar->setStyleSheet("QProgressBar{border:none;border-radius:3px;background-color:rgba(255,255,255,0.2)}"
                            "QProgressBar::chunk{border-radius:3px;background:white}");
        mBrightBar->setStyleSheet("QProgressBar{border:none;border-radius:3px;background-color:rgba(255,255,255,0.2)}"
                            "QProgressBar::chunk{border-radius:3px;background:white}");
        setPalette(QPalette(QColor("#232426")));//设置窗口背景色
    }
    mBut->setPixmap(drawLightColoredPixmap((QIcon::fromTheme(mIconName).pixmap(iconSize)),m_styleSettings->get("style-name").toString()));
}

double VolumeWindow::getGlobalOpacity()
{
    double transparency=0.0;
    if(QGSettings::isSchemaInstalled(QString("org.ukui.control-center.personalise").toLocal8Bit()))
    {
        QGSettings gsetting(QString("org.ukui.control-center.personalise").toLocal8Bit());
        if(gsetting.keys().contains(QString("transparency")))
            transparency=gsetting.get("transparency").toDouble();
    }
    return transparency;
}

QT_BEGIN_NAMESPACE
extern void qt_blurImage(QImage &blurImage, qreal radius, bool quality, int transposed);
QT_END_NAMESPACE
void VolumeWindow::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    QPainterPath rectPath;
    rectPath.addRoundedRect(this->rect().adjusted(10, 10, -10, -10), 12, 12);
    // 画一个黑底
    QPixmap pixmap(this->rect().size());
    pixmap.fill(Qt::transparent);
    QPainter pixmapPainter(&pixmap);
    pixmapPainter.setRenderHint(QPainter::Antialiasing);
    //      pixmapPainter.setCompositionMode(QPainter::CompositionMode_Difference);
    pixmapPainter.setPen(Qt::transparent);
    pixmapPainter.setBrush(Qt::black);
    pixmapPainter.setOpacity(0.16);
    pixmapPainter.drawPath(rectPath);
    pixmapPainter.end();
    // 模糊这个黑底
    QImage img = pixmap.toImage();
    qt_blurImage(img, 8, false, false);

    // 挖掉中心
    pixmap = QPixmap::fromImage(img);
    QPainter pixmapPainter2(&pixmap);
    pixmapPainter2.setRenderHint(QPainter::Antialiasing);
    pixmapPainter2.setCompositionMode(QPainter::CompositionMode_Clear);
    pixmapPainter2.setPen(Qt::transparent);
    pixmapPainter2.setBrush(Qt::transparent);
    pixmapPainter2.drawPath(rectPath);
        // 绘制阴影
    painter.drawPixmap(this->rect(), pixmap, pixmap.rect());

    //绘制描边
    QPainterPath linePath;
    linePath.addRoundedRect(QRect(9,9,m_frame->width()+1,m_frame->height()+1),12,12);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::black);
    painter.setBrush(Qt::transparent);
    painter.setOpacity(0.15);
    painter.drawPath(linePath);

    //毛玻璃
    qreal opacity = getGlobalOpacity();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::transparent);
    painter.setBrush(this->palette().base());
    painter.setPen(Qt::transparent);
    painter.setOpacity(opacity);
    painter.drawPath(linePath);

    KWindowEffects::enableBlurBehind(this->winId(), true, QRegion(linePath.toFillPolygon().toPolygon()));

    QWidget::paintEvent(event);
}
