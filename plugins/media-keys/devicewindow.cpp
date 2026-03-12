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
#include "devicewindow.h"
#include "ui_devicewindow.h"
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QBitmap>
#include <KWindowEffects>
#include "clib-syslog.h"
#include "usd_base_class.h"

#define DBUS_NAME       "org.ukui.SettingsDaemon"
#define DBUS_PATH       "/org/ukui/SettingsDaemon/wayland"
#define DBUS_INTERFACE  "org.ukui.SettingsDaemon.wayland"

#define QT_THEME_SCHEMA             "org.ukui.style"

#define PANEL_SCHEMA "org.ukui.panel.settings"
#define PANEL_SIZE_KEY "panelsize"

#define DEFAULT_LOCALE_ICON_NAME ":/ukui_res/ukui/"
#define INTEL_LOCALE_ICON_NAME ":/ukui_res/ukui_intel/"

DeviceWindow::DeviceWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DeviceWindow)
{
    ui->setupUi(this);
    initWindowInfo();
    mDbusXrandInter = new QDBusInterface(DBUS_NAME,
                                         DBUS_PATH,
                                         DBUS_INTERFACE,
                                         QDBusConnection::sessionBus(), this);
     if (!mDbusXrandInter->isValid()) {
        USD_LOG(LOG_DEBUG, "stderr:%s\n",qPrintable(QDBusConnection::sessionBus().lastError().message()));
    }
    //监听dbus变化  更改主屏幕时，会进行信号发送
    connect(mDbusXrandInter, SIGNAL(screenPrimaryChanged(int,int,int,int)),
            this, SLOT(priScreenChanged(int,int,int,int)));

    m_styleSettings = new QGSettings(QT_THEME_SCHEMA);
    connect(m_styleSettings,SIGNAL(changed(const QString&)),
            this,SLOT(onStyleChanged(const QString&)));

    if(UsdBaseClass::isTablet()) {
        m_LocalIconPath = INTEL_LOCALE_ICON_NAME;
    } else {
        m_LocalIconPath = DEFAULT_LOCALE_ICON_NAME;
    }
}

DeviceWindow::~DeviceWindow()
{
    delete ui;
    delete mTimer;
    mTimer = nullptr;
}

/* 主屏幕变化监听函数 */
void DeviceWindow::priScreenChanged(int x, int y, int Width, int Height)
{
    const QByteArray id(PANEL_SCHEMA);

    int pSize = 0;

    if (QGSettings::isSchemaInstalled(id)){
        QGSettings * settings = new QGSettings(id);
        pSize = settings->get(PANEL_SIZE_KEY).toInt();

        delete settings;
    }

    int ax,ay;
    ax = x+Width - this->width() - 200;
    ay = y+Height - this->height() - pSize-8;
    move(ax,ay);

    USD_LOG(LOG_DEBUG,"move it at %d,%d",ax,ay);

}
void DeviceWindow::geometryChangedHandle()
{
    int x=QApplication::primaryScreen()->geometry().x();
    int y=QApplication::primaryScreen()->geometry().y();
    int width = QApplication::primaryScreen()->size().width();
    int height = QApplication::primaryScreen()->size().height();

    USD_LOG(LOG_DEBUG,"getchangehandle....%dx%d at(%d,%d)",width,height,x,y);
    priScreenChanged(x,y,width,height);
}

void DeviceWindow::initWindowInfo()
{ 
    mTimer = new QTimer();
    connect(mTimer,SIGNAL(timeout()),this,SLOT(timeoutHandle()));

    this->setFixedSize(92,92);
    m_frame = new QFrame(this);
    m_frame->setFixedSize(QSize(72,72));
    m_frame->move(10,10);
    m_btnStatus = new QLabel(m_frame);
    m_btnStatus->setFixedSize(QSize(48,48));
    m_btnStatus->move((m_frame->width() - m_btnStatus->width())/2,(m_frame->height() - m_btnStatus->height())/2);

    connect(QApplication::primaryScreen(), &QScreen::geometryChanged, this, &DeviceWindow::geometryChangedHandle);
    connect(static_cast<QApplication *>(QCoreApplication::instance()),
            &QApplication::primaryScreenChanged, this, &DeviceWindow::geometryChangedHandle);

    setWindowFlags(Qt::FramelessWindowHint |
                   Qt::Tool |
                   Qt::WindowStaysOnTopHint |
                   Qt::X11BypassWindowManagerHint |
                   Qt::Popup);
    setAttribute(Qt::WA_TranslucentBackground, true);

    setAutoFillBackground(true);

    geometryChangedHandle();
}

void DeviceWindow::setAction(const QString icon)
{
    mIconName = icon;
}

void DeviceWindow::dialogShow()
{
    geometryChangedHandle();
    repaintWidget();
    show();
    mTimer->start(2000);
}

void DeviceWindow::timeoutHandle()
{
    hide();
    mTimer->stop();
}

QPixmap DeviceWindow::drawLightColoredPixmap(const QPixmap &source, const QString &style)
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

void DeviceWindow::repaintWidget()
{
    if(m_styleSettings->get("style-name").toString() == "ukui-light"){
        setPalette(QPalette(QColor("#F5F5F5")));//设置窗口背景
    } else{
        setPalette(QPalette(QColor("#232426")));//设置窗口背景色
    }
    QString m_LocalIconName;
    m_LocalIconName = m_LocalIconPath + mIconName + QString(".svg");
    QPixmap m_pixmap = QIcon::fromTheme(mIconName,QIcon(m_LocalIconName)).pixmap(QSize(48,48));
    m_btnStatus->setPixmap(drawLightColoredPixmap(m_pixmap,m_styleSettings->get("style-name").toString()));
}

void DeviceWindow::onStyleChanged(const QString&)
{
    if(!this->isHidden()) {
        hide();
        repaintWidget();
        show();
    }
}

void DeviceWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

double DeviceWindow::getGlobalOpacity()
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

void DeviceWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    QPainterPath rectPath;
    rectPath.addRoundedRect(this->rect().adjusted(10, 10, -10, -10), 12, 12);
    // 画一个黑底
    QPixmap pixmap(this->rect().size());
    pixmap.fill(Qt::transparent);
    QPainter pixmapPainter(&pixmap);
    pixmapPainter.setRenderHint(QPainter::Antialiasing);
    pixmapPainter.setPen(Qt::transparent);
    pixmapPainter.setBrush(this->palette().base());
    pixmapPainter.setCompositionMode(QPainter::CompositionMode_Difference);
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
    painter.setCompositionMode(QPainter::CompositionMode_Difference);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(this->palette().color(QPalette::ColorRole::BrightText));
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

