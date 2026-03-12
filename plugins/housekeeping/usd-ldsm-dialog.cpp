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
#include "usd-ldsm-dialog.h"
#include "ui_usd-ldsm-dialog.h"

#include <QDesktopWidget>
#include <QRect>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QFont>
#include <QList>
#include <QDebug>

#include <QGSettings/qgsettings.h>
#include <syslog.h>
#include <glib.h>
#include "clib-syslog.h"
#include "usd_global_define.h"

LdsmDialog::LdsmDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LdsmDialog)
{
    ui->setupUi(this);
}

LdsmDialog::LdsmDialog(bool other_usable_partitions,bool other_partitions,bool display_baobab,bool has_trash,
                       long space_remaining,QString partition_name,QString mount_path,
                       QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LdsmDialog)
{

    ui->setupUi(this);
    this->other_usable_partitions=other_usable_partitions;
    this->other_partitions=other_partitions;
    this->has_trash=has_trash;
    this->space_remaining=space_remaining;
    this->partition_name=partition_name;
    this->mount_path=mount_path;
    this->analyze_button=nullptr;

    m_fontSetting = new QGSettings(UKUI_STYLE_SCHEMA, QByteArray(), this);
    connect(m_fontSetting, SIGNAL(changed(QString)), this, SLOT(updateText(QString)));

    windowLayoutInit(display_baobab);
    allConnectEvent(display_baobab);
}

LdsmDialog::~LdsmDialog()
{
    delete ui;
    delete picture_label;
    delete primary_label;
    delete ignore_check_button;
    delete ignore_button;

    if(this->has_trash)
        delete trash_empty;
    if(analyze_button)
        delete analyze_button;
}

void LdsmDialog::windowLayoutInit(bool display_baobab)
{
    QFont font;
    QDesktopWidget* desktop=QApplication::desktop();
    QRect desk_rect = desktop->screenGeometry(desktop->screenNumber(QCursor::pos()));
    Qt::WindowFlags flags=Qt::Dialog;
    flags |=Qt::WindowMinMaxButtonsHint;
    flags |=Qt::WindowCloseButtonHint;
    setWindowFlags(flags);
    setFixedSize(660,210);
    setWindowIcon(QIcon::fromTheme("dialog-warning"));
    int dialog_width=width();
    int dialog_height=height();
    int rect_width=desk_rect.width();
    int rect_height=desk_rect.height();

    setWindowTitle(tr("Low Disk Space"));
    this->move((rect_width-dialog_width)/2+desk_rect.left(),(rect_height-dialog_height)/2+desk_rect.top());

    picture_label = new QLabel(this);
    primary_label = new QLabel(this);
    scroll_area = new QScrollArea(this);
    ignore_check_button = new QCheckBox(this);
    ignore_button=new QPushButton(this);

    picture_label->setProperty("Name", "picture_label");
    primary_label->setProperty("Name", "primary_label");
    scroll_area->setProperty("Name", "scroll_area");
    scroll_area->setFrameShape(QFrame::NoFrame);
    ignore_check_button->setProperty("Name", "ignore_check_button");
    ignore_button->setProperty("Name", "ignore_button");

    //warning picture
    picture_label->setGeometry(20,40,32,32);
    picture_label->setAlignment(Qt::AlignCenter);
    picture_label->setStyleSheet(QString("border-image:url(../ldsm_dialog/warning.png);"));
    //warning information text
    scroll_area->setGeometry(50, 20, 560, 40*2);
    scroll_area->setWidget(primary_label);
    scroll_area->setWidgetResizable(true);
    primary_label->setGeometry(50, 20, 560, 40*2);

    primary_label->setWordWrap(true);
    primary_label->setAlignment(Qt::AlignLeft);

    primary_label->setText(getPrimaryText());
//    primary_label->adjustSize();

    //gsettings set box
    ignore_check_button->setGeometry(70,135,400,30);
    ignore_check_button->setText(getCheckButtonText());

    ignore_button->setGeometry(dialog_width-110,dialog_height-50,96,36);
    ignore_button->setText(tr("Confirm"));

    if(this->has_trash){
        trash_empty = new QPushButton(this);
        trash_empty->setProperty("Name", "trash_empty_button");
        trash_empty->setGeometry(dialog_width-240,dialog_height-50,96,36);
        trash_empty->setText(tr("Empty Trash"));
    }
    if(display_baobab){
        analyze_button = new QPushButton(this);
        analyze_button->setText(tr("Examine"));
        if(this->has_trash)
            analyze_button->setGeometry(dialog_width-320,dialog_height-50,96,36);
        else
            analyze_button->setGeometry(dialog_width-215,dialog_height-50,96,36);
    }
    updateText("");
}

QString LdsmDialog::getPrimaryText()
{
    char* free_space = g_format_size(space_remaining);
    return QString(tr("The remaining space of drive \"%1\" is less than %2, "\
                            "clear the garbage or move the data to another disk in time.")).arg(partition_name).arg(free_space);
}

