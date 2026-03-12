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

#include "pulseaudiomanager.h"
#include <QDebug>
#include <QMutexLocker>

#define VOLMUE_NORMAL 100.00f

#define MIN_VOLUME PA_VOLUME_NORM/500

PulseAudioManager::PulseAudioManager(QObject *parent) : QObject(parent)
{
}

PulseAudioManager::~PulseAudioManager()
{
    if (m_paMainloopApi) {
        m_paMainloopApi->quit(m_paMainloopApi, 0);
        m_paMainloopApi = nullptr;
    }
    if (m_paContext) {
        pa_context_set_state_callback(m_paContext, nullptr, nullptr);
        pa_context_disconnect(m_paContext);
        pa_context_unref(m_paContext);
        m_paContext = nullptr;
    }
    if (m_paThreadMainLoop) {
        pa_threaded_mainloop_stop(m_paThreadMainLoop);
        pa_threaded_mainloop_free(m_paThreadMainLoop);
        m_paThreadMainLoop = nullptr;
    }
    m_sinkList.clear();
    m_sourceList.clear();
}

void PulseAudioManager::subscribeCallback(pa_context *c, pa_subscription_event_type_t type, uint32_t idx, void *data)
{
    PulseAudioManager* pa = reinterpret_cast<PulseAudioManager*>(data);
    int event = (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK);
    switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SINK:
        if (event == PA_SUBSCRIPTION_EVENT_CHANGE
                || event == PA_SUBSCRIPTION_EVENT_NEW) {
            pa_operation *o = pa_context_get_sink_info_by_index(pa->m_paContext, idx, &PulseAudioManager::sinkInfoCallback, data);
            pa_operation_unref(o);
        } else if (event == PA_SUBSCRIPTION_EVENT_REMOVE) {
            QMutexLocker locker(&pa->m_mutex);
            pa->m_sinkList.remove(idx);
        }
        break;
    case PA_SUBSCRIPTION_EVENT_SOURCE:
        if (event == PA_SUBSCRIPTION_EVENT_CHANGE
                || event == PA_SUBSCRIPTION_EVENT_NEW) {
            pa_operation *o = pa_context_get_source_info_by_index(pa->m_paContext, idx, &PulseAudioManager::sourceInfoCallback, data);
            pa_operation_unref(o);
        } else if (event == PA_SUBSCRIPTION_EVENT_REMOVE) {
            QMutexLocker locker(&pa->m_mutex);
            pa->m_sourceList.remove(idx);
        }
        break;
    case PA_SUBSCRIPTION_EVENT_SERVER:
    {
        pa_operation *o = pa_context_get_server_info(pa->m_paContext, &PulseAudioManager::serverInfoCallback, data);
        pa_operation_unref(o);
        break;
    }
    default:
        break;
    }
}

void PulseAudioManager::contextStateCallback(pa_context *c, void *data)
{
//    assert(c && data);
    PulseAudioManager* pa = reinterpret_cast<PulseAudioManager*>(data);
    int state = pa_context_get_state(c);
    switch (state) {
    case PA_CONTEXT_READY:
    {
        pa_operation *o = pa_context_subscribe(pa->m_paContext,
                                              (pa_subscription_mask_t)
                                              (PA_SUBSCRIPTION_MASK_SINK |
                                               PA_SUBSCRIPTION_MASK_SOURCE |
                                               PA_SUBSCRIPTION_MASK_SERVER),
                                               &PulseAudioManager::sucessCallback, data);
        pa_operation_unref(o);
        pa->initPulseDevice();
        break;
    }
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        USD_LOG(LOG_WARNING, "PA_CONTEXT_FAILED || PA_CONTEXT_TERMINATED");
        break;
    default:
        break;
    }
}

void PulseAudioManager::sucessCallback(pa_context *c, int success, void *data)
{
    PulseAudioManager* pa = reinterpret_cast<PulseAudioManager*>(data);
    pa_threaded_mainloop_signal(pa->m_paThreadMainLoop, 0);
}

