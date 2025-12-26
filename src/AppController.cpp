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

#include <windows.h>

AppController::AppController(QObject *parent)
    : QObject(parent)
{
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

    connect(m_audio, &AudioBackend::devicesChanged, this, &AppController::rebuildHiddenMenus);
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
    positionFlyout();
    m_view->show();
    m_view->requestActivate();
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

    applyWindowEffectsIfPossible();
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
    m_tray.setIcon(QIcon::fromTheme(QStringLiteral("audio-volume-high")));
    m_tray.show();

    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            toggleFlyout(); // left-click
        }
    });
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

        int x = trayGeom.right() - w;
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
    const auto devices = m_audio->devicesSnapshot();
    if (devices.isEmpty()) {
        QAction *a = m_hiddenDevicesMenu->addAction(tr("(No devices)"));
        a->setEnabled(false);
    } else {
        for (const auto &d : devices) {
            QAction *a = m_hiddenDevicesMenu->addAction(d.name);
            a->setCheckable(true);
            a->setChecked(m_config->isDeviceHidden(d.id));
            connect(a, &QAction::toggled, this, [this, id = d.id](bool checked) {
                m_config->setDeviceHidden(id, checked);
                m_audio->refresh(); // re-filter
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

    for (const auto &d : devices) {
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


