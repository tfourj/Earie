#pragma once

#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>

class QMenu;
class QAction;
class QQuickView;

class AudioBackend;
class ConfigStore;

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool allDevices READ allDevices WRITE setAllDevices NOTIFY allDevicesChanged)
    Q_PROPERTY(bool showSystemSessions READ showSystemSessions WRITE setShowSystemSessions NOTIFY showSystemSessionsChanged)
    Q_PROPERTY(bool showProcessStatusOnHover READ showProcessStatusOnHover WRITE setShowProcessStatusOnHover NOTIFY showProcessStatusOnHoverChanged)
    Q_PROPERTY(bool scrollWheelVolumeOnHover READ scrollWheelVolumeOnHover WRITE setScrollWheelVolumeOnHover NOTIFY scrollWheelVolumeOnHoverChanged)
public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    bool init();

    bool allDevices() const { return m_allDevices; }
    void setAllDevices(bool v);

    bool showSystemSessions() const { return m_showSystemSessions; }
    void setShowSystemSessions(bool v);

    bool showProcessStatusOnHover() const { return m_showProcessStatusOnHover; }
    void setShowProcessStatusOnHover(bool v);

    bool scrollWheelVolumeOnHover() const { return m_scrollWheelVolumeOnHover; }
    void setScrollWheelVolumeOnHover(bool v);

public slots:
    Q_INVOKABLE void toggleFlyout();
    Q_INVOKABLE void showFlyout();
    Q_INVOKABLE void hideFlyout();
    // Called by QML when content height changes (e.g. sessions hidden/unhidden).
    Q_INVOKABLE void requestRelayout();
    Q_INVOKABLE void setProcessHiddenGlobal(const QString &exePath, bool hidden);
    Q_INVOKABLE void setProcessHiddenForDevice(const QString &deviceId, const QString &exePath, bool hidden);

signals:
    void allDevicesChanged();
    void showSystemSessionsChanged();
    void showProcessStatusOnHoverChanged();
    void scrollWheelVolumeOnHoverChanged();

private slots:
    void rebuildHiddenMenus();

private:
    void buildTray();
    void buildFlyout();
    void positionFlyout();
    void adjustFlyoutHeightToContent();
    void applyWindowEffectsIfPossible();
    void updateTrayIcon();

    bool eventFilter(QObject *watched, QEvent *event) override;

    QSystemTrayIcon m_tray;
    QPointer<QMenu> m_menu;
    QPointer<QMenu> m_hiddenDevicesMenu;
    QPointer<QMenu> m_hiddenProcessesMenu;

    QAction *m_actionOpen = nullptr;
    QAction *m_actionQuit = nullptr;
    QAction *m_actionDefaultOnly = nullptr;
    QAction *m_actionAllDevices = nullptr;

    QPointer<QQuickView> m_view;

    QPointer<AudioBackend> m_audio;
    QPointer<ConfigStore> m_config;

    bool m_allDevices = false;
    bool m_showSystemSessions = false;
    bool m_showProcessStatusOnHover = false;
    bool m_scrollWheelVolumeOnHover = false;

    QTimer m_trayIconCoalesce;
    int m_pendingTrayVolPct = -1;
    bool m_pendingTrayMuted = false;
    int m_lastTrayVolPct = -1;
    bool m_lastTrayMuted = false;

    // When the flyout closes due to WindowDeactivate (e.g. clicking the tray icon),
    // the tray "activated" signal may arrive right after and would re-open it.
    // We suppress the next toggle for a short window to make click-to-close reliable.
    bool m_suppressNextTrayToggle = false;
    QTimer m_trayToggleSuppressTimer;

    // Coalesce repeated relayout requests while QML is settling.
    QTimer m_relayoutCoalesce;
};


