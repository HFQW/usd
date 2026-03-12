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
#include "widget.h"
#include "ui_widget.h"

#include <QDebug>
#include <QScreen>
#include <QDBusInterface>
#include <QDBusConnection>

#include <kwindowsystem.h>
#include <QGraphicsEffect>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <KWindowEffects>
#include <QBitmap>
#include <QJsonDocument>
#include <QTextCodec>

#include "expendbutton.h"
#include "qtsingleapplication.h"
#include "clib-syslog.h"

#define TITLEHEIGHT 90
#define OPTIONSHEIGHT 70
#define BOTTOMHEIGHT 38


#define XSETTINGS_SCHEMA            "org.ukui.SettingsDaemon.plugins.xsettings"
#define XSETTINGS_KEY_SCALING       "scaling-factor"

#define QT_THEME_SCHEMA             "org.ukui.style"

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    m_superPresss = false;

    metaEnum = QMetaEnum::fromType<UsdBaseClass::eScreenMode>();

    QDBusInterface *screensChangedSignalHandle = new QDBusInterface(DBUS_XRANDR_NAME,DBUS_XRANDR_PATH,DBUS_XRANDR_INTERFACE,QDBusConnection::sessionBus(),this);
   // m_statusManagerDbus = new QDBusInterface("com.kylin.statusmanager.interface","/","com.kylin.statusmanager.interface",QDBusConnection::sessionBus(),this);

     if (screensChangedSignalHandle->isValid()) {
         connect(screensChangedSignalHandle, SIGNAL(screensParamChanged(QString)), this, SLOT(screensParamChangedSignal(QString)));
         //USD_LOG(LOG_DEBUG, "..");
     } else {
         USD_LOG(LOG_ERR, "screensChangedSignalHandle");
     }

}

Widget::~Widget()
{
    delete ui;
    delete m_scaleSetting;
    delete btnsGroup;
}

void Widget::screensParamChangedSignal(QString screensParam)
{
   close();
}
void Widget::beginSetup()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setProperty("useStyleWindowManager", false);

    setAttribute(Qt::WA_TranslucentBackground, true);

    m_scaleSetting = new QGSettings(XSETTINGS_SCHEMA);
    m_scale = m_scaleSetting->get(XSETTINGS_KEY_SCALING).toDouble();

    m_styleSettings = new QGSettings(QT_THEME_SCHEMA);

    /* 不在任务栏显示图标 */
    KWindowSystem::setState(winId(), NET::SkipTaskbar | NET::SkipPager);

    QScreen * pScreen = QGuiApplication::screens().at(0);
    QPoint point = pScreen->geometry().center();
    move(point.x() - width()/2, point.y() - height()/2);

    initData();
    m_currentModeId = getCurrentStatus();
    setupComponent();
    setupConnect();

    initCurrentStatus(m_currentModeId);

    connect(QApplication::primaryScreen(), &QScreen::geometryChanged, this, &Widget::geometryChangedHandle);

    //connect(XEventMonitor::instance(), SIGNAL(buttonPress(int,int)),this, SLOT(XkbButtonEvent(int,int)));
    
    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this,[&](WId activeWindowId) {
        if (activeWindowId != 0 && activeWindowId != this->winId()) {
            close();
        }
    });

    //XEventMonitor::instance()->start();
}

void Widget::initData()
{
    btnsGroup = new QButtonGroup;
}