void PulseAudioManager::connectPulseContext()
{
    m_paThreadMainLoop = pa_threaded_mainloop_new();
    if (!m_paThreadMainLoop) {
        USD_LOG(LOG_WARNING,"new m_paThreadMainLoop failed");
        return;
    }

    m_paMainloopApi = pa_threaded_mainloop_get_api(m_paThreadMainLoop);
    pa_threaded_mainloop_lock(m_paThreadMainLoop);
    pa_proplist* plist = pa_proplist_new();
    pa_proplist_sets(plist, PA_PROP_APPLICATION_ID, "ukui-settings-daemon");
    pa_proplist_sets(plist, PA_PROP_APPLICATION_NAME, "ukui-settings-daemon");
    m_paContext = pa_context_new_with_proplist(m_paMainloopApi, nullptr, plist);
    pa_proplist_free(plist);
    if (!m_paContext) {
        pa_threaded_mainloop_unlock(m_paThreadMainLoop);
        pa_threaded_mainloop_free(m_paThreadMainLoop);
        USD_LOG(LOG_WARNING,"unable to create pa_context .");
        return;
    }

    pa_context_set_state_callback(m_paContext, &PulseAudioManager::contextStateCallback, this);
    pa_context_set_subscribe_callback(m_paContext, &PulseAudioManager::subscribeCallback, this);

    if (pa_context_connect(m_paContext, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        pa_context_unref(m_paContext);
        pa_threaded_mainloop_unlock(m_paThreadMainLoop);
        pa_threaded_mainloop_free(m_paThreadMainLoop);
        USD_LOG(LOG_WARNING,"connect pa_context failed");
        return;
    }

    if (pa_threaded_mainloop_start(m_paThreadMainLoop) < 0) {
        pa_context_disconnect(m_paContext);
        pa_context_unref(m_paContext);
        pa_threaded_mainloop_unlock(m_paThreadMainLoop);
        pa_threaded_mainloop_free(m_paThreadMainLoop);
        USD_LOG(LOG_WARNING,"pa_threaded_mainloop_start failed");
        return;
    }
    pa_threaded_mainloop_unlock(m_paThreadMainLoop);
}

void PulseAudioManager::serverInfoCallback(pa_context *c, const pa_server_info *i, void *data)
{
    PulseAudioManager* pa = reinterpret_cast<PulseAudioManager*>(data);
    pa->updateDefault(i);
}

void PulseAudioManager::sinkInfoCallback(pa_context *c, const pa_sink_info *si, int eol, void *data)
{
    PulseAudioManager* pa = reinterpret_cast<PulseAudioManager*>(data);
    if (!eol) {
        pa->updateSinkInfo(si);
    }
}

void PulseAudioManager::sourceInfoCallback(pa_context *c, const pa_source_info *so, int eol, void *data)
{
    PulseAudioManager* pa = reinterpret_cast<PulseAudioManager*>(data);
    if (!eol) {
        pa->updateSourceInfo(so);
    }
}

void PulseAudioManager::updateSinkInfo(const pa_sink_info* i)
{
    QMutexLocker locker(&m_mutex);
    PaObjectPtr sink;
    if (m_sinkList.count(i->index)) {
        sink = m_sinkList.value(i->index);
    } else {
        sink = PaObjectPtr(new PaObject);
        m_sinkList[i->index] = sink;
    }
    sink->description = QString::fromLatin1(i->description);
    sink->name = QString::fromLatin1(i->name);
    sink->channels = i->channel_map.channels;
    sink->mute = i->mute;
    sink->volume = pa_cvolume_max(&i->volume);
    sink->index = i->index;
    sink->channelMap = i->channel_map;
    sink->balance = pa_cvolume_get_balance(&i->volume, &i->channel_map) * 100.0;
    sink->isDefault = (sink->name == m_defaultSinkName);
    if (sink->isDefault) {
        if (m_defaultSink.isNull()) {
            m_defaultSink = PaObjectPtr(new PaObject);
        }
        m_defaultSink = sink;
        Q_EMIT sinkVolumeChanged(volForPulseVol(sink->volume));
        Q_EMIT sinkMuteChanged(sink->mute);
        USD_LOG(LOG_DEBUG, "id : %d %s change volume to %d", sink->index, sink->name.toLatin1().data(), volForPulseVol(sink->volume));
    }
}

void PulseAudioManager::updateSourceInfo(const pa_source_info* i)
{
    QMutexLocker locker(&m_mutex);
    PaObjectPtr source;
    if (m_sourceList.count(i->index)) {
        source = m_sourceList.value(i->index);
    } else {
        source = PaObjectPtr(new PaObject);
        m_sourceList[i->index] = source;
    }
    source->description = QString::fromLatin1(i->description);
    source->name = QString::fromLatin1(i->name);
    source->channels = i->channel_map.channels;
    source->mute = i->mute;
    source->volume = pa_cvolume_max(&i->volume);
    source->index = i->index;
    source->channelMap = i->channel_map;
    source->balance = pa_cvolume_get_balance(&i->volume, &i->channel_map);
    source->isDefault = (source->name == m_defaultSourceName);
    if (source->isDefault) {
        if (m_defaultSource.isNull()) {
            m_defaultSource = PaObjectPtr(new PaObject);
        }
        m_defaultSource = source;
    }
}

void PulseAudioManager::updateDefault(const pa_server_info *i)
{
    QMutexLocker locker(&m_mutex);
    //get sink info by name
    m_defaultSinkName = QString::fromLatin1(i->default_sink_name);
    pa_operation* o = pa_context_get_sink_info_by_name(m_paContext, i->default_sink_name, &PulseAudioManager::sinkInfoCallback, this);
    pa_operation_unref(o);

    //get source info by name
    m_defaultSourceName = QString::fromLatin1(i->default_source_name);
    o = pa_context_get_source_info_by_name(m_paContext, i->default_source_name, &PulseAudioManager::sourceInfoCallback, this);
    pa_operation_unref(o);
}

uint32_t PulseAudioManager::volToPulseVol(int value)
{
    return value == 0 ? MIN_VOLUME : value / VOLMUE_NORMAL * PA_VOLUME_NORM;
}

uint32_t PulseAudioManager::volForPulseVol(uint32_t value)
{
    return qRound(value * VOLMUE_NORMAL / PA_VOLUME_NORM);
}

void PulseAudioManager::initPulseDevice()
{
    pa_operation* o;
    //get sink info list
    o = pa_context_get_sink_info_list(m_paContext, &PulseAudioManager::sinkInfoCallback, this);
    pa_operation_unref(o);

    //get source info list
    o = pa_context_get_source_info_list(m_paContext, &PulseAudioManager::sourceInfoCallback, this);
    pa_operation_unref(o);

    //get server info
    o = pa_context_get_server_info(m_paContext, serverInfoCallback, this);
    pa_operation_unref(o);

}

void PulseAudioManager::setSinkVolume(int value)
{
    if (getSinkMute()) {
        setSinkMute(false);
    }
    pa_threaded_mainloop_lock(m_paThreadMainLoop);
    pa_cvolume volume;
    pa_cvolume_init(&volume);
    pa_cvolume_set(&volume, m_defaultSink->channels, volToPulseVol(value));
    pa_cvolume_set_balance(&volume, &m_defaultSink->channelMap, m_defaultSink->balance / 100.0);

    pa_operation* o;
    o = pa_context_set_sink_volume_by_index(m_paContext, m_defaultSink->index, &volume, nullptr, nullptr);
    pa_operation_unref(o);
    pa_threaded_mainloop_unlock(m_paThreadMainLoop);
}

void PulseAudioManager::setSinkMute(bool mute)
{
    pa_threaded_mainloop_lock(m_paThreadMainLoop);
    pa_operation* o;
    o = pa_context_set_sink_mute_by_index(m_paContext, m_defaultSink->index, mute, nullptr, nullptr);
    pa_operation_unref(o);
    pa_threaded_mainloop_unlock(m_paThreadMainLoop);
}

void PulseAudioManager::setSourceMute(bool mute)
{
    pa_threaded_mainloop_lock(m_paThreadMainLoop);
    pa_operation* o;
    o = pa_context_set_source_mute_by_index(m_paContext, m_defaultSource->index, mute, nullptr, nullptr);
    pa_operation_unref(o);
    pa_threaded_mainloop_unlock(m_paThreadMainLoop);
}

uint PulseAudioManager::getSinkVolume()
{
    QMutexLocker locker(&m_mutex);
    return volForPulseVol(m_defaultSink->volume);
}

bool PulseAudioManager::getSinkMute()
{
    QMutexLocker locker(&m_mutex);
    return m_defaultSink->mute;
}

bool PulseAudioManager::getSourceMute()
{
    QMutexLocker locker(&m_mutex);
    return m_defaultSource->mute;
}

