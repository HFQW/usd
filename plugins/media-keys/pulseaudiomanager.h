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

#ifndef PLUSEAUDIOMANAGER_H
#define PLUSEAUDIOMANAGER_H

#include <QObject>

#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <QMutex>
#include <QSharedPointer>
#include <QMap>

extern "C" {
    #include "clib-syslog.h"
}

struct PaObject
{
    QString name;
    QString description;
    bool isDefault = false;
    bool mute;
    uint32_t index;
    uint channels;
    int balance;
    pa_volume_t volume;
    pa_channel_map channelMap;

    PaObject operator =(const PaObject& other) {
        this->description = other.description;
        this->name = other.name;
        this->channels = other.channels;
        this->mute = other.mute;
        this->volume = other.volume;
        this->index = other.index;
        this->channelMap = other.channelMap;
        this->balance = other.balance;
        return *this;
    }
};

typedef QSharedPointer<PaObject> PaObjectPtr;

class PulseAudioManager : public QObject
{
    Q_OBJECT
//    Q_PROPERTY(int sinkVolume MEMBER m_sinkVolume NOTIFY sinkVolumeChanged)
//    Q_PROPERTY(bool mute MEMBER m_sinkMute NOTIFY sinkMuteChanged)
public:
    explicit PulseAudioManager(QObject *parent = nullptr);
    ~PulseAudioManager();

public:
    void updateSinkInfo(const pa_sink_info* i);
    void updateSourceInfo(const pa_source_info* o);
    void updateDefault(const pa_server_info* i);
    void setSinkVolume(int value);
    void setSinkMute(bool muted);
    void setSourceMute(bool mute);
    uint getSinkVolume();
    bool getSinkMute();
    bool getSourceMute();
    void connectPulseContext();     //链接pulse 上下文
private:
    inline uint32_t volToPulseVol(int value);
    inline uint32_t volForPulseVol(uint32_t value);
private:
    void initPulseDevice();

protected:

    static void sinkInfoCallback(pa_context *c, const pa_sink_info *i, int eol, void *data);
    static void sourceInfoCallback(pa_context *c, const pa_source_info *i, int eol, void *data);
    static void serverInfoCallback(pa_context *c, const pa_server_info *i, void *data);

    static void subscribeCallback(pa_context *c, pa_subscription_event_type_t type, uint32_t idx, void *data);
    static void contextStateCallback(pa_context *c, void *data);
    static void sucessCallback(pa_context *c, int success, void *data);
private:
    pa_threaded_mainloop *m_paThreadMainLoop = nullptr;
    pa_context *m_paContext = nullptr;
    pa_mainloop_api *m_paMainloopApi = nullptr;

    PaObjectPtr m_defaultSink;
    PaObjectPtr m_defaultSource;
    QString m_defaultSinkName;
    QString m_defaultSourceName;

    QMap<uint32_t, PaObjectPtr> m_sinkList;
    QMap<uint32_t, PaObjectPtr> m_sourceList;
    QMutex m_mutex;

Q_SIGNALS:
    void sinkVolumeChanged(int);
    void sinkMuteChanged(bool);
};

#endif // PLUSEAUDIOMANAGER_H