void Widget::setupComponent()
{
    QStringList btnTextList;
    QStringList btnImageList;
    QJsonDocument parser;
    QString firstScreen = nullptr;
    QString secondScreen = nullptr;
    QString screensParam = getScreensParam();
    QTextCodec *tc = QTextCodec::codecForName("UTF-8");
    QVariantList screensParamList = parser.fromJson(screensParam.toUtf8().data()).toVariant().toList();
    m_connectScreen = screensParamList.length();
    for (const auto &variantInfo : screensParamList) {
        const QVariantMap info = variantInfo.toMap();
        const auto metadata = info[QStringLiteral("metadata")].toMap();
        const QString outputName = metadata[QStringLiteral("name")].toString();

        if (firstScreen.isEmpty()){
            firstScreen = "First Screen";
            continue;
        }

        if (secondScreen.isEmpty()) {
            secondScreen = "Vice Screen";
            continue;
        }

    }

    if (secondScreen.isEmpty()) {
        secondScreen = "None";
    }
#if 0
    if (UsdBaseClass::isTablet()) {
        btnTextList<<"Clone Screen";
        btnTextList<<"Extend Screen";

    } else {
        btnTextList<<firstScreen;
        btnTextList<<secondScreen;
        btnTextList<<"Clone Screen";
        btnTextList<<"Extend Screen";
    }

    if (UsdBaseClass::isTablet()) {
        btnImageList<<":/img/clone.png";
        btnImageList<<":/img/extend.png";
    } else {
        btnImageList<<":/img/main.png";
        btnImageList<<":/img/vice.png";
        btnImageList<<":/img/clone.png";
        btnImageList<<":/img/extend.png";
    }
#else
    btnTextList<<firstScreen;
    btnTextList<<secondScreen;
    btnTextList<<"Clone Screen";
    btnTextList<<"Extend Screen";

    btnImageList<<":/img/main.png";
    btnImageList<<":/img/vice.png";
    btnImageList<<":/img/clone.png";
    btnImageList<<":/img/extend.png";
#endif

    this->setFixedWidth(384);
    this->setFixedHeight(TITLEHEIGHT + OPTIONSHEIGHT * btnTextList.length() + BOTTOMHEIGHT);

    const QString style = m_styleSettings->get("style-name").toString();

    ui->outputPrimaryTip->hide();

    for (int i = 0; i < btnTextList.length(); i++) {
        ExpendButton * btn = new ExpendButton();
        if (getIsTabletMode() && i != m_currentModeId) {
            btn->disableTextImage();
        }
        btn->setFixedHeight(70);
        btnsGroup->addButton(btn, i);
        btn->setSign(i % 2,style);
        btn->setBtnText(tr(btnTextList[i].toLatin1().data()));
        btn->setBtnLogo(btnImageList[i],style);

        ui->btnsVerLayout->addWidget(btn);
    }

    m_qssDefaultOrDark = ("QWidget#titleWidget{background: transparent;}"\
                          "QWidget#bottomWidget{background: transparent; border: none;}"\
                          "QLabel#titleLabel{color: #FFFFFF;background: transparent; }"\
                          );

    m_qssLight = ("QWidget#titleWidget{background: transparent; border: none;}"\
                  "QWidget#bottomWidget{background: transparent; border: none; }"\
                  "QLabel#titleLabel{color: #232426;background: transparent;}"\
                  );
    /*跟随系统字体变化*/
    qreal fontSize = m_styleSettings->get("system-font-size").toReal();
    QFont font;
    font.setPointSizeF(fontSize + 4);
    ui->titleLabel->setFont(font);
}

void Widget::setupConnect()
{
    connect(btnsGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, [=](int id) {
        /* 获取旧选项 */
        for (QAbstractButton * button : btnsGroup->buttons()) {
            ExpendButton * btn = dynamic_cast<ExpendButton *>(button);
            //qDebug() << "old index: " << btn->getBtnChecked();
            int index = btnsGroup->id(button);
            if (index == id && btn->getBtnChecked()){
                close();
            }
        }
#if 0
        //0,1  ->1,2
        if (true == UsdBaseClass::isTablet()) {
            id += 2;
        }
#endif
        {
            QString mode = "";
            switch (id) {
            case UsdBaseClass::eScreenMode::firstScreenMode:
                mode = metaEnum.key(UsdBaseClass::eScreenMode::firstScreenMode);
                break;
            case UsdBaseClass::eScreenMode::cloneScreenMode:
                mode = metaEnum.key(UsdBaseClass::eScreenMode::secondScreenMode);
                break;
            case UsdBaseClass::eScreenMode::extendScreenMode:
                mode = metaEnum.key(UsdBaseClass::eScreenMode::cloneScreenMode);
                break;
            case UsdBaseClass::eScreenMode::secondScreenMode:
                mode = metaEnum.key(UsdBaseClass::eScreenMode::extendScreenMode);
                break;
            default:
                break;
            }
            if (!getIsTabletMode()) {
                setScreenModeByDbus(mode);
            }
        }
        close();
    });
}

