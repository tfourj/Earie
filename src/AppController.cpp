#include "AppController.h"

#include "AudioBackend.h"
#include "DeviceListModel.h"
#include "IconCache.h"
#include "ConfigStore.h"
#include "WinAcrylic.h"
#include "WinTrayPositioner.h"

#include <QAction>
#include <QEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QTimer>

#include <windows.h>

static QIcon makeEarieTrayIcon(double volume01, bool muted);

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_trayIconCoalesce.setSingleShot(true);
    m_trayIconCoalesce.setInterval(60);
    m_trayIconCoalesce.setParent(this);
    connect(&m_trayIconCoalesce, &QTimer::timeout, this, [this]() {
        if (m_pendingTrayVolPct < 0)
            return;
        const int pct = m_pendingTrayVolPct;
        const bool muted = m_pendingTrayMuted;
        m_pendingTrayVolPct = -1;

        if (pct == m_lastTrayVolPct && muted == m_lastTrayMuted)
            return;
        m_lastTrayVolPct = pct;
        m_lastTrayMuted = muted;

        m_tray.setIcon(makeEarieTrayIcon(pct / 100.0, muted));
    });
}

AppController::~AppController()
{
    if (m_view) {
        m_view->removeEventFilter(this);
    }
}

bool AppController::init()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return false;

    m_config = new ConfigStore(this);
    m_config->load();

    m_allDevices = (m_config->mode() == ConfigStore::Mode::AllDevices);
    m_showSystemSessions = m_config->showSystemSessions();

    m_audio = new AudioBackend(this);
    m_audio->setConfig(m_config);
    m_audio->setAllDevices(m_allDevices);
    m_audio->setShowSystemSessions(m_showSystemSessions);
    m_audio->start();

    buildFlyout();
    buildTray();
    rebuildHiddenMenus();
    updateTrayIcon();

    connect(m_audio, &AudioBackend::devicesChanged, this, &AppController::rebuildHiddenMenus);
    connect(m_audio, &AudioBackend::devicesChanged, this, &AppController::updateTrayIcon);
    connect(m_audio, &AudioBackend::knownProcessesChanged, this, &AppController::rebuildHiddenMenus);
    return true;
}

void AppController::setAllDevices(bool v)
{
    if (m_allDevices == v)
        return;
    m_allDevices = v;
    if (m_config)
        m_config->setMode(m_allDevices ? ConfigStore::Mode::AllDevices : ConfigStore::Mode::DefaultDeviceOnly);
    if (m_audio)
        m_audio->setAllDevices(m_allDevices);
    emit allDevicesChanged();
}

void AppController::setShowSystemSessions(bool v)
{
    if (m_showSystemSessions == v)
        return;
    m_showSystemSessions = v;
    if (m_config)
        m_config->setShowSystemSessions(m_showSystemSessions);
    if (m_audio)
        m_audio->setShowSystemSessions(m_showSystemSessions);
    emit showSystemSessionsChanged();
}

void AppController::toggleFlyout()
{
    if (!m_view)
        return;
    if (m_view->isVisible())
        hideFlyout();
    else
        showFlyout();
}

void AppController::showFlyout()
{
    if (!m_view)
        return;
    adjustFlyoutHeightToContent();
    positionFlyout();
    m_view->show();
    m_view->requestActivate();

    // QML layout/delegates may settle a tick later; re-measure after show.
    QTimer::singleShot(0, this, [this]() {
        if (!m_view || !m_view->isVisible())
            return;
        adjustFlyoutHeightToContent();
        positionFlyout();
    });
    QTimer::singleShot(50, this, [this]() {
        if (!m_view || !m_view->isVisible())
            return;
        adjustFlyoutHeightToContent();
        positionFlyout();
    });
}

void AppController::hideFlyout()
{
    if (!m_view)
        return;
    m_view->hide();
}

void AppController::buildFlyout()
{
    m_view = new QQuickView;
    m_view->setResizeMode(QQuickView::SizeRootObjectToView);
    m_view->setColor(Qt::transparent);
    m_view->setFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);

    // Avoid taskbar entry (Tool helps) and let us close on focus loss.
    m_view->installEventFilter(this);

    m_view->rootContext()->setContextProperty(QStringLiteral("appController"), this);
    m_view->rootContext()->setContextProperty(QStringLiteral("audioBackend"), m_audio);
    m_view->rootContext()->setContextProperty(
        QStringLiteral("deviceModel"),
        m_audio ? static_cast<QObject *>(m_audio->deviceModel()) : nullptr);
    if (m_audio && m_audio->iconCache()) {
        m_view->engine()->addImageProvider(QStringLiteral("appicon"), m_audio->iconCache());
    }

    m_view->setSource(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    m_view->setWidth(420);
    m_view->setHeight(520);
    m_view->setMinimumWidth(420);
    m_view->setMaximumWidth(420);

    applyWindowEffectsIfPossible();
}

