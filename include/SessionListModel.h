#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QVector>

class AudioSession;

class SessionListModel final : public QAbstractListModel
{
public:
    enum Roles {
        SessionObjectRole = Qt::UserRole + 1,
        PidRole,
        ExePathRole,
        DisplayNameRole,
        IconKeyRole,
        VolumeRole,
        MutedRole
    };

    explicit SessionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVector<AudioSession *> sessions() const { return m_sessions; }
    AudioSession *sessionAt(int row) const;
    int indexOf(quint32 pid, const QString &exePath) const;

    void insertSession(int row, AudioSession *session);
    void removeSessionAt(int row);
    void clear();

private:
    QVector<AudioSession *> m_sessions;
};


