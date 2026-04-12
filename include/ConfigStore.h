#pragma once

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

class ConfigStore final : public QObject
{
    Q_OBJECT
public:
    enum class Mode { DefaultDeviceOnly, AllDevices };
    Q_ENUM(Mode)
    enum class TrayIconMode { White, Black };
    Q_ENUM(TrayIconMode)

    explicit ConfigStore(QObject *parent = nullptr);

    void load();
    void save() const;

    QString configPath() const;

    Mode mode() const { return m_mode; }
    void setMode(Mode m);

    bool showSystemSessions() const { return m_showSystemSessions; }
    void setShowSystemSessions(bool v);

    bool showInputDevices() const { return m_showInputDevices; }
    void setShowInputDevices(bool v);

    bool showInputApplications() const { return m_showInputApplications; }
    void setShowInputApplications(bool v);

    bool showProcessStatusOnHover() const { return m_showProcessStatusOnHover; }
    void setShowProcessStatusOnHover(bool v);

    bool scrollWheelVolumeOnHover() const { return m_scrollWheelVolumeOnHover; }
    void setScrollWheelVolumeOnHover(bool v);

    bool debugMode() const { return m_debugMode; }
    void setDebugMode(bool v);

    bool startWithWindows() const { return m_startWithWindows; }
    void setStartWithWindows(bool v);

    bool useNativeTrayIcon() const { return m_useNativeTrayIcon; }
    void setUseNativeTrayIcon(bool v);

    TrayIconMode trayIconMode() const { return m_trayIconMode; }
    void setTrayIconMode(TrayIconMode v);

    bool isDeviceHidden(const QString &deviceId) const;
    void setDeviceHidden(const QString &deviceId, bool hidden);
    QStringList hiddenDevices() const;
    QString hiddenDeviceName(const QString &deviceId) const;
    QString deviceName(const QString &deviceId) const;
    void rememberDeviceName(const QString &deviceId, const QString &deviceName);
    bool remapDeviceId(const QString &oldDeviceId, const QString &newDeviceId, const QString &newDeviceName = QString());
    QStringList deviceOrder() const { return m_deviceOrder; }
    void setDeviceOrder(const QStringList &order);
    QString deviceColor(const QString &deviceId) const;
    void setDeviceColor(const QString &deviceId, const QString &colorKey);
    QStringList rememberedDeviceIds() const;

    bool isProcessHiddenGlobal(const QString &exePath) const;
    void setProcessHiddenGlobal(const QString &exePath, bool hidden);

    bool isProcessHiddenForDevice(const QString &deviceId, const QString &exePath) const;
    void setProcessHiddenForDevice(const QString &deviceId, const QString &exePath, bool hidden);

    const QSet<QString> &hiddenProcessesGlobalSet() const { return m_hiddenProcessesGlobal; }
    const QHash<QString, QSet<QString>> &hiddenProcessesPerDeviceMap() const { return m_hiddenProcessesPerDevice; }

signals:
    void changed();

private:
    Mode m_mode = Mode::DefaultDeviceOnly;
    bool m_showSystemSessions = false;
    bool m_showInputDevices = false;
    bool m_showInputApplications = true;
    bool m_showProcessStatusOnHover = false;
    bool m_scrollWheelVolumeOnHover = false;
    bool m_debugMode = false;
    bool m_startWithWindows = false;
    bool m_useNativeTrayIcon = false;
    TrayIconMode m_trayIconMode = TrayIconMode::White;

    QSet<QString> m_hiddenDevices;
    QHash<QString, QString> m_deviceNames; // deviceId -> last known name
    QHash<QString, QString> m_deviceColors; // deviceId -> palette key
    QSet<QString> m_hiddenProcessesGlobal; // exePath
    QHash<QString, QSet<QString>> m_hiddenProcessesPerDevice; // deviceId -> exePaths

    QStringList m_deviceOrder; // ordered list of deviceIds
};