QString Widget::getScreensParam()
{
    QDBusMessage message = QDBusMessage::createMethodCall(DBUS_XRANDR_NAME,
                                                          DBUS_XRANDR_PATH,
                                                          DBUS_XRANDR_INTERFACE,
                                                          DBUS_XRANDR_GET_SCREEN_PARAM);
    QList<QVariant> args;
    args.append(qAppName());
    message.setArguments(args);

    QDBusMessage response = QDBusConnection::sessionBus().call(message);

    if (response.type() == QDBusMessage::ReplyMessage) {
        if(response.arguments().isEmpty() == false) {
            QString value = response.arguments().takeFirst().toString();
            return value;
        }
    } else {
        USD_LOG(LOG_DEBUG, "called failed cuz:%s", message.errorName().toLatin1().data());
    }

    return nullptr;
}

int Widget::getCurrentStatus()
{
    QDBusMessage message = QDBusMessage::createMethodCall(DBUS_XRANDR_NAME,
                                                          DBUS_XRANDR_PATH,
                                                          DBUS_XRANDR_INTERFACE,
                                                          DBUS_XRANDR_GET_MODE);
    QList<QVariant> args;
    args.append(qAppName());
    message.setArguments(args);

    QDBusMessage response = QDBusConnection::sessionBus().call(message);

    if (response.type() == QDBusMessage::ReplyMessage) {
        if(response.arguments().isEmpty() == false) {
            int value = response.arguments().takeFirst().toInt();
            USD_LOG(LOG_DEBUG, "get mode :%s", metaEnum.key(value));

//0,1,2,3==>>>0,2,3,1
            switch (value) {
            case UsdBaseClass::eScreenMode::firstScreenMode:
                value = UsdBaseClass::eScreenMode::firstScreenMode;
                break;
            case UsdBaseClass::eScreenMode::cloneScreenMode:
                value = UsdBaseClass::eScreenMode::extendScreenMode;
                break;
            case UsdBaseClass::eScreenMode::extendScreenMode:
                value = UsdBaseClass::eScreenMode::secondScreenMode;
                break;
            case UsdBaseClass::eScreenMode::secondScreenMode:
                value = UsdBaseClass::eScreenMode::cloneScreenMode;
                break;
            default:
                break;
            }
            return value;
        }
    } else {
        USD_LOG(LOG_DEBUG, "called failed cuz:%s", message.errorName().toLatin1().data());
    }

    return 0;
}

void Widget::initCurrentStatus(int id)
{
    //set all no checked
#if 0
    if(true == UsdBaseClass::isTablet()) {
        id -= 2;
    }
#endif

    for (QAbstractButton * button : btnsGroup->buttons()) {
        ExpendButton * btn = dynamic_cast<ExpendButton *>(button);

        btn->setBtnChecked(false);

        if (id == btnsGroup->id(button)){
            btn->setBtnChecked(true);
            btn->setChecked(true);
        }
    }

    if (id == -1){
        ExpendButton * btn1 = dynamic_cast<ExpendButton *>(btnsGroup->button(0));
        btn1->setChecked(true);
    }
}

