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

#ifndef __COLORDEFINE_H__
#define __COLORDEFINE_H__

#define USD_COLOR_SCHEMA            "org.ukui.SettingsDaemon.plugins.color"
#define COLOR_KEY_LAST_COORDINATES  "night-light-last-coordinates"
#define COLOR_KEY_ENABLED           "night-light-enabled"
#define COLOR_KEY_ALLDAY            "night-light-allday"
#define COLOR_KEY_AUTO_THEME        "theme-schedule-automatic"
#define COLOR_KEY_TEMPERATURE       "night-light-temperature"
#define COLOR_KEY_AUTOMATIC         "night-light-schedule-automatic"
#define COLOR_KEY_AUTOMATIC_FROM    "night-light-schedule-automatic-from"
#define COLOR_KEY_AUTOMATIC_TO      "night-light-schedule-automatic-to"
#define COLOR_KEY_FROM              "night-light-schedule-from"
#define COLOR_KEY_TO                "night-light-schedule-to"
#define COLOR_KEY_ACTIVE            "active"
#define COLOR_KEY_DARK_MODE         "dark-mode"
#define COLOR_KEY_ENABLED_DM        "night-light-enabled-dm"
#define COLOR_KEY_ALLDAY_DM         "night-light-allday-dm"
#define COLOR_KEY_AUTOMATIC_DM      "night-light-schedule-automatic-dm"
#define COLOR_KEY_STYLE_NAME_DM     "style-name-dm"
#define COLOR_KEY_REAL_TIME_TEMPERATURE "real-time-temperature"
#define HAD_SET_EDU                  "had-set-edu"

#define COLOR_KEY_AUTO_THEME_DM     "theme-schedule-automatic-dm"
#define GTK_THEME_SCHEMA            "org.mate.interface"
#define GTK_THEME_KEY               "gtk-theme"

#define QT_THEME_SCHEMA             "org.ukui.style"
#define QT_THEME_KEY                "style-name"

#define HAD_READ_KWIN               "had-read-kwin-config"

#define KWIN_COLOR_ACTIVE            "Active"
#define KWIN_COLOR_ACTIVE_ENABLE     "ActiveEnabled"
#define KWIN_NIGHT_TEMP              "NightTemperature"
#define KWIN_COLOR_MODE              "Mode"
#define KWIN_COLOR_START             "EveningBeginFixed"
#define KWIN_COLOR_END               "MorningBeginFixed"
#define KWIN_CURRENT_TEMP            "CurrentColorTemperature"
#define KWIN_LATITUDE                "LatitudeFixed"
#define KWIN_LONGITUDE               "LongitudeFixed"

#define USD_NIGHT_LIGHT_SCHEDULE_TIMEOUT    5       /* seconds */
#define USD_NIGHT_LIGHT_POLL_TIMEOUT        60      /* seconds */
#define USD_NIGHT_LIGHT_POLL_SMEAR          1       /* hours */
#define USD_NIGHT_LIGHT_SMOOTH_SMEAR        5.f     /* seconds */

#define USD_FRAC_DAY_MAX_DELTA              (1.f/60.f)     /* 1 minute */
#define USD_TEMPERATURE_MAX_DELTA           (10.f)


#define IP_API_ADDRESS_BACKUP  "http://ip-api.com/json/"
#define IP_API_ADDRESS  "https://location.services.mozilla.com/v1/geolocate?key=geoclue"
#define USD_CHECK_RETURN(A,B)  if(!(A)) {return B;}

#define COLOR_TEMPERATURE_DEFAULT  6500
#define COLOR_MIN_TEMPERATURE   1000


#endif
