#include "DeviceDirectionProxyModel.h"

#include "DeviceListModel.h"

DeviceDirectionProxyModel::DeviceDirectionProxyModel(bool wantInput, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_wantInput(wantInput)
{
}

bool DeviceDirectionProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel() ? sourceModel()->index(sourceRow, 0, sourceParent) : QModelIndex();
    if (!index.isValid())
        return false;
    return index.data(DeviceListModel::IsInputRole).toBool() == m_wantInput;
}
