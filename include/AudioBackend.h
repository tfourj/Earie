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

    QVector<DeviceSnapshot> devicesSnapshot() const;
    QVector<ProcessSnapshot> knownProcessesSnapshot() const;
    QVector<ProcessSnapshot> knownProcessesForDeviceSnapshot(const QString &deviceId) const;

    // Called by QML via AudioDevice/AudioSession objects.
    void setDeviceVolume(const QString &deviceId, double volume01);
    void setDeviceMuted(const QString &deviceId, bool muted);
    void setSessionVolume(const QString &deviceId, quint32 pid, const QString &exePath, double volume01);
    void setSessionMuted(const QString &deviceId, quint32 pid, const QString &exePath, bool muted);

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
};


