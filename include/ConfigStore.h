#pragma once

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>

class ConfigStore final : public QObject
{
    Q_OBJECT
public:
    enum class Mode { DefaultDeviceOnly, AllDevices };
    Q_ENUM(Mode)

    explicit ConfigStore(QObject *parent = nullptr);

    void load();
    void save() const;

    QString configPath() const;

    Mode mode() const { return m_mode; }
    void setMode(Mode m);

    bool showSystemSessions() const { return m_showSystemSessions; }
    void setShowSystemSessions(bool v);

    bool showProcessStatusOnHover() const { return m_showProcessStatusOnHover; }
    void setShowProcessStatusOnHover(bool v);

    bool isDeviceHidden(const QString &deviceId) const;
    void setDeviceHidden(const QString &deviceId, bool hidden);
    QStringList hiddenDevices() const;
    QStringList deviceOrder() const { return m_deviceOrder; }
    void setDeviceOrder(const QStringList &order);

    bool isProcessHiddenGlobal(const QString &exePath) const;
    void setProcessHiddenGlobal(const QString &exePath, bool hidden);

    bool isProcessHiddenForDevice(const QString &deviceId, const QString &exePath) const;
    void setProcessHiddenForDevice(const QString &deviceId, const QString &exePath, bool hidden);

signals:
    void changed();

private:
    Mode m_mode = Mode::DefaultDeviceOnly;
    bool m_showSystemSessions = false;
    bool m_showProcessStatusOnHover = false;

    QSet<QString> m_hiddenDevices;
    QSet<QString> m_hiddenProcessesGlobal; // exePath
    QHash<QString, QSet<QString>> m_hiddenProcessesPerDevice; // deviceId -> exePaths

    QStringList m_deviceOrder; // ordered list of deviceIds
};


