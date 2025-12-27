#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>

struct SessionState
{
    QString deviceId;
    quint32 pid = 0;
    QString exePath;
    QString displayName;
    QString iconKey; // currently exePath; backend may override
    double volume = 1.0; // 0..1
    bool muted = false;
    bool active = false;
    qint64 lastActiveMs = 0; // epoch ms
};

struct DeviceState
{
    QString id;
    QString name;
    bool isDefault = false;
    double volume = 1.0; // 0..1
    bool muted = false;
    QVector<SessionState> sessions;
};

struct SessionPeak
{
    QString deviceId;
    quint32 pid = 0;
    QString exePath;
    double peak = 0.0; // 0..1
};

Q_DECLARE_METATYPE(SessionState)
Q_DECLARE_METATYPE(DeviceState)
Q_DECLARE_METATYPE(QVector<DeviceState>)
Q_DECLARE_METATYPE(SessionPeak)
Q_DECLARE_METATYPE(QVector<SessionPeak>)

class AudioWorker final : public QObject
{
    Q_OBJECT
public:
    explicit AudioWorker(QObject *parent = nullptr);
    ~AudioWorker() override;

public slots:
    void start();
    void stop();

    void setShowSystemSessions(bool show);

    void setDeviceVolume(const QString &deviceId, double volume01);
    void setDeviceMuted(const QString &deviceId, bool muted);
    void setSessionVolume(const QString &deviceId, quint32 pid, const QString &exePath, double volume01);
    void setSessionMuted(const QString &deviceId, quint32 pid, const QString &exePath, bool muted);

signals:
    void snapshotReady(const QVector<DeviceState> &devices);
    void peaksReady(const QVector<SessionPeak> &peaks);
    void error(const QString &message);

private:
    void scheduleSnapshot();
    void emitSnapshotNow();
    void emitPeaksNow();

    bool m_showSystemSessions = false;
    std::atomic<bool> m_destroying{false};
    QTimer m_snapshotTimer;
    QTimer m_meterTimer;

    // PIMPL-ish: implemented in cpp to keep COM headers out of here.
    struct Impl;
    Impl *m = nullptr;
};


