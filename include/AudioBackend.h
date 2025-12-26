#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QThread>
#include <QVector>

class ConfigStore;
class DeviceListModel;
class AudioDevice;
class AudioSession;
class IconCache;
class UpdateCoalescer;
class AudioWorker;
struct DeviceState;

class AudioBackend final : public QObject
{
    Q_OBJECT
public:
    struct DeviceSnapshot {
        QString id;
        QString name;
    };
    struct ProcessSnapshot {
        QString exePath;
        QString displayName;
    };

    explicit AudioBackend(QObject *parent = nullptr);
    ~AudioBackend() override;

    void setConfig(ConfigStore *cfg);
    void setAllDevices(bool all);
    void setShowSystemSessions(bool show);

    void start();
    void refresh();

    DeviceListModel *deviceModel() const { return m_deviceModel; }
    IconCache *iconCache() const { return m_iconCache; }

    bool hasDefaultDevice() const { return m_hasDefaultDevice; }
    QString defaultDeviceId() const { return m_defaultDeviceId; }
    QString defaultDeviceName() const { return m_defaultDeviceName; }
    double defaultDeviceVolume() const { return m_defaultDeviceVolume; } // 0..1
    bool defaultDeviceMuted() const { return m_defaultDeviceMuted; }

    QVector<DeviceSnapshot> devicesSnapshot() const;
    QVector<DeviceSnapshot> devicesSnapshotAll() const; // includes hidden + regardless of mode
    QVector<ProcessSnapshot> knownProcessesSnapshot() const;
    QVector<ProcessSnapshot> knownProcessesForDeviceSnapshot(const QString &deviceId) const;

    // Called by QML via AudioDevice/AudioSession objects.
    void setDeviceVolume(const QString &deviceId, double volume01);
    void setDeviceMuted(const QString &deviceId, bool muted);
    void setSessionVolume(const QString &deviceId, quint32 pid, const QString &exePath, double volume01);
    void setSessionMuted(const QString &deviceId, quint32 pid, const QString &exePath, bool muted);

public slots:
    Q_INVOKABLE void moveDeviceBefore(const QString &movingDeviceId, const QString &beforeDeviceId);
    Q_INVOKABLE void moveDeviceToIndex(const QString &movingDeviceId, int toIndex);

signals:
    void devicesChanged();
    void knownProcessesChanged();

private:
    void applySnapshot(const QVector<DeviceState> &devices);
    void rebuildMenusIfChanged(bool devicesChanged, bool processesChanged);

    QPointer<ConfigStore> m_config;
    bool m_allDevices = false;
    bool m_showSystemSessions = false;

    DeviceListModel *m_deviceModel = nullptr;
    IconCache *m_iconCache = nullptr;
    UpdateCoalescer *m_coalescer = nullptr;

    QThread m_workerThread;
    AudioWorker *m_worker = nullptr;

    QHash<QString, AudioDevice *> m_deviceById;
    // deviceId -> (pid|exePath) -> session
    QHash<QString, QHash<QString, AudioSession *>> m_sessionByKeyByDevice;

    QVector<DeviceState> m_lastSnapshot;

    bool m_hasDefaultDevice = false;
    QString m_defaultDeviceId;
    QString m_defaultDeviceName;
    double m_defaultDeviceVolume = 1.0;
    bool m_defaultDeviceMuted = false;
};