bool Widget::getIsTabletMode()
{
    static int ret = -1;
    if (ret!=-1) {
        return ret;
    }

    if (!UsdBaseClass::isTablet()) {
       return false;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(DBUS_STATUSMANAGER_NAME,
                                                          DBUS_STATUSMANAGER_PATH,
                                                          DBUS_STATUSMANAGER_INTERFACE,
                                                          DBUS_STATUSMANAGER_GET_MODE);

    QDBusMessage response = QDBusConnection::sessionBus().call(message);

    if (response.type() == QDBusMessage::ReplyMessage) {
        USD_LOG(LOG_DAEMON,"ready get replyMessage");
        if(response.arguments().isEmpty() == false) {
            bool value = response.arguments().takeFirst().toBool();
            USD_LOG(LOG_DAEMON,"get replyMessage:%d", value);
            ret = value;
            return value;
        }
    }
    USD_LOG(LOG_DAEMON,"can't get replyMessage..");
    return false;
}


void Widget::geometryChangedHandle()
{
    int x=QApplication::primaryScreen()->geometry().x();
    int y=QApplication::primaryScreen()->geometry().y();
    int width = QApplication::primaryScreen()->size().width();
    int height = QApplication::primaryScreen()->size().height();
    int ax,ay;
    ax = x+(width - this->width())/2;
    ay = y+(height - this->height())/2;
    move(ax,ay);

}

void Widget::nextSelectedOption()
{
    int current = btnsGroup->checkedId();
    int next;
    if (getIsTabletMode()) {
        return;
    }
    next = current == btnsGroup->buttons().count() - 1 ? 0 : current + 1;

    ExpendButton * btn = dynamic_cast<ExpendButton *>(btnsGroup->button(next));
    btn->setChecked(true);
}

void Widget::lastSelectedOption()
{
    int current = btnsGroup->checkedId();
    int last;
    if (getIsTabletMode()) {
        return;
    }
    /* no button checked */
    if (current == -1)
        current = btnsGroup->buttons().count();

    last = current == 0 ? btnsGroup->buttons().count() - 1 : current - 1;

    ExpendButton * btn = dynamic_cast<ExpendButton *>(btnsGroup->button(last));
    btn->setChecked(true);
}

void Widget::confirmCurrentOption()
{
    int current = btnsGroup->checkedId();
    if (current == -1)
        return;

    ExpendButton * btn = dynamic_cast<ExpendButton *>(btnsGroup->button(current));
    //btn->click();
    btn->animateClick();
}

void Widget::closeApp()
{
    close();
}

void Widget::setScreenModeByDbus(QString modeName)
{
    if (m_connectScreen==1) {
        USD_LOG(LOG_DEBUG,"don't set screen mode when connect 1 screen");
        return;
    }

    QList<QVariant> args;
    if (modeName.isEmpty()) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(DBUS_XRANDR_NAME,
                                                          DBUS_XRANDR_PATH,
                                                          DBUS_XRANDR_INTERFACE,
                                                          DBUS_XRANDR_SET_MODE);
    args.append(modeName);
    args.append(qAppName());
    message.setArguments(args);

    QDBusConnection::sessionBus().send(message);
}

void Widget::msgReceiveAnotherOne(const QString &msg)
{
    nextSelectedOption();
}

void Widget::receiveButtonClick(int x, int y)
{
    if (!this->geometry().contains(x, y)) {
        close();
    }

}

void Widget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Up:
        lastSelectedOption();
        break;
    case Qt::Key_Down:
        nextSelectedOption();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        confirmCurrentOption();
        break;
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
        break;
    default:
        close();
        break;
    }
}

void Widget::showEvent(QShowEvent* event)
{
    if(m_styleSettings->get("style-name").toString() == "ukui-light")
    {
        setStyleSheet(m_qssLight);
        setPalette(QPalette(QColor("#FFFFFF")));//设置窗口背景色

    } else {
        setPalette(QPalette(QColor("#40232426")));//设置窗口背景色
        setStyleSheet(m_qssDefaultOrDark);
    }
}

double Widget::getGlobalOpacity()
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

void Widget::paintEvent(QPaintEvent *event)
{
    QRect rect = this->rect();
    QPainterPath path;
    qreal opacity = getGlobalOpacity();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);  //反锯齿;
    painter.setPen(Qt::transparent);
    qreal radius=24;
    path.moveTo(rect.topRight() - QPointF(radius, 0));
    path.lineTo(rect.topLeft() + QPointF(radius, 0));
    path.quadTo(rect.topLeft(), rect.topLeft() + QPointF(0, radius));
    path.lineTo(rect.bottomLeft() + QPointF(0, -radius));
    path.quadTo(rect.bottomLeft(), rect.bottomLeft() + QPointF(radius, 0));
    path.lineTo(rect.bottomRight() - QPointF(radius, 0));
    path.quadTo(rect.bottomRight(), rect.bottomRight() + QPointF(0, -radius));
    path.lineTo(rect.topRight() + QPointF(0, radius));
    path.quadTo(rect.topRight(), rect.topRight() + QPointF(-radius, -0));

    painter.setBrush(this->palette().base());
    painter.setPen(Qt::transparent);
    painter.setOpacity(opacity);
    painter.drawPath(path);

    KWindowEffects::enableBlurBehind(this->winId(), true, QRegion(path.toFillPolygon().toPolygon()));
    QWidget::paintEvent(event);
}





