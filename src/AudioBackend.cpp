#include "AudioBackend.h"

#include "AudioDevice.h"
#include "AudioSession.h"
#include "AudioWorker.h"
#include "ConfigStore.h"
#include "DeviceDirectionProxyModel.h"
#include "DeviceListModel.h"
#include "IconCache.h"
#include "SessionListModel.h"
#include "UpdateCoalescer.h"

#include <QDateTime>
#include <QSet>
#include <QStringList>

static QString sessionKeyStr(quint32 pid, const QString &exePath);

AudioBackend::AudioBackend(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QVector<DeviceState>>("QVector<DeviceState>");
    qRegisterMetaType<QVector<SessionPeak>>("QVector<SessionPeak>");
    qRegisterMetaType<DeviceDirection>("DeviceDirection");

    m_deviceModel = new DeviceListModel(this);
    m_outputDeviceModel = new DeviceDirectionProxyModel(false, this);
    m_outputDeviceModel->setSourceModel(m_deviceModel);
    m_inputDeviceModel = new DeviceDirectionProxyModel(true, this);
    m_inputDeviceModel->setSourceModel(m_deviceModel);
    // The QQmlEngine will take ownership when we addImageProvider("appicon", ...).
    m_iconCache = new IconCache();
    m_coalescer = new UpdateCoalescer(this);
}

AudioBackend::~AudioBackend()
{
    qInfo() << "AudioBackend: Starting destruction";
    if (m_worker) {
        qInfo() << "AudioBackend: Disconnecting worker thread finished signal";
        disconnect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
        qInfo() << "AudioBackend: Stopping worker";
        QMetaObject::invokeMethod(m_worker, &AudioWorker::stop, Qt::BlockingQueuedConnection);
        qInfo() << "AudioBackend: Quitting worker thread";
        m_workerThread.quit();
        qInfo() << "AudioBackend: Waiting for worker thread to finish";
        m_workerThread.wait();
        qInfo() << "AudioBackend: Deleting worker";
        delete m_worker;
        m_worker = nullptr;
    } else {
        qInfo() << "AudioBackend: No worker to clean up, quitting thread";
        m_workerThread.quit();
        m_workerThread.wait();
    }
    qInfo() << "AudioBackend: Destruction complete";
}

void AudioBackend::setConfig(ConfigStore *cfg)
{
    m_config = cfg;
}

void AudioBackend::setAllDevices(bool all)
{
    m_allDevices = all;
    refresh();
}

void AudioBackend::setShowSystemSessions(bool show)
{
    m_showSystemSessions = show;
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setShowSystemSessions", Qt::QueuedConnection, Q_ARG(bool, m_showSystemSessions));
    refresh();
}

void AudioBackend::setShowInputDevices(bool show)
{
    m_showInputDevices = show;
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setShowInputDevices", Qt::QueuedConnection, Q_ARG(bool, m_showInputDevices));
    refresh();
}

void AudioBackend::start()
{
    if (m_worker)
        return;

    m_worker = new AudioWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &AudioWorker::snapshotReady, this, [this](const QVector<DeviceState> &devices) {
        // Coalesce on GUI thread to avoid thrashing QML bindings.
        if (m_coalescer) {
            m_coalescer->post([this, devices]() {
                m_lastSnapshot = devices;
                applySnapshot(devices);
            });
        } else {
            m_lastSnapshot = devices;
            applySnapshot(devices);
        }
    }, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::peaksReady, this, [this](const QVector<SessionPeak> &peaks) {
        if (m_coalescer) {
            m_coalescer->post([this, peaks]() { applyPeaks(peaks); });
        } else {
            applyPeaks(peaks);
        }
    }, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::error, this, [](const QString &msg) {
        qWarning("%s", qPrintable(msg));
    }, Qt::QueuedConnection);

    m_workerThread.start();

    QMetaObject::invokeMethod(m_worker, &AudioWorker::setShowSystemSessions, Qt::QueuedConnection, m_showSystemSessions);
    QMetaObject::invokeMethod(m_worker, &AudioWorker::setShowInputDevices, Qt::QueuedConnection, m_showInputDevices);
    QMetaObject::invokeMethod(m_worker, &AudioWorker::start, Qt::QueuedConnection);
}

