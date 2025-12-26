#pragma once

#include <QAbstractListModel>
#include <QVector>

class AudioDevice;

class DeviceListModel final : public QAbstractListModel
{
public:
    enum Roles {
        DeviceObjectRole = Qt::UserRole + 1,
        DeviceIdRole,
        NameRole,
        IsDefaultRole,
        VolumeRole,
        MutedRole,
        SessionsModelRole
    };

    explicit DeviceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVector<AudioDevice *> devices() const { return m_devices; }
    AudioDevice *deviceAt(int row) const;
    int indexOfDeviceId(const QString &deviceId) const;

    void insertDevice(int row, AudioDevice *device);
    void removeDeviceAt(int row);
    void moveDevice(int fromRow, int toRow);
    void clear();

private:
    QVector<AudioDevice *> m_devices;
};


