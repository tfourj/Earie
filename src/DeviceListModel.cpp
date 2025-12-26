#include "DeviceListModel.h"

#include "AudioDevice.h"
#include "SessionListModel.h"

DeviceListModel::DeviceListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DeviceListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_devices.size();
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
        return {};
    AudioDevice *d = m_devices.at(index.row());
    if (!d)
        return {};

    switch (role) {
    case DeviceObjectRole: return QVariant::fromValue(static_cast<QObject *>(d));
    case DeviceIdRole: return d->id();
    case NameRole: return d->name();
    case IsDefaultRole: return d->isDefault();
    case VolumeRole: return d->volume();
    case MutedRole: return d->muted();
    case SessionsModelRole: return QVariant::fromValue(static_cast<QObject *>(d->sessionsModel()));
    default: return {};
    }
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        { DeviceObjectRole, "deviceObject" },
        { DeviceIdRole, "deviceId" },
        { NameRole, "name" },
        { IsDefaultRole, "isDefault" },
        { VolumeRole, "volume" },
        { MutedRole, "muted" },
        { SessionsModelRole, "sessionsModel" }
    };
}

AudioDevice *DeviceListModel::deviceAt(int row) const
{
    if (row < 0 || row >= m_devices.size())
        return nullptr;
    return m_devices.at(row);
}

int DeviceListModel::indexOfDeviceId(const QString &deviceId) const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices.at(i) && m_devices.at(i)->id() == deviceId)
            return i;
    }
    return -1;
}

void DeviceListModel::insertDevice(int row, AudioDevice *device)
{
    if (!device)
        return;
    row = qBound(0, row, m_devices.size());
    beginInsertRows(QModelIndex(), row, row);
    m_devices.insert(row, device);
    endInsertRows();
}

void DeviceListModel::removeDeviceAt(int row)
{
    if (row < 0 || row >= m_devices.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_devices.removeAt(row);
    endRemoveRows();
}

void DeviceListModel::clear()
{
    if (m_devices.isEmpty())
        return;
    beginRemoveRows(QModelIndex(), 0, m_devices.size() - 1);
    m_devices.clear();
    endRemoveRows();
}


