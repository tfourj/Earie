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

void AudioSession::setVolume(double v)
{
    if (m_backend)
        m_backend->setSessionVolume(m_deviceId, m_pid, m_exePath, v);
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


