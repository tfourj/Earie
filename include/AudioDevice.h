#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QTimer>
#include <QString>

#include "SessionListModel.h"
class AudioBackend;

class AudioDevice final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)
    Q_PROPERTY(double volume READ volume NOTIFY changed) // 0..1
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* sessionsModel READ sessionsModel CONSTANT)
public:
    explicit AudioDevice(AudioBackend *backend, const QString &id, const QString &name, QObject *parent = nullptr);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    bool isDefault() const { return m_isDefault; }
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }

    // QML-facing model (avoid moc needing SessionListModel complete type)
    QAbstractItemModel *sessionsModel() const { return static_cast<QAbstractItemModel *>(m_sessions); }
    // C++-facing typed access
    SessionListModel *sessionsModelTyped() const { return m_sessions; }

    void setName(const QString &n);
    void setIsDefault(bool d);
    void setVolumeInternal(double v);
    void setMutedInternal(bool m);

public slots:
    Q_INVOKABLE void setVolume(double v);
    Q_INVOKABLE void setMuted(bool m);
    Q_INVOKABLE void toggleMute();

signals:
    void changed();

private:
    void flushPendingVolume();

    AudioBackend *m_backend = nullptr;
    QString m_id;
    QString m_name;
    bool m_isDefault = false;
    double m_volume = 1.0;
    bool m_muted = false;
    SessionListModel *m_sessions = nullptr;

    QTimer m_volumeCommitTimer;
    double m_pendingVolume = -1.0;
};


