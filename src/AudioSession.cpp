#include "AudioSession.h"

#include "AudioBackend.h"

#include <QFileInfo>
#include <QtGlobal>

AudioSession::AudioSession(AudioBackend *backend,
                           const QString &deviceId,
                           quint32 pid,
                           const QString &exePath,
                           QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_deviceId(deviceId)
    , m_pid(pid)
    , m_exePath(exePath)
{
    m_displayName = exePath.isEmpty() ? QStringLiteral("Unknown") : QFileInfo(exePath).baseName();

    m_volumeCommitTimer.setSingleShot(true);
    m_volumeCommitTimer.setInterval(16);
    m_volumeCommitTimer.setParent(this);
    connect(&m_volumeCommitTimer, &QTimer::timeout, this, &AudioSession::flushPendingVolume);
}

void AudioSession::setDisplayName(const QString &s)
{
    if (m_displayName == s)
        return;
    m_displayName = s;
    emit changed();
}

void AudioSession::setIconKey(const QString &k)
{
    if (m_iconKey == k)
        return;
    m_iconKey = k;
    emit changed();
}

void AudioSession::setVolumeInternal(double v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(m_volume, v))
        return;
    m_volume = v;
    emit changed();
}

void AudioSession::setMutedInternal(bool m)
{
    if (m_muted == m)
        return;
    m_muted = m;
    emit changed();
}

void AudioSession::setActiveInternal(bool a)
{
    if (m_active == a)
        return;
    m_active = a;
    emit changed();
}

void AudioSession::setPeakInternal(double p)
{
    p = qBound(0.0, p, 1.0);
    if (qFuzzyCompare(m_peak, p))
        return;
    m_peak = p;
    emit changed();
}

void AudioSession::setVolume(double v)
{
    // Coalesce rapid slider drags to avoid flooding COM calls (and potential instability).
    m_pendingVolume = qBound(0.0, v, 1.0);
    if (!m_volumeCommitTimer.isActive())
        m_volumeCommitTimer.start();
}

void AudioSession::setMuted(bool m)
{
    if (m_backend)
        m_backend->setSessionMuted(m_deviceId, m_pid, m_exePath, m);
}

void AudioSession::toggleMute()
{
    setMuted(!muted());
}

void AudioSession::flushPendingVolume()
{
    if (m_pendingVolume < 0.0)
        return;
    const double v = m_pendingVolume;
    m_pendingVolume = -1.0;
    if (m_backend)
        m_backend->setSessionVolume(m_deviceId, m_pid, m_exePath, v);
}