void AudioBackend::applyPeaks(const QVector<SessionPeak> &peaks)
{
    if (peaks.isEmpty())
        return;

    QHash<QString, double> maxPeakByDevice;

    for (const auto &p : peaks) {
        if (p.deviceId.isEmpty() || p.pid == 0 || p.exePath.isEmpty())
            continue;

        auto maxIt = maxPeakByDevice.find(p.deviceId);
        if (maxIt == maxPeakByDevice.end() || p.peak > maxIt.value())
            maxPeakByDevice.insert(p.deviceId, p.peak);

        auto devIt = m_sessionByKeyByDevice.find(p.deviceId);
        if (devIt == m_sessionByKeyByDevice.end())
            continue;

        const QString keyStr = sessionKeyStr(p.pid, p.exePath);
        auto sessIt = devIt->find(keyStr);
        if (sessIt == devIt->end())
            continue;

        if (sessIt.value())
            sessIt.value()->setPeakInternal(p.peak);
    }

    // Also drive a per-device peak meter (max of its sessions).
    for (auto it = maxPeakByDevice.constBegin(); it != maxPeakByDevice.constEnd(); ++it) {
        if (auto *dev = m_deviceById.value(it.key(), nullptr)) {
            dev->setPeakInternal(it.value());
        }
    }
}

void AudioBackend::refresh()
{
    // Snapshot filtering is applied on the GUI side (mode + hidden rules),
    // so a refresh is effectively "re-apply last snapshot". Worker will also
    // naturally re-snapshot due to polling/callbacks.
    if (!m_lastSnapshot.isEmpty())
        applySnapshot(m_lastSnapshot);
}

static QString sessionKeyStr(quint32 pid, const QString &exePath)
{
    return QString::number(pid) + QLatin1Char('|') + exePath;
}

