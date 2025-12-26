#include "SessionListModel.h"

#include "AudioSession.h"

SessionListModel::SessionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SessionListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_sessions.size();
}

QVariant SessionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sessions.size())
        return {};
    AudioSession *s = m_sessions.at(index.row());
    if (!s)
        return {};

    switch (role) {
    case SessionObjectRole: return QVariant::fromValue(static_cast<QObject *>(s));
    case PidRole: return s->pid();
    case ExePathRole: return s->exePath();
    case DisplayNameRole: return s->displayName();
    case IconKeyRole: return s->iconKey();
    case VolumeRole: return s->volume();
    case MutedRole: return s->muted();
    default: return {};
    }
}

QHash<int, QByteArray> SessionListModel::roleNames() const
{
    return {
        { SessionObjectRole, "sessionObject" },
        { PidRole, "pid" },
        { ExePathRole, "exePath" },
        { DisplayNameRole, "displayName" },
        { IconKeyRole, "iconKey" },
        { VolumeRole, "volume" },
        { MutedRole, "muted" }
    };
}

AudioSession *SessionListModel::sessionAt(int row) const
{
    if (row < 0 || row >= m_sessions.size())
        return nullptr;
    return m_sessions.at(row);
}

int SessionListModel::indexOf(quint32 pid, const QString &exePath) const
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        AudioSession *s = m_sessions.at(i);
        if (s && s->pid() == pid && s->exePath() == exePath)
            return i;
    }
    return -1;
}

void SessionListModel::insertSession(int row, AudioSession *session)
{
    if (!session)
        return;
    row = qBound(0, row, m_sessions.size());
    beginInsertRows(QModelIndex(), row, row);
    m_sessions.insert(row, session);
    endInsertRows();
}

void SessionListModel::removeSessionAt(int row)
{
    if (row < 0 || row >= m_sessions.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_sessions.removeAt(row);
    endRemoveRows();
}

void SessionListModel::clear()
{
    if (m_sessions.isEmpty())
        return;
    beginRemoveRows(QModelIndex(), 0, m_sessions.size() - 1);
    m_sessions.clear();
    endRemoveRows();
}


