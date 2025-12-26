#pragma once

#include <QObject>
#include <QTimer>
#include <QString>

class AudioBackend;

class AudioSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString deviceId READ deviceId CONSTANT)
    Q_PROPERTY(quint32 pid READ pid CONSTANT)
    Q_PROPERTY(QString exePath READ exePath CONSTANT)
    Q_PROPERTY(QString displayName READ displayName NOTIFY changed)
    Q_PROPERTY(QString iconKey READ iconKey NOTIFY changed)
    Q_PROPERTY(double volume READ volume NOTIFY changed) // 0..1
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
public:
    explicit AudioSession(AudioBackend *backend,
                          const QString &deviceId,
                          quint32 pid,
                          const QString &exePath,
                          QObject *parent = nullptr);

    QString deviceId() const { return m_deviceId; }
    quint32 pid() const { return m_pid; }
    QString exePath() const { return m_exePath; }

    QString displayName() const { return m_displayName; }
    QString iconKey() const { return m_iconKey; }
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    bool active() const { return m_active; }

    void setDisplayName(const QString &s);
    void setIconKey(const QString &k);
    void setVolumeInternal(double v);
    void setMutedInternal(bool m);
    void setActiveInternal(bool a);

public slots:
    Q_INVOKABLE void setVolume(double v);
    Q_INVOKABLE void setMuted(bool m);
    Q_INVOKABLE void toggleMute();

signals:
    void changed();

private:
    void flushPendingVolume();

    AudioBackend *m_backend = nullptr;
    QString m_deviceId;
    quint32 m_pid = 0;
    QString m_exePath;

    QString m_displayName;
    QString m_iconKey;
    double m_volume = 1.0;
    bool m_muted = false;
    bool m_active = false;

    QTimer m_volumeCommitTimer;
    double m_pendingVolume = -1.0;
};