void AudioBackend::applySnapshot(const QVector<DeviceState> &devices)
{
    if (!m_deviceModel)
        return;

    if (m_config) {
        QSet<QString> connectedIds;
        QHash<QString, QStringList> connectedIdsByName;
        QHash<QString, QString> connectedNameById;
        connectedIds.reserve(devices.size());
        connectedIdsByName.reserve(devices.size());
        connectedNameById.reserve(devices.size());

        for (const auto &ds : devices) {
            if (ds.id.isEmpty())
                continue;
            connectedIds.insert(ds.id);
            connectedNameById.insert(ds.id, ds.name);
            const QString nameKey = ds.name.trimmed().toCaseFolded();
            if (!nameKey.isEmpty())
                connectedIdsByName[nameKey].append(ds.id);
        }

        QHash<QString, QStringList> disconnectedHiddenIdsByName;
        const auto hiddenIds = m_config->hiddenDevices();
        disconnectedHiddenIdsByName.reserve(hiddenIds.size());
        for (const auto &hiddenId : hiddenIds) {
            if (hiddenId.isEmpty() || connectedIds.contains(hiddenId))
                continue;
            const QString hiddenNameKey = m_config->hiddenDeviceName(hiddenId).trimmed().toCaseFolded();
            if (!hiddenNameKey.isEmpty())
                disconnectedHiddenIdsByName[hiddenNameKey].append(hiddenId);
        }

        for (auto it = disconnectedHiddenIdsByName.constBegin(); it != disconnectedHiddenIdsByName.constEnd(); ++it) {
            const QStringList oldIds = it.value();
            const QStringList newIds = connectedIdsByName.value(it.key());
            if (oldIds.size() != 1 || newIds.size() != 1)
                continue;
            const QString oldId = oldIds.constFirst();
            const QString newId = newIds.constFirst();
            if (oldId == newId)
                continue;
            (void)m_config->remapDeviceId(oldId, newId, connectedNameById.value(newId));
        }
    }

    bool anyDevicesChanged = false;
    bool anyProcessesChanged = false;

    // Track default device status for tray icon behavior (EarTrumpet-like).
    bool foundDefault = false;
    QString defId;
    QString defName;
    double defVol = 1.0;
    bool defMuted = false;

    QSet<QString> keepDeviceIds;

    for (const auto &ds : devices) {
        if (ds.id.isEmpty())
            continue;

        if (ds.direction == DeviceDirection::Output && ds.isDefault && !foundDefault) {
            foundDefault = true;
            defId = ds.id;
            defName = ds.name;
            defVol = ds.volume;
            defMuted = ds.muted;
        }

        if (m_config)
            m_config->rememberDeviceName(ds.id, ds.name);
        if (m_config && m_config->isDeviceHidden(ds.id)) {
            continue;
        }

        if (!m_allDevices && !ds.isDefault)
            continue;

        keepDeviceIds.insert(ds.id);

        AudioDevice *dev = m_deviceById.value(ds.id, nullptr);
        if (!dev) {
            dev = new AudioDevice(this, ds.id, ds.name, ds.direction, this);
            m_deviceById.insert(ds.id, dev);
        }
        // Ensure it's present in the model (it may exist in cache but be filtered out previously).
        if (m_deviceModel->indexOfDeviceId(ds.id) < 0) {
            m_deviceModel->insertDevice(m_deviceModel->rowCount(), dev);
            anyDevicesChanged = true;
        }

        dev->setName(ds.name);
        dev->setColorKey(m_config ? m_config->deviceColor(ds.id) : QString());
        dev->setDirection(ds.direction);
        dev->setIsDefault(ds.isDefault);
        dev->setVolumeInternal(ds.volume);
        dev->setMutedInternal(ds.muted);

        // Sessions diff per device.
        auto &map = m_sessionByKeyByDevice[ds.id];
        QSet<QString> keepSessions;

        for (const auto &ss : ds.sessions) {
            if (ss.pid == 0 || ss.exePath.isEmpty())
                continue;

            if (m_config) {
                if (m_config->isProcessHiddenGlobal(ss.exePath))
                    continue;
                if (m_config->isProcessHiddenForDevice(ds.id, ss.exePath))
                    continue;
            }

            const QString key = sessionKeyStr(ss.pid, ss.exePath);
            keepSessions.insert(key);

            AudioSession *sess = map.value(key, nullptr);
            if (!sess) {
                sess = new AudioSession(this, ds.id, ss.pid, ss.exePath, this);
                sess->setDisplayName(ss.displayName);
                sess->setIconKey(ss.exePath);
                sess->setVolumeInternal(ss.volume);
                sess->setMutedInternal(ss.muted);
                sess->setActiveInternal(ss.active);
                map.insert(key, sess);
            } else {
                sess->setDisplayName(ss.displayName);
                sess->setIconKey(ss.exePath);
                sess->setVolumeInternal(ss.volume);
                sess->setMutedInternal(ss.muted);
                sess->setActiveInternal(ss.active);
            }

            // Ensure it's present in the model (it may exist in cache but be filtered out previously).
            if (dev->sessionsModelTyped()->indexOf(sess->pid(), sess->exePath()) < 0) {
                dev->sessionsModelTyped()->insertSession(dev->sessionsModelTyped()->rowCount(), sess);
                anyProcessesChanged = true;
            }
        }

        // Remove missing sessions.
        const auto existingKeys = map.keys();
        for (const auto &k : existingKeys) {
            if (keepSessions.contains(k))
                continue;
            AudioSession *sess = map.value(k, nullptr);
            if (!sess)
                continue;
            const int row = dev->sessionsModelTyped()->indexOf(sess->pid(), sess->exePath());
            if (row >= 0) {
                dev->sessionsModelTyped()->removeSessionAt(row);
                anyProcessesChanged = true;
            }
            map.remove(k);
            delete sess;
        }
    }

    // Apply user-defined device order (from config), keeping any remaining devices after.
    if (m_config && m_deviceModel) {
        QStringList desired;
        const QStringList cfgOrder = m_config->deviceOrder();
        QSet<QString> inDesired;
        for (const auto &id : cfgOrder) {
            if (keepDeviceIds.contains(id) && m_deviceModel->indexOfDeviceId(id) >= 0) {
                desired.append(id);
                inDesired.insert(id);
            }
        }
        // Append the rest in current model order while keeping output/input groups independent.
        auto appendRemaining = [&](DeviceDirection direction) {
            for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
                auto *d = m_deviceModel->deviceAt(i);
                if (!d)
                    continue;
                if (d->direction() != direction)
                    continue;
                if (!keepDeviceIds.contains(d->id()))
                    continue;
                if (inDesired.contains(d->id()))
                    continue;
                desired.append(d->id());
                inDesired.insert(d->id());
            }
            for (const auto &ds : devices) {
                if (ds.direction != direction)
                    continue;
                if (!keepDeviceIds.contains(ds.id))
                    continue;
                if (inDesired.contains(ds.id))
                    continue;
                desired.append(ds.id);
                inDesired.insert(ds.id);
            }
        };
        appendRemaining(DeviceDirection::Output);
        appendRemaining(DeviceDirection::Input);

        // Reorder model to match desired list.
        for (int targetRow = 0; targetRow < desired.size(); ++targetRow) {
            const int curRow = m_deviceModel->indexOfDeviceId(desired.at(targetRow));
            if (curRow >= 0 && curRow != targetRow) {
                m_deviceModel->moveDevice(curRow, targetRow);
                anyDevicesChanged = true;
            }
        }
    }

    // Remove devices no longer present/visible.
    const auto existingDeviceIds = m_deviceById.keys();
    for (const auto &id : existingDeviceIds) {
        if (keepDeviceIds.contains(id))
            continue;
        AudioDevice *dev = m_deviceById.value(id, nullptr);
        if (!dev)
            continue;

        auto removedSessions = m_sessionByKeyByDevice.take(id);
        for (auto it = removedSessions.begin(); it != removedSessions.end(); ++it) {
            delete it.value();
        }

        const int row = m_deviceModel->indexOfDeviceId(id);
        if (row >= 0)
            m_deviceModel->removeDeviceAt(row);
        m_deviceById.remove(id);
        delete dev;
        anyDevicesChanged = true;
    }

    // Update cached default device info (even if default device is hidden / not currently displayed).
    const bool defaultChanged =
        (m_hasDefaultDevice != foundDefault
         || m_defaultDeviceId != defId
         || m_defaultDeviceName != defName
         || !qFuzzyCompare(m_defaultDeviceVolume, defVol)
         || m_defaultDeviceMuted != defMuted);

    if (defaultChanged) {
        m_hasDefaultDevice = foundDefault;
        m_defaultDeviceId = defId;
        m_defaultDeviceName = defName;
        m_defaultDeviceVolume = defVol;
        m_defaultDeviceMuted = defMuted;
    }

    if (anyProcessesChanged && m_iconCache) {
        QSet<QString> keep;
        for (auto devIt = m_sessionByKeyByDevice.constBegin(); devIt != m_sessionByKeyByDevice.constEnd(); ++devIt) {
            const auto &sessions = devIt.value();
            for (auto it = sessions.constBegin(); it != sessions.constEnd(); ++it) {
                auto *sess = it.value();
                if (sess && !sess->exePath().isEmpty())
                    keep.insert(sess->exePath());
            }
        }
        m_iconCache->purgeUnused(keep);
    }

    rebuildMenusIfChanged(anyDevicesChanged, anyProcessesChanged, defaultChanged);
}

