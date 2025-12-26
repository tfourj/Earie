#include "AudioDevice.h"

#include "AudioBackend.h"
#include "SessionListModel.h"

#include <QtGlobal>

AudioDevice::AudioDevice(AudioBackend *backend, const QString &id, const QString &name, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_id(id)
    , m_name(name)
{
    m_sessions = new SessionListModel(this);
}

void AudioDevice::setName(const QString &n)
{
    if (m_name == n)
        return;
    m_name = n;
    emit changed();
}

void AudioDevice::setIsDefault(bool d)
{
    if (m_isDefault == d)
        return;
    m_isDefault = d;
    emit changed();
}

void AudioDevice::setVolumeInternal(double v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(m_volume, v))
        return;
    m_volume = v;
    emit changed();
}

void AudioDevice::setMutedInternal(bool m)
{
    if (m_muted == m)
        return;
    m_muted = m;
    emit changed();
}

void AudioDevice::setVolume(double v)
{
    if (m_backend)
        m_backend->setDeviceVolume(m_id, v);
}

void AudioDevice::setMuted(bool m)
{
    if (m_backend)
        m_backend->setDeviceMuted(m_id, m);
}

void AudioDevice::toggleMute()
{
    setMuted(!muted());
}


