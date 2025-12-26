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

    m_volumeCommitTimer.setSingleShot(true);
    m_volumeCommitTimer.setInterval(16);
    m_volumeCommitTimer.setParent(this);
    connect(&m_volumeCommitTimer, &QTimer::timeout, this, &AudioDevice::flushPendingVolume);
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

void AudioDevice::setPeakInternal(double p)
{
    p = qBound(0.0, p, 1.0);
    if (qFuzzyCompare(m_peak, p))
        return;
    m_peak = p;
    emit changed();
}

void AudioDevice::setVolume(double v)
{
    m_pendingVolume = qBound(0.0, v, 1.0);
    if (!m_volumeCommitTimer.isActive())
        m_volumeCommitTimer.start();
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

void AudioDevice::flushPendingVolume()
{
    if (m_pendingVolume < 0.0)
        return;
    const double v = m_pendingVolume;
    m_pendingVolume = -1.0;
    if (m_backend)
        m_backend->setDeviceVolume(m_id, v);
}