void AudioBackend::moveDeviceBefore(const QString &movingDeviceId, const QString &beforeDeviceId)
{
    if (!m_deviceModel || !m_config)
        return;
    if (movingDeviceId.isEmpty() || beforeDeviceId.isEmpty() || movingDeviceId == beforeDeviceId)
        return;

    const int from = m_deviceModel->indexOfDeviceId(movingDeviceId);
    const int to = m_deviceModel->indexOfDeviceId(beforeDeviceId);
    if (from < 0 || to < 0 || from == to)
        return;

    m_deviceModel->moveDevice(from, to);

    // Persist new order.
    QStringList order;
    order.reserve(m_deviceModel->rowCount());
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        auto *d = m_deviceModel->deviceAt(i);
        if (d)
            order.append(d->id());
    }
    m_config->setDeviceOrder(order);

    emit devicesChanged();
}

void AudioBackend::moveDeviceToIndex(const QString &movingDeviceId, int toIndex)
{
    if (!m_deviceModel || !m_config)
        return;
    if (movingDeviceId.isEmpty())
        return;

    const int from = m_deviceModel->indexOfDeviceId(movingDeviceId);
    if (from < 0)
        return;

    if (toIndex < 0)
        toIndex = 0;
    if (toIndex >= m_deviceModel->rowCount())
        toIndex = m_deviceModel->rowCount() - 1;
    if (from == toIndex)
        return;

    m_deviceModel->moveDevice(from, toIndex);

    QStringList order;
    order.reserve(m_deviceModel->rowCount());
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        auto *d = m_deviceModel->deviceAt(i);
        if (d)
            order.append(d->id());
    }
    m_config->setDeviceOrder(order);
    emit devicesChanged();
}