void AppController::adjustFlyoutHeightToContent()
{
    if (!m_view)
        return;

    // Best-effort read of QML content height hint.
    QObject *root = m_view->rootObject();
    int hint = 0;
    if (root) {
        const QVariant v = root->property("contentHeightHint");
        if (v.isValid())
            hint = v.toInt();
    }

    // Clamp to screen work area (prevents going off-screen).
    QRect trayGeom = m_tray.geometry();
    QScreen *screen = trayGeom.isValid() ? QGuiApplication::screenAt(trayGeom.center()) : QGuiApplication::primaryScreen();
    QRect work = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    const int margin = 12;
    const int maxH = qMax(240, work.height() - margin * 2);
    const int minH = 240;

    int desired = hint > 0 ? hint : 520;
    desired = qBound(minH, desired, maxH);

    // Apply. Width is already fixed.
    m_view->setMinimumHeight(desired);
    m_view->setMaximumHeight(desired);
    m_view->resize(m_view->width(), desired);
}

void AppController::applyWindowEffectsIfPossible()
{
    if (!m_view)
        return;
    HWND hwnd = reinterpret_cast<HWND>(m_view->winId());
    WinAcrylic::enableAcrylic(hwnd);
}

void AppController::buildTray()
{
    m_menu = new QMenu;

    m_actionOpen = m_menu->addAction(tr("Open mixer"));
    connect(m_actionOpen, &QAction::triggered, this, &AppController::showFlyout);

    m_menu->addSeparator();

    QAction *modeHeader = m_menu->addAction(tr("Mode"));
    modeHeader->setEnabled(false);

    m_actionDefaultOnly = m_menu->addAction(tr("Default device only"));
    m_actionDefaultOnly->setCheckable(true);
    m_actionAllDevices = m_menu->addAction(tr("All devices"));
    m_actionAllDevices->setCheckable(true);

    auto syncModeChecks = [this]() {
        if (!m_actionDefaultOnly || !m_actionAllDevices)
            return;
        m_actionDefaultOnly->setChecked(!m_allDevices);
        m_actionAllDevices->setChecked(m_allDevices);
    };
    syncModeChecks();

    connect(m_actionDefaultOnly, &QAction::triggered, this, [this, syncModeChecks]() {
        setAllDevices(false);
        syncModeChecks();
    });
    connect(m_actionAllDevices, &QAction::triggered, this, [this, syncModeChecks]() {
        setAllDevices(true);
        syncModeChecks();
    });

    m_menu->addSeparator();

    m_actionShowSystem = m_menu->addAction(tr("Show system sessions"));
    m_actionShowSystem->setCheckable(true);
    m_actionShowSystem->setChecked(m_showSystemSessions);
    connect(m_actionShowSystem, &QAction::toggled, this, &AppController::setShowSystemSessions);

    m_menu->addSeparator();

    m_hiddenDevicesMenu = m_menu->addMenu(tr("Hidden devices…"));
    m_hiddenProcessesMenu = m_menu->addMenu(tr("Hidden processes…"));

    m_menu->addSeparator();
    m_actionQuit = m_menu->addAction(tr("Quit"));
    connect(m_actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_tray.setToolTip(QStringLiteral("Earie"));
    m_tray.setContextMenu(m_menu);
    // Set a default icon before showing to avoid "No Icon set" warnings.
    m_tray.setIcon(makeEarieTrayIcon(1.0, false));
    m_tray.show();

    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            toggleFlyout(); // left-click
        }
    });
}

static QIcon makeEarieTrayIcon(double volume01, bool muted)
{
    const double v = qBound(0.0, volume01, 1.0);
    const int bars = muted
        ? -1
        : (v < 0.05 ? 0 : (v < 0.33 ? 1 : (v < 0.66 ? 2 : 3)));

    const QString path = (bars < 0)
        ? QStringLiteral(":/assets/vol_m.ico")
        : QStringLiteral(":/assets/vol_%1.ico").arg(bars);

    QIcon icon(path);
    if (icon.isNull()) {
        // Safety fallback (should not happen if resources are embedded)
        icon = QIcon::fromTheme(QStringLiteral("audio-volume-high"));
    }
    return icon;
}

void AppController::updateTrayIcon()
{
    if (!m_audio)
        return;

    double vol = 1.0;
    bool muted = false;
    if (m_audio->hasDefaultDevice()) {
        vol = m_audio->defaultDeviceVolume();
        muted = m_audio->defaultDeviceMuted();
    }
    const int pct = qBound(0, static_cast<int>(qRound(vol * 100.0)), 100);
    m_pendingTrayVolPct = pct;
    m_pendingTrayMuted = muted;
    if (!m_trayIconCoalesce.isActive())
        m_trayIconCoalesce.start();
}

