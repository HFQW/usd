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

#include "keyboard-widget.h"
#include "ui_keyboardwidget.h"
#include <QTimer>
#include <QPainter>
#include <QBitmap>
#include <QScreen>
#include "clib-syslog.h"
#include <QDebug>
#include <QLayout>
#include <QPixmap>
#include <QIcon>
#include <QPainterPath>
#include <KWindowEffects>
#include "usd_base_class.h"

#define QT_THEME_SCHEMA             "org.ukui.style"

#define PANEL_SCHEMA "org.ukui.panel.settings"
#define PANEL_SIZE_KEY "panelsize"

#define DEFAULT_LOCALE_ICON_NAME ":/ukui_res/ukui/"
#define INTEL_LOCALE_ICON_NAME ":/ukui_res/ukui_intel/"

KeyboardWidget::KeyboardWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::KeyboardWidget)
{
    ui->setupUi(this);
    initWidgetInfo();
}

KeyboardWidget::~KeyboardWidget()
{
    delete ui;
}

void KeyboardWidget::initWidgetInfo()
{
    setWindowFlags(Qt::FramelessWindowHint |
                   Qt::Tool |
                   Qt::WindowStaysOnTopHint |
                   Qt::X11BypassWindowManagerHint |
                   Qt::Popup);

    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(true);
    if(UsdBaseClass::isTablet()) {
        m_LocalIconPath = INTEL_LOCALE_ICON_NAME;
    } else {
        m_LocalIconPath = DEFAULT_LOCALE_ICON_NAME;
    }

    m_styleSettings = new QGSettings(QT_THEME_SCHEMA);
    connect(m_styleSettings,SIGNAL(changed(const QString&)),
            this,SLOT(onStyleChanged(const QString&)));


    m_timer = new QTimer(this);
    connect(m_timer,SIGNAL(timeout()),this,SLOT(timeoutHandle()));

    connect(QApplication::primaryScreen(), &QScreen::geometryChanged, this, &KeyboardWidget::geometryChangedHandle);

    connect(static_cast<QApplication *>(QCoreApplication::instance()),
            &QApplication::primaryScreenChanged, this, &KeyboardWidget::geometryChangedHandle);


    this->setFixedSize(92,92);
    m_frame = new QFrame(this);
    m_frame->setFixedSize(QSize(72,72));
    m_frame->move(10,10);
    m_btnStatus = new QLabel(m_frame);
    m_btnStatus->setFixedSize(QSize(48,48));
    m_btnStatus->move((m_frame->width() - m_btnStatus->width())/2,(m_frame->height() - m_btnStatus->height())/2);

    geometryChangedHandle();

}


void KeyboardWidget::timeoutHandle()
{
    m_timer->stop();
    hide();
}
void KeyboardWidget::showWidget()
{
    geometryChangedHandle();
    repaintWidget();
    show();
    m_timer->start(2500);

}


void KeyboardWidget::setIcons(QString icon)
{
    m_iconName = icon;
}

void KeyboardWidget::geometryChangedHandle()
{
    int x=QApplication::primaryScreen()->geometry().x();
    int y=QApplication::primaryScreen()->geometry().y();
    int width = QApplication::primaryScreen()->size().width();
    int height = QApplication::primaryScreen()->size().height();

    int pSize = 0;
    const QByteArray id(PANEL_SCHEMA);
    if (QGSettings::isSchemaInstalled(id)){
        QGSettings * settings = new QGSettings(id);
        pSize = settings->get(PANEL_SIZE_KEY).toInt();

        delete settings;
    }
    int ax,ay;
    ax = x+width - this->width() - 200;
    ay = y+height - this->height() - pSize - 8;
    move(ax,ay);
}

void KeyboardWidget::onStyleChanged(const QString& key)
{
    Q_UNUSED(key)
    if(!this->isHidden()) {
        hide();
        repaintWidget();
        show();
    }
}

QPixmap KeyboardWidget::drawLightColoredPixmap(const QPixmap &source, const QString &style)
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

void KeyboardWidget::repaintWidget()
{
    if(m_styleSettings->get("style-name").toString() == "ukui-light"){
        setPalette(QPalette(QColor("#F5F5F5")));//设置窗口背景
    } else{
        setPalette(QPalette(QColor("#232426")));//设置窗口背景色
    }
    QString m_LocalIconName;
    m_LocalIconName = m_LocalIconPath + m_iconName + QString(".svg");
    QPixmap m_pixmap = QIcon::fromTheme(m_iconName,QIcon(m_LocalIconName)).pixmap(QSize(48,48));
    m_btnStatus->setPixmap(drawLightColoredPixmap(m_pixmap,m_styleSettings->get("style-name").toString()));
}


void KeyboardWidget::resizeEvent(QResizeEvent* event)
{
//    m_btnStatus->move((width() - m_btnStatus->width())/2,(height() - m_btnStatus->height())/2);
    QWidget::resizeEvent(event);
}

double KeyboardWidget::getGlobalOpacity()
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

void KeyboardWidget::paintEvent(QPaintEvent *event)
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
    painter.setPen(this->palette().color(QPalette::ColorRole::BrightText));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::transparent);
    painter.setOpacity(0.15);
    painter.drawPath(linePath);

    //毛玻璃
    qreal opacity = getGlobalOpacity();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::transparent);
    painter.setBrush(this->palette().base());
    painter.setOpacity(opacity);
    painter.drawPath(linePath);

    KWindowEffects::enableBlurBehind(this->winId(), true, QRegion(linePath.toFillPolygon().toPolygon()));

    QWidget::paintEvent(event);
}