void AudioBackend::moveDeviceToIndexInDirection(const QString &movingDeviceId, bool isInput, int toIndex)
{
    if (!m_deviceModel || !m_config)
        return;
    if (movingDeviceId.isEmpty())
        return;

    AudioDevice *movingDevice = m_deviceById.value(movingDeviceId, nullptr);
    if (!movingDevice || movingDevice->isInput() != isInput)
        return;

    QVector<int> rows;
    rows.reserve(m_deviceModel->rowCount());
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        auto *device = m_deviceModel->deviceAt(i);
        if (device && device->isInput() == isInput)
            rows.push_back(i);
    }
    if (rows.isEmpty())
        return;

    const int from = m_deviceModel->indexOfDeviceId(movingDeviceId);
    if (from < 0)
        return;

    toIndex = qBound(0, toIndex, rows.size() - 1);
    const int targetRow = rows.at(toIndex);
    if (from == targetRow)
        return;

    m_deviceModel->moveDevice(from, targetRow);

    QStringList order;
    order.reserve(m_deviceModel->rowCount());
    for (int i = 0; i < m_deviceModel->rowCount(); ++i) {
        auto *d = m_deviceModel->deviceAt(i);
        if (d)
            order.append(d->id());
    }
    m_config->setDeviceOrder(order);
    emit devicesChanged();
}

QVector<AudioBackend::DeviceSnapshot> AudioBackend::devicesSnapshot() const
{
    QVector<DeviceSnapshot> out;
    for (int i = 0; i < (m_deviceModel ? m_deviceModel->rowCount() : 0); ++i) {
        auto *d = m_deviceModel->deviceAt(i);
        if (!d)
            continue;
        out.push_back({ d->id(), d->name(), d->direction() });
    }
    return out;
}

QVector<AudioBackend::DeviceSnapshot> AudioBackend::devicesSnapshotAll() const
{
    QVector<DeviceSnapshot> out;
    out.reserve(m_lastSnapshot.size());
    for (const auto &ds : m_lastSnapshot) {
        if (ds.id.isEmpty())
            continue;
        out.push_back({ ds.id, ds.name, ds.direction });
    }
    return out;
}

QVector<AudioBackend::ProcessSnapshot> AudioBackend::knownProcessesSnapshot() const
{
    QHash<QString, QString> uniq;
    for (const auto &ds : m_lastSnapshot) {
        for (const auto &ss : ds.sessions) {
            if (ss.exePath.isEmpty())
                continue;
            uniq.insert(ss.exePath, ss.displayName);
        }
    }
    QVector<ProcessSnapshot> out;
    out.reserve(uniq.size());
    for (auto it = uniq.begin(); it != uniq.end(); ++it) {
        out.push_back({ it.key(), it.value().isEmpty() ? it.key() : it.value() });
    }
    return out;
}

QVector<AudioBackend::ProcessSnapshot> AudioBackend::knownProcessesForDeviceSnapshot(const QString &deviceId) const
{
    QHash<QString, QString> uniq;
    const auto sessions = m_sessionByKeyByDevice.value(deviceId);
    for (auto *s : sessions) {
        if (!s)
            continue;
        const QString exe = s->exePath();
        if (exe.isEmpty())
            continue;
        uniq.insert(exe, s->displayName());
    }
    QVector<ProcessSnapshot> out;
    out.reserve(uniq.size());
    for (auto it = uniq.begin(); it != uniq.end(); ++it) {
        out.push_back({ it.key(), it.value().isEmpty() ? it.key() : it.value() });
    }
    return out;
}