void AppController::positionFlyout()
{
    if (!m_view)
        return;

    // Prefer tray geometry when available.
    QRect trayGeom = m_tray.geometry();
    const int w = m_view->width();
    const int h = m_view->height();

    if (trayGeom.isValid()) {
        QScreen *screen = QGuiApplication::screenAt(trayGeom.center());
        QRect work = screen ? screen->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
        const int margin = 12;

        // User request: keep flyout snapped to the right edge of the screen.
        int x = work.right() - w - margin;
        int y = trayGeom.top() - h - margin;

        // If not enough above, go below.
        if (y < work.top() + margin)
            y = trayGeom.bottom() + margin;

        // Clamp.
        x = qMax(work.left() + margin, qMin(x, work.right() - w - margin));
        y = qMax(work.top() + margin, qMin(y, work.bottom() - h - margin));

        m_view->setPosition(QPoint(x, y));
        return;
    }

    m_view->setPosition(WinTrayPositioner::suggestFlyoutTopLeft(w, h));
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view) {
        if (event->type() == QEvent::WindowDeactivate) {
            // Close on focus loss (EarTrumpet-like).
            hideFlyout();
        }
    }
    return QObject::eventFilter(watched, event);
}

void AppController::rebuildHiddenMenus()
{
    if (!m_hiddenDevicesMenu || !m_hiddenProcessesMenu || !m_audio || !m_config)
        return;

    m_hiddenDevicesMenu->clear();
    m_hiddenProcessesMenu->clear();

    // Hidden devices list (check means hidden).
    const auto devicesAll = m_audio->devicesSnapshotAll();
    const auto devicesVisible = m_audio->devicesSnapshot();
    if (devicesAll.isEmpty()) {
        QAction *a = m_hiddenDevicesMenu->addAction(tr("(No devices)"));
        a->setEnabled(false);
    } else {
        QSet<QString> seen;
        for (const auto &d : devicesAll) {
            if (d.id.isEmpty())
                continue;
            seen.insert(d.id);
            QAction *a = m_hiddenDevicesMenu->addAction(d.name);
            a->setCheckable(true);
            a->setChecked(m_config->isDeviceHidden(d.id));
            connect(a, &QAction::toggled, this, [this, id = d.id](bool checked) {
                m_config->setDeviceHidden(id, checked);
                m_audio->refresh(); // re-filter
            });
        }

        // Also show hidden device IDs that are currently disconnected (so user can unhide).
        for (const auto &hiddenId : m_config->hiddenDevices()) {
            if (hiddenId.isEmpty() || seen.contains(hiddenId))
                continue;
            QAction *a = m_hiddenDevicesMenu->addAction(tr("[disconnected] %1").arg(hiddenId));
            a->setCheckable(true);
            a->setChecked(true);
            connect(a, &QAction::toggled, this, [this, id = hiddenId](bool checked) {
                m_config->setDeviceHidden(id, checked);
                m_audio->refresh();
            });
        }
    }

    // Hidden processes: Global and Per-device.
    QMenu *globalMenu = m_hiddenProcessesMenu->addMenu(tr("Global"));
    QMenu *perDeviceMenu = m_hiddenProcessesMenu->addMenu(tr("Per device"));

    const auto known = m_audio->knownProcessesSnapshot();
    if (known.isEmpty()) {
        QAction *a = globalMenu->addAction(tr("(No processes)"));
        a->setEnabled(false);
    } else {
        for (const auto &p : known) {
            const QString label = p.displayName;
            QAction *a = globalMenu->addAction(label);
            a->setCheckable(true);
            a->setChecked(m_config->isProcessHiddenGlobal(p.exePath));
            connect(a, &QAction::toggled, this, [this, exe = p.exePath](bool checked) {
                m_config->setProcessHiddenGlobal(exe, checked);
                m_audio->refresh();
            });
        }
    }

    for (const auto &d : devicesVisible) {
        QMenu *devMenu = perDeviceMenu->addMenu(d.name);
        const auto perDev = m_audio->knownProcessesForDeviceSnapshot(d.id);
        if (perDev.isEmpty()) {
            QAction *a = devMenu->addAction(tr("(No processes)"));
            a->setEnabled(false);
            continue;
        }
        for (const auto &p : perDev) {
            QAction *a = devMenu->addAction(p.displayName);
            a->setCheckable(true);
            a->setChecked(m_config->isProcessHiddenForDevice(d.id, p.exePath));
            connect(a, &QAction::toggled, this, [this, devId = d.id, exe = p.exePath](bool checked) {
                m_config->setProcessHiddenForDevice(devId, exe, checked);
                m_audio->refresh();
            });
        }
    }
}


