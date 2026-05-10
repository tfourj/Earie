#pragma once

#include <QSortFilterProxyModel>

class DeviceDirectionProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit DeviceDirectionProxyModel(bool wantInput, QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool m_wantInput = false;
};