QVector<AudioBackend::ProcessSnapshot> AudioBackend::knownProcessesForDeviceSnapshotAll(const QString &deviceId) const
{
    QHash<QString, QString> uniq;
    for (const auto &ds : m_lastSnapshot) {
        if (ds.id != deviceId)
            continue;
        for (const auto &ss : ds.sessions) {
            if (ss.exePath.isEmpty())
                continue;
            uniq.insert(ss.exePath, ss.displayName);
        }
        break;
    }
    QVector<ProcessSnapshot> out;
    out.reserve(uniq.size());
    for (auto it = uniq.begin(); it != uniq.end(); ++it) {
        out.push_back({ it.key(), it.value().isEmpty() ? it.key() : it.value() });
    }
    return out;
}

void AudioBackend::preloadIconsForActiveSessions(int maxCount)
{
    if (!m_iconCache)
        return;
    if (maxCount <= 0)
        return;

    QSet<QString> seen;
    int loaded = 0;
    for (const auto &ds : m_lastSnapshot) {
        for (const auto &ss : ds.sessions) {
            if (ss.exePath.isEmpty())
                continue;
            if (seen.contains(ss.exePath))
                continue;
            seen.insert(ss.exePath);
            m_iconCache->ensureIconForExePath(ss.exePath);
            loaded++;
            if (loaded >= maxCount)
                return;
        }
    }
}

void AudioBackend::setDeviceVolume(const QString &deviceId, double volume01)
{
    if (auto *d = m_deviceById.value(deviceId, nullptr))
        d->setVolumeInternal(volume01);

    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setDeviceVolume", Qt::QueuedConnection,
                                  Q_ARG(QString, deviceId), Q_ARG(double, volume01));
}

void AudioBackend::setDeviceMuted(const QString &deviceId, bool muted)
{
    if (auto *d = m_deviceById.value(deviceId, nullptr))
        d->setMutedInternal(muted);

    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setDeviceMuted", Qt::QueuedConnection,
                                  Q_ARG(QString, deviceId), Q_ARG(bool, muted));
}

void AudioBackend::setSessionVolume(const QString &deviceId, quint32 pid, const QString &exePath, double volume01)
{
    const QString key = sessionKeyStr(pid, exePath);
    if (auto *s = m_sessionByKeyByDevice.value(deviceId).value(key, nullptr))
        s->setVolumeInternal(volume01);

    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setSessionVolume", Qt::QueuedConnection,
                                  Q_ARG(QString, deviceId), Q_ARG(quint32, pid),
                                  Q_ARG(QString, exePath), Q_ARG(double, volume01));
}

void AudioBackend::setSessionMuted(const QString &deviceId, quint32 pid, const QString &exePath, bool muted)
{
    const QString key = sessionKeyStr(pid, exePath);
    if (auto *s = m_sessionByKeyByDevice.value(deviceId).value(key, nullptr))
        s->setMutedInternal(muted);

    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setSessionMuted", Qt::QueuedConnection,
                                  Q_ARG(QString, deviceId), Q_ARG(quint32, pid),
                                  Q_ARG(QString, exePath), Q_ARG(bool, muted));
}

void AudioBackend::setDeviceColorKey(const QString &deviceId, const QString &colorKey)
{
    if (auto *d = m_deviceById.value(deviceId, nullptr))
        d->setColorKey(colorKey);
}

void AudioBackend::rebuildMenusIfChanged(bool devicesChangedNow, bool processesChangedNow, bool defaultDeviceChangedNow)
{
    if (devicesChangedNow) {
        qInfo() << "Emitting devicesChanged";
        emit devicesChanged();
    }
    if (processesChangedNow) {
        qInfo() << "Emitting knownProcessesChanged";
        emit knownProcessesChanged();
    }
    if (defaultDeviceChangedNow) {
        qInfo() << "Emitting defaultDeviceChanged";
        emit defaultDeviceChanged();
    }
}