QString LdsmDialog::getCheckButtonText()
{
    return QString(tr("Messages that no longer remind this disk"));
}

void LdsmDialog::allConnectEvent(bool display_baobab)
{
    connect(ignore_check_button, &QCheckBox::stateChanged,
            this,   &LdsmDialog::checkButtonClicked);

    connect(ignore_button, &QPushButton::clicked,
            this,   &LdsmDialog::checkButtonIgnore);

    if(has_trash)
        connect(trash_empty, &QPushButton::clicked,
                this,   &LdsmDialog::checkButtonTrashEmpty);

    if(display_baobab)
        connect(analyze_button, &QPushButton::clicked,
                this, &LdsmDialog::checkButtonAnalyze);

    if(sender() == ignore_button) {
        USD_LOG(LOG_DEBUG,"Ignore button pressed!");
    } else {
        USD_LOG(LOG_DEBUG,"Other button pressed!");
    }
}

//update gsettings "ignore-paths" key contents
bool update_ignore_paths(QList<QString>** ignore_paths,QString mount_path,bool ignore)
{
    bool found=(*ignore_paths)->contains(mount_path.toLatin1().data());
    if(ignore && found==false){
        (*ignore_paths)->push_front(mount_path.toLatin1().data());
        return true;
    }
    if(!ignore && found==true){
        (*ignore_paths)->removeOne(mount_path.toLatin1().data());
        return true;
    }
    return false;
}

/****************slots*********************/
void LdsmDialog::checkButtonIgnore()
{
    done(LDSM_DIALOG_IGNORE);
}

void LdsmDialog::checkButtonTrashEmpty()
{
    done(LDSM_DIALOG_RESPONSE_EMPTY_TRASH);
}

void LdsmDialog::updateText(QString key)
{
    USD_LOG(LOG_DEBUG,"get key:%s",key.toLatin1().data());

    if(has_trash) {
        resetFont(trash_empty, tr("Empty Trash"));
    }

    resetFont(ignore_button, tr("Confirm"));
}

void LdsmDialog::resetFont(QWidget *pWid, QString str)
{
    QPushButton *pBtn = nullptr;
    QLabel *pLbl = nullptr;
    int fontWidth = 0;
    int btnWidth = 0;

    if (pWid->property("Name").toString().contains("button")) {
        pBtn = static_cast<QPushButton*>(pWid);
    } else if (pWid->property("Name").toString().contains("label")) {
        pLbl = static_cast<QLabel*>(pWid);
    } else {
        return;
    }

    QFontMetrics qFm(pWid->font());
    fontWidth = qFm.width(str);
    if (pBtn != nullptr) {
        btnWidth = pBtn->width() - 7;
        if (fontWidth >= btnWidth) {
            QString elideNote = qFm.elidedText(pBtn->text(), Qt::ElideRight, btnWidth);
            pBtn->setText(elideNote);
            pBtn->setToolTip(str);
        } else {
            pBtn->setText(str);
            pBtn->setToolTip("");
        }
    } else {
        btnWidth = pLbl->width() - 7;
        if (fontWidth >= btnWidth) {
            QString elideNote = qFm.elidedText(pLbl->text(), Qt::ElideRight, btnWidth);
            pLbl->setText(elideNote);
            pLbl->setToolTip(str);
        } else {
            pLbl->setText(str);
            pLbl->setToolTip("");
        }
    }
}

void LdsmDialog::checkButtonAnalyze()
{
    done(LDSM_DIALOG_RESPONSE_ANALYZE);
}

void LdsmDialog::checkButtonClicked(int state)
{
    QGSettings* settings;
    QStringList ignore_list;
    QStringList ignoreStr;
    QList<QString>* ignore_paths;
    bool ignore,updated;
    int i;
    QList<QString>::iterator l;

    ignore_paths =new QList<QString>();
    settings = new QGSettings(SETTINGS_SCHEMA);
    //get contents from "ignore-paths" key
    if (!settings->get(SETTINGS_IGNORE_PATHS).toStringList().isEmpty())
        ignore_list.append(settings->get(SETTINGS_IGNORE_PATHS).toStringList());

    for(auto str: ignore_list){
        if(!str.isEmpty())
            ignore_paths->push_back(str);
    }

    ignore = state;
    updated = update_ignore_paths(&ignore_paths, mount_path, ignore);

    if(updated){
        for(l = ignore_paths->begin(); l != ignore_paths->end(); ++l){
            ignoreStr.append(*l);
        }
        //set latest contents to gsettings "ignore-paths" key
        settings->set(SETTINGS_IGNORE_PATHS, QVariant::fromValue(ignoreStr));
    }
    //free QList Memory
    if(ignore_paths){
        ignore_paths->clear();
    }
    delete settings;
}
