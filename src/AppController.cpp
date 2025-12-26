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
#include <QDesktopServices>
#include <QMenu>
#include <QProcess>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QCursor>
#include <QScreen>
#include <QTimer>
#include <QAbstractItemModel>
#include <QWidgetAction>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QVariant>
#include <QHash>
#include <QSet>
#include <QVector>
#include <algorithm>

#include <windows.h>

static QIcon makeEarieTrayIcon(double volume01, bool muted);
static QString trayMenuStyleSheet();
static void openWindowsVolumeMixer();
static void openWindowsPlaybackDevices();
static void openWindowsSoundSettings();

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

    m_trayToggleSuppressTimer.setSingleShot(true);
    m_trayToggleSuppressTimer.setInterval(200);
    m_trayToggleSuppressTimer.setParent(this);
    connect(&m_trayToggleSuppressTimer, &QTimer::timeout, this, [this]() {
        m_suppressNextTrayToggle = false;
    });

    m_relayoutCoalesce.setSingleShot(true);
    m_relayoutCoalesce.setInterval(30);
    m_relayoutCoalesce.setParent(this);
    connect(&m_relayoutCoalesce, &QTimer::timeout, this, [this]() {
        if (!m_view || !m_view->isVisible())
            return;
        adjustFlyoutHeightToContent();
        positionFlyout();
    });
}

AppController::~AppController()
{
    if (m_view) {
        m_view->removeEventFilter(this);
    }
    if (m_hiddenView) {
        m_hiddenView->removeEventFilter(this);
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
    m_showProcessStatusOnHover = m_config->showProcessStatusOnHover();
    m_scrollWheelVolumeOnHover = m_config->scrollWheelVolumeOnHover();
    m_startWithWindows = m_config->startWithWindows();
    applyStartWithWindows(m_startWithWindows);

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
    connect(m_audio, &AudioBackend::defaultDeviceChanged, this, &AppController::updateTrayIcon);
    connect(m_audio, &AudioBackend::knownProcessesChanged, this, &AppController::rebuildHiddenMenus);
    connect(m_audio, &AudioBackend::devicesChanged, this, [this]() { emit hiddenItemsChanged(); });
    connect(m_audio, &AudioBackend::knownProcessesChanged, this, [this]() { emit hiddenItemsChanged(); });

    // If the user clicks the desktop while a QML menu is open, the flyout is already deactivated
    // (because the menu is its own native window), so WindowDeactivate won't fire again.
    // Close popups + flyout on app deactivation to match expected behavior.
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            return;
        const bool flyoutVisible = m_view && m_view->isVisible();
        const bool hiddenVisible = m_hiddenView && m_hiddenView->isVisible();
        if (!flyoutVisible && !hiddenVisible)
            return;
        // Clicking the tray icon often deactivates the app before the tray "activated" signal arrives.
        // If we close here without suppression, the next tray click will immediately re-open the flyout.
        if (flyoutVisible) {
            m_suppressNextTrayToggle = true;
            if (!m_trayToggleSuppressTimer.isActive())
                m_trayToggleSuppressTimer.start();
            if (m_popupDepth > 0)
                emit closeAllPopupsRequested();
            hideFlyout();
        }
        if (hiddenVisible) {
            hideHiddenItemsWindow();
        }
    });
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
    
    // Sync tray menu checkboxes
    if (m_actionDefaultOnly && m_actionAllDevices) {
        m_actionDefaultOnly->setChecked(!m_allDevices);
        m_actionAllDevices->setChecked(m_allDevices);
    }
    
    emit allDevicesChanged();
}

QPoint AppController::cursorPos() const
{
    return QCursor::pos();
}

QRect AppController::cursorScreenAvailableGeometry() const
{
    const QPoint p = QCursor::pos();
    QScreen *s = QGuiApplication::screenAt(p);
    if (!s)
        s = QGuiApplication::primaryScreen();
    return s ? s->availableGeometry() : QRect(0, 0, 1920, 1080);
}

QVariantList AppController::hiddenDevicesSnapshot() const
{
    QVariantList out;
    if (!m_audio || !m_config)
        return out;

    const auto devicesAll = m_audio->devicesSnapshotAll();
    QSet<QString> seen;
    for (const auto &d : devicesAll) {
        if (d.id.isEmpty())
            continue;
        seen.insert(d.id);
        QVariantMap m;
        m.insert(QStringLiteral("deviceId"), d.id);
        m.insert(QStringLiteral("name"), d.name.isEmpty() ? d.id : d.name);
        m.insert(QStringLiteral("connected"), true);
        m.insert(QStringLiteral("hidden"), m_config->isDeviceHidden(d.id));
        out.append(m);
    }

    for (const auto &hiddenId : m_config->hiddenDevices()) {
        if (hiddenId.isEmpty() || seen.contains(hiddenId))
            continue;
        QVariantMap m;
        m.insert(QStringLiteral("deviceId"), hiddenId);
        m.insert(QStringLiteral("name"), hiddenId);
        m.insert(QStringLiteral("connected"), false);
        m.insert(QStringLiteral("hidden"), true);
        out.append(m);
    }
    return out;
}

QVariantList AppController::hiddenProcessesGlobalSnapshot() const
{
    QVariantList out;
    if (!m_audio || !m_config)
        return out;

    const auto known = m_audio->knownProcessesSnapshot();
    QHash<QString, QString> nameByExe;
    nameByExe.reserve(known.size());
    for (const auto &p : known) {
        if (!p.exePath.isEmpty())
            nameByExe.insert(p.exePath, p.displayName);
    }
    for (const auto &exe : m_config->hiddenProcessesGlobalSet()) {
        if (exe.isEmpty())
            continue;
        if (!nameByExe.contains(exe)) {
            const QString base = QFileInfo(exe).fileName();
            nameByExe.insert(exe, base.isEmpty() ? exe : base);
        }
    }

    QVector<QString> exes;
    exes.reserve(nameByExe.size());
    for (auto it = nameByExe.begin(); it != nameByExe.end(); ++it)
        exes.push_back(it.key());
    std::sort(exes.begin(), exes.end(), [&nameByExe](const QString &a, const QString &b) {
        const QString an = nameByExe.value(a).toCaseFolded();
        const QString bn = nameByExe.value(b).toCaseFolded();
        if (an == bn)
            return a.toCaseFolded() < b.toCaseFolded();
        return an < bn;
    });

    for (const auto &exe : exes) {
        const QString label = nameByExe.value(exe).isEmpty() ? exe : nameByExe.value(exe);
        QVariantMap m;
        m.insert(QStringLiteral("exePath"), exe);
        m.insert(QStringLiteral("name"), label);
        m.insert(QStringLiteral("hidden"), m_config->isProcessHiddenGlobal(exe));
        out.append(m);
    }
    return out;
}

QVariantList AppController::hiddenProcessesPerDeviceSnapshot() const
{
    QVariantList out;
    if (!m_audio || !m_config)
        return out;

    const auto devicesAll = m_audio->devicesSnapshotAll();
    QSet<QString> visibleDevIds;
    for (const auto &d : devicesAll)
        visibleDevIds.insert(d.id);

    const auto hiddenMap = m_config->hiddenProcessesPerDeviceMap();
    for (const auto &d : devicesAll) {
        if (m_config->isDeviceHidden(d.id))
            continue;
        QHash<QString, QString> perNameByExe;
        const auto perDevKnown = m_audio->knownProcessesForDeviceSnapshotAll(d.id);
        perNameByExe.reserve(perDevKnown.size());
        for (const auto &p : perDevKnown) {
            if (!p.exePath.isEmpty())
                perNameByExe.insert(p.exePath, p.displayName);
        }
        const auto itHiddenSet = hiddenMap.constFind(d.id);
        if (itHiddenSet != hiddenMap.constEnd()) {
            for (const auto &exe : itHiddenSet.value()) {
                if (exe.isEmpty())
                    continue;
                if (!perNameByExe.contains(exe)) {
                    const QString base = QFileInfo(exe).fileName();
                    perNameByExe.insert(exe, base.isEmpty() ? exe : base);
                }
            }
        }

        QVector<QString> exes;
        exes.reserve(perNameByExe.size());
        for (auto it = perNameByExe.begin(); it != perNameByExe.end(); ++it)
            exes.push_back(it.key());
        std::sort(exes.begin(), exes.end(), [&perNameByExe](const QString &a, const QString &b) {
            const QString an = perNameByExe.value(a).toCaseFolded();
            const QString bn = perNameByExe.value(b).toCaseFolded();
            if (an == bn)
                return a.toCaseFolded() < b.toCaseFolded();
            return an < bn;
        });

        QVariantList procList;
        for (const auto &exe : exes) {
            const QString label = perNameByExe.value(exe).isEmpty() ? exe : perNameByExe.value(exe);
            QVariantMap p;
            p.insert(QStringLiteral("exePath"), exe);
            p.insert(QStringLiteral("name"), label);
            p.insert(QStringLiteral("hidden"), m_config->isProcessHiddenForDevice(d.id, exe));
            procList.append(p);
        }

        QVariantMap dev;
        dev.insert(QStringLiteral("deviceId"), d.id);
        dev.insert(QStringLiteral("name"), d.name.isEmpty() ? d.id : d.name);
        dev.insert(QStringLiteral("connected"), true);
        dev.insert(QStringLiteral("processes"), procList);
        out.append(dev);
    }

    for (auto it = hiddenMap.begin(); it != hiddenMap.end(); ++it) {
        const QString devId = it.key();
        if (devId.isEmpty() || visibleDevIds.contains(devId))
            continue;
        if (m_config->isDeviceHidden(devId))
            continue;
        const auto &exeSet = it.value();
        if (exeSet.isEmpty())
            continue;

        QHash<QString, QString> perNameByExe;
        for (const auto &exe : exeSet) {
            if (exe.isEmpty())
                continue;
            const QString base = QFileInfo(exe).fileName();
            perNameByExe.insert(exe, base.isEmpty() ? exe : base);
        }

        QVector<QString> exes;
        exes.reserve(perNameByExe.size());
        for (auto it2 = perNameByExe.begin(); it2 != perNameByExe.end(); ++it2)
            exes.push_back(it2.key());
        std::sort(exes.begin(), exes.end(), [&perNameByExe](const QString &a, const QString &b) {
            const QString an = perNameByExe.value(a).toCaseFolded();
            const QString bn = perNameByExe.value(b).toCaseFolded();
            if (an == bn)
                return a.toCaseFolded() < b.toCaseFolded();
            return an < bn;
        });

        QVariantList procList;
        for (const auto &exe : exes) {
            const QString label = perNameByExe.value(exe).isEmpty() ? exe : perNameByExe.value(exe);
            QVariantMap p;
            p.insert(QStringLiteral("exePath"), exe);
            p.insert(QStringLiteral("name"), label);
            p.insert(QStringLiteral("hidden"), m_config->isProcessHiddenForDevice(devId, exe));
            procList.append(p);
        }

        QVariantMap dev;
        dev.insert(QStringLiteral("deviceId"), devId);
        dev.insert(QStringLiteral("name"), devId);
        dev.insert(QStringLiteral("connected"), false);
        dev.insert(QStringLiteral("processes"), procList);
        out.append(dev);
    }

    return out;
}

void AppController::popupOpened()
{
    m_popupDepth = qMax(0, m_popupDepth + 1);
}

void AppController::popupClosed()
{
    m_popupDepth = qMax(0, m_popupDepth - 1);
    // If a popup just closed and the flyout is no longer active, close it now.
    if (m_popupDepth == 0 && m_view && m_view->isVisible() && !m_view->isActive()) {
        hideFlyout();
    }
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

void AppController::setShowProcessStatusOnHover(bool v)
{
    if (m_showProcessStatusOnHover == v)
        return;
    m_showProcessStatusOnHover = v;
    if (m_config)
        m_config->setShowProcessStatusOnHover(m_showProcessStatusOnHover);
    emit showProcessStatusOnHoverChanged();
}

void AppController::setScrollWheelVolumeOnHover(bool v)
{
    if (m_scrollWheelVolumeOnHover == v)
        return;
    m_scrollWheelVolumeOnHover = v;
    if (m_config)
        m_config->setScrollWheelVolumeOnHover(m_scrollWheelVolumeOnHover);
    emit scrollWheelVolumeOnHoverChanged();
}

void AppController::setStartWithWindows(bool v)
{
    if (m_startWithWindows == v)
        return;
    m_startWithWindows = v;
    if (m_config)
        m_config->setStartWithWindows(m_startWithWindows);
    applyStartWithWindows(m_startWithWindows);
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

void AppController::setDeviceHidden(const QString &deviceId, bool hidden)
{
    if (!m_config || deviceId.isEmpty())
        return;
    m_config->setDeviceHidden(deviceId, hidden);
    if (m_audio)
        m_audio->refresh();
    rebuildHiddenMenus();
    requestRelayout();
    emit hiddenItemsChanged();
}

void AppController::setProcessHiddenGlobal(const QString &exePath, bool hidden)
{
    if (!m_config || exePath.isEmpty())
        return;
    m_config->setProcessHiddenGlobal(exePath, hidden);
    if (m_audio)
        m_audio->refresh();
    rebuildHiddenMenus();
    requestRelayout();
    emit hiddenItemsChanged();
}

void AppController::setProcessHiddenForDevice(const QString &deviceId, const QString &exePath, bool hidden)
{
    if (!m_config || deviceId.isEmpty() || exePath.isEmpty())
        return;
    m_config->setProcessHiddenForDevice(deviceId, exePath, hidden);
    if (m_audio)
        m_audio->refresh();
    rebuildHiddenMenus();
    requestRelayout();
    emit hiddenItemsChanged();
}

void AppController::requestRelayout()
{
    if (!m_view || !m_view->isVisible())
        return;
    if (!m_relayoutCoalesce.isActive())
        m_relayoutCoalesce.start();
    // A second measure pass after delegates settle (covers animated row removal, etc).
    QTimer::singleShot(80, this, [this]() {
        if (!m_view || !m_view->isVisible())
            return;
        adjustFlyoutHeightToContent();
        positionFlyout();
    });
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

void AppController::showHiddenItemsWindow()
{
    if (!m_hiddenView)
        buildHiddenItemsWindow();
    if (!m_hiddenView)
        return;

    adjustHiddenItemsHeightToContent();
    positionHiddenItemsWindow(true);
    m_hiddenView->show();
    m_hiddenView->requestActivate();

    QTimer::singleShot(0, this, [this]() {
        if (!m_hiddenView || !m_hiddenView->isVisible())
            return;
        adjustHiddenItemsHeightToContent();
        positionHiddenItemsWindow(false);
    });
    QTimer::singleShot(50, this, [this]() {
        if (!m_hiddenView || !m_hiddenView->isVisible())
            return;
        adjustHiddenItemsHeightToContent();
        positionHiddenItemsWindow(false);
    });
}

void AppController::requestHiddenItemsRelayout()
{
    if (!m_hiddenView || !m_hiddenView->isVisible())
        return;
    adjustHiddenItemsHeightToContent();
    positionHiddenItemsWindow(false);
    QTimer::singleShot(80, this, [this]() {
        if (!m_hiddenView || !m_hiddenView->isVisible())
            return;
        adjustHiddenItemsHeightToContent();
        positionHiddenItemsWindow(false);
    });
}

void AppController::hideHiddenItemsWindow()
{
    if (!m_hiddenView)
        return;
    m_hiddenView->hide();
}

void AppController::showAboutDialog()
{
    QDialog dialog;
    dialog.setWindowTitle(tr("About Earie"));
    dialog.setFixedSize(300, 200);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    // Icon in the middle
    QLabel *iconLabel = new QLabel(&dialog);
    QPixmap pixmap(":/assets/earie.ico");
    if (!pixmap.isNull()) {
        iconLabel->setPixmap(pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // App name and version
    QLabel *titleLabel = new QLabel(tr("Earie"), &dialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    QLabel *versionLabel = new QLabel(tr("Version %1").arg(EARIE_VERSION), &dialog);
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    layout->addStretch();

    // Credits
    QLabel *creditsLabel = new QLabel(tr("Made by tfourj"), &dialog);
    creditsLabel->setAlignment(Qt::AlignCenter);
    QFont creditsFont = creditsLabel->font();
    creditsFont.setPointSize(9);
    creditsLabel->setFont(creditsFont);
    layout->addWidget(creditsLabel);

    dialog.exec();
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

    applyWindowEffectsIfPossible(m_view);

    // Auto-resize while visible when switching modes / devices list changes.
    if (m_audio && m_audio->deviceModel()) {
        auto *model = static_cast<QAbstractItemModel *>(m_audio->deviceModel());
        auto relayout = [this]() {
            if (!m_view || !m_view->isVisible())
                return;
            // Layout/contentHeight may settle after a tick; measure twice.
            QTimer::singleShot(0, this, [this]() {
                if (!m_view || !m_view->isVisible())
                    return;
                adjustFlyoutHeightToContent();
                positionFlyout();
            });
            QTimer::singleShot(80, this, [this]() {
                if (!m_view || !m_view->isVisible())
                    return;
                adjustFlyoutHeightToContent();
                positionFlyout();
            });
        };
        connect(model, &QAbstractItemModel::rowsInserted, this, relayout);
        connect(model, &QAbstractItemModel::rowsRemoved, this, relayout);
        connect(model, &QAbstractItemModel::modelReset, this, relayout);
        connect(model, &QAbstractItemModel::layoutChanged, this, relayout);
    }
}

void AppController::buildHiddenItemsWindow()
{
    if (m_hiddenView)
        return;

    m_hiddenView = new QQuickView;
    m_hiddenView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_hiddenView->setColor(Qt::transparent);
    m_hiddenView->setFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    m_hiddenView->installEventFilter(this);

    m_hiddenView->rootContext()->setContextProperty(QStringLiteral("appController"), this);
    m_hiddenView->setSource(QUrl(QStringLiteral("qrc:/qml/HiddenItemsWindow.qml")));
    m_hiddenView->setWidth(420);
    m_hiddenView->setHeight(520);
    m_hiddenView->setMinimumWidth(420);
    m_hiddenView->setMaximumWidth(420);

    applyWindowEffectsIfPossible(m_hiddenView);
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
    const int maxH = qMax(200, work.height() - margin * 2);
    const int minH = 160;

    int desired = hint > 0 ? hint : 520;
    desired = qBound(minH, desired, maxH);

    // Apply. Width is already fixed.
    m_view->setMinimumHeight(desired);
    m_view->setMaximumHeight(desired);
    m_view->resize(m_view->width(), desired);
}

void AppController::adjustHiddenItemsHeightToContent()
{
    if (!m_hiddenView)
        return;

    QObject *root = m_hiddenView->rootObject();
    int hint = 0;
    if (root) {
        const QVariant v = root->property("contentHeightHint");
        if (v.isValid())
            hint = v.toInt();
    }

    QScreen *screen = nullptr;
    if (m_hiddenView && m_hiddenView->isVisible())
        screen = QGuiApplication::screenAt(m_hiddenView->geometry().center());
    if (!screen && m_view && m_view->isVisible())
        screen = QGuiApplication::screenAt(m_view->geometry().center());
    if (!screen)
        screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    QRect work = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    const int margin = 12;
    const int maxH = qMax(220, work.height() - margin * 2);
    const int minH = 200;

    int desired = hint > 0 ? hint : 520;
    desired = qBound(minH, desired, maxH);

    m_hiddenView->setMinimumHeight(desired);
    m_hiddenView->setMaximumHeight(desired);
    m_hiddenView->resize(m_hiddenView->width(), desired);
}

void AppController::applyStartWithWindows(bool v)
{
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    const QString appName = QStringLiteral("Earie");
    if (v) {
        const QString exePath = QDir::toNativeSeparators(QGuiApplication::applicationFilePath());
        runKey.setValue(appName, QStringLiteral("\"%1\"").arg(exePath));
    } else {
        runKey.remove(appName);
    }
}

void AppController::applyWindowEffectsIfPossible(QQuickView *view)
{
    if (!view)
        return;
    HWND hwnd = reinterpret_cast<HWND>(view->winId());
    WinAcrylic::enableAcrylic(hwnd);
}

void AppController::buildTray()
{
    m_menu = new QMenu;
    // Best-effort: style the tray QMenu to resemble the in-app QML StyledMenu.
    // Note: On some Windows setups the tray context menu may still be partially native-looking.
    m_menu->setStyleSheet(trayMenuStyleSheet());

    connect(m_menu, &QMenu::aboutToHide, this, [this]() {
        if (!m_deferHiddenMenuRebuild)
            return;
        QTimer::singleShot(0, this, [this]() {
            if (!m_deferHiddenMenuRebuild)
                return;
            m_deferHiddenMenuRebuild = false;
            rebuildHiddenMenus();
        });
    });

    m_actionOpen = m_menu->addAction(tr("Open mixer"));
    connect(m_actionOpen, &QAction::triggered, this, &AppController::showFlyout);

    m_menu->addSeparator();

    QAction *aWinMixer = m_menu->addAction(tr("Windows Volume Mixer"));
    connect(aWinMixer, &QAction::triggered, this, []() { openWindowsVolumeMixer(); });

    QAction *aPlayback = m_menu->addAction(tr("Playback devices"));
    connect(aPlayback, &QAction::triggered, this, []() { openWindowsPlaybackDevices(); });

    QAction *aSoundSettings = m_menu->addAction(tr("Sound settings"));
    connect(aSoundSettings, &QAction::triggered, this, []() { openWindowsSoundSettings(); });

    m_menu->addSeparator();

    m_actionDefaultOnly = m_menu->addAction(tr("Show default device"));
    m_actionDefaultOnly->setCheckable(true);
    m_actionAllDevices = m_menu->addAction(tr("Show all devices"));
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

    m_actionStartWithWindows = m_menu->addAction(tr("Start with Windows"));
    m_actionStartWithWindows->setCheckable(true);
    m_actionStartWithWindows->setChecked(m_startWithWindows);
    connect(m_actionStartWithWindows, &QAction::triggered, this, [this](bool checked) {
        setStartWithWindows(checked);
    });

    m_menu->addSeparator();

    QAction *aHiddenItems = m_menu->addAction(tr("Manage hidden items…"));
    connect(aHiddenItems, &QAction::triggered, this, &AppController::showHiddenItemsWindow);

    m_menu->addSeparator();

    QAction *aAbout = m_menu->addAction(tr("About"));
    connect(aAbout, &QAction::triggered, this, &AppController::showAboutDialog);

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
            // If the flyout just closed due to WindowDeactivate (often triggered by this same click),
            // don't immediately re-open it.
            if (m_suppressNextTrayToggle) {
                m_suppressNextTrayToggle = false;
                return;
            }
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

static QString trayMenuStyleSheet()
{
    // Dark, rounded menu matching the app theme as closely as native tray menus allow.
    return QStringLiteral(
        "QMenu {"
        "  background-color: #1D1F22;"
        "  color: #E7EAF0;"
        "  border: 1px solid rgba(255,255,255,0.08);"
        "  padding: 6px;"
        "  border-radius: 10px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 12px;"
        "  border-radius: 8px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #2E3136;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  margin: 6px 10px;"
        "  background: rgba(255,255,255,0.08);"
        "}"
        "QCheckBox {"
        "  color: #E7EAF0;"
        "}"
        "QCheckBox::indicator {"
        "  width: 14px;"
        "  height: 14px;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "  border: 1px solid rgba(255,255,255,0.22);"
        "  background: transparent;"
        "  border-radius: 3px;"
        "}"
        "QCheckBox::indicator:checked {"
        "  border: 1px solid rgba(35,150,255,0.95);"
        "  background: rgba(35,150,255,0.75);"
        "  border-radius: 3px;"
        "}"
    );
}

static void openWindowsVolumeMixer()
{
#if defined(Q_OS_WIN)
    // Classic Windows volume mixer.
    // (On Win11 this still works, though Microsoft is moving some UX into Settings.)
    QProcess::startDetached(QStringLiteral("sndvol.exe"), {});
#endif
}

static void openWindowsPlaybackDevices()
{
#if defined(Q_OS_WIN)
    // Opens the classic Sound control panel (Playback tab available).
    // Using control.exe tends to be more reliable than directly executing the .cpl.
    QProcess::startDetached(QStringLiteral("control.exe"), { QStringLiteral("mmsys.cpl") });
#endif
}

static void openWindowsSoundSettings()
{
#if defined(Q_OS_WIN)
    // Modern Windows Settings page.
    // Win11/10: ms-settings:sound
    if (!QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:sound")))) {
        // Fallback to classic control panel.
        openWindowsPlaybackDevices();
    }
#endif
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

void AppController::positionHiddenItemsWindow(bool recomputeAnchor)
{
    if (!m_hiddenView)
        return;

    const int w = m_hiddenView->width();
    const int h = m_hiddenView->height();
    const int margin = 12;

    if (recomputeAnchor || !m_hiddenAnchorValid) {
        QScreen *screen = nullptr;
        if (m_view && m_view->isVisible())
            screen = QGuiApplication::screenAt(m_view->geometry().center());
        if (!screen)
            screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();

        QRect work = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
        int x = work.right() - w - margin;
        int y = work.bottom() - margin - h;
        x = qMax(work.left() + margin, qMin(x, work.right() - w - margin));
        y = qMax(work.top() + margin, qMin(y, work.bottom() - h - margin));

        m_hiddenAnchorPos = QPoint(x, y + h);
        m_hiddenAnchorWork = work;
        m_hiddenAnchorValid = true;
    }

    QRect work = m_hiddenAnchorWork;
    if (!work.isValid()) {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        work = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    }

    int x = m_hiddenAnchorPos.x();
    int bottomY = m_hiddenAnchorPos.y();
    x = qMax(work.left() + margin, qMin(x, work.right() - w - margin));
    bottomY = qMax(work.top() + margin + h, qMin(bottomY, work.bottom() - margin));
    int y = bottomY - h;

    m_hiddenView->setPosition(QPoint(x, y));
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view) {
        if (event->type() == QEvent::WindowDeactivate) {
            // Close on focus loss (EarTrumpet-like), but NOT while a QML popup/menu is open:
            // those can be separate native windows and would otherwise instantly close the flyout.
            if (m_popupDepth > 0)
                return QObject::eventFilter(watched, event);

            if (m_view && m_view->isVisible()) {
                m_suppressNextTrayToggle = true;
                if (!m_trayToggleSuppressTimer.isActive())
                    m_trayToggleSuppressTimer.start();
            }
            hideFlyout();
        }
    } else if (watched == m_hiddenView) {
        if (event->type() == QEvent::WindowDeactivate) {
            if (m_popupDepth > 0)
                return QObject::eventFilter(watched, event);
            hideHiddenItemsWindow();
        }
    }
    return QObject::eventFilter(watched, event);
}

void AppController::rebuildHiddenMenus()
{
    if (!m_hiddenDevicesMenu || !m_hiddenProcessesMenu || !m_audio || !m_config)
        return;

    // Never rebuild while the tray menu is open; it will close/flicker and can crash due to
    // QWidgetAction widgets getting destroyed during signal delivery.
    if (m_menu && m_menu->isVisible()) {
        m_deferHiddenMenuRebuild = true;
        emit hiddenItemsChanged();
        return;
    }

    m_hiddenDevicesMenu->clear();
    m_hiddenProcessesMenu->clear();

    auto addCheckItem = [this](QMenu *menu, const QString &label, bool checked, std::function<void(bool)> onToggled) {
        if (!menu)
            return;
        auto *wa = new QWidgetAction(menu);
        auto *cb = new QCheckBox(label);
        cb->setChecked(checked);
        cb->setFocusPolicy(Qt::NoFocus);
        // Keep it visually aligned with standard menu text.
        cb->setStyleSheet(QStringLiteral("QCheckBox { padding: 6px 12px; }"));
        connect(cb, &QCheckBox::toggled, this, [onToggled](bool v) {
            if (onToggled)
                onToggled(v);
        });
        wa->setDefaultWidget(cb);
        menu->addAction(wa);
    };

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
            addCheckItem(m_hiddenDevicesMenu, d.name, m_config->isDeviceHidden(d.id), [this, id = d.id](bool checked) {
                m_config->setDeviceHidden(id, checked);
                m_audio->refresh(); // re-filter
                emit hiddenItemsChanged();
            });
        }

        // Also show hidden device IDs that are currently disconnected (so user can unhide).
        for (const auto &hiddenId : m_config->hiddenDevices()) {
            if (hiddenId.isEmpty() || seen.contains(hiddenId))
                continue;
            addCheckItem(m_hiddenDevicesMenu, tr("[disconnected] %1").arg(hiddenId), true, [this, id = hiddenId](bool checked) {
                m_config->setDeviceHidden(id, checked);
                m_audio->refresh();
                emit hiddenItemsChanged();
            });
        }
    }

    // Hidden processes: Global and Per-device.
    QMenu *globalMenu = m_hiddenProcessesMenu->addMenu(tr("Global"));
    QMenu *perDeviceMenu = m_hiddenProcessesMenu->addMenu(tr("Per device"));

    const auto known = m_audio->knownProcessesSnapshot();
    QHash<QString, QString> nameByExe;
    nameByExe.reserve(known.size());
    for (const auto &p : known) {
        if (!p.exePath.isEmpty())
            nameByExe.insert(p.exePath, p.displayName);
    }
    // Ensure config-stored hidden processes are always present, even if not currently "known".
    for (const auto &exe : m_config->hiddenProcessesGlobalSet()) {
        if (exe.isEmpty())
            continue;
        if (!nameByExe.contains(exe)) {
            const QString base = QFileInfo(exe).fileName();
            nameByExe.insert(exe, base.isEmpty() ? exe : base);
        }
    }

    if (nameByExe.isEmpty()) {
        QAction *a = globalMenu->addAction(tr("(No processes)"));
        a->setEnabled(false);
    } else {
        QVector<QString> exes;
        exes.reserve(nameByExe.size());
        for (auto it = nameByExe.begin(); it != nameByExe.end(); ++it)
            exes.push_back(it.key());
        std::sort(exes.begin(), exes.end(), [&nameByExe](const QString &a, const QString &b) {
            const QString an = nameByExe.value(a).toCaseFolded();
            const QString bn = nameByExe.value(b).toCaseFolded();
            if (an == bn)
                return a.toCaseFolded() < b.toCaseFolded();
            return an < bn;
        });

        for (const auto &exe : exes) {
            const QString label = nameByExe.value(exe).isEmpty() ? exe : nameByExe.value(exe);
            addCheckItem(globalMenu, label, m_config->isProcessHiddenGlobal(exe), [this, exe](bool checked) {
                m_config->setProcessHiddenGlobal(exe, checked);
                m_audio->refresh();
                rebuildHiddenMenus();
                requestRelayout();
            });
        }
    }

    for (const auto &d : devicesVisible) {
        QMenu *devMenu = perDeviceMenu->addMenu(d.name);
        const auto perDevKnown = m_audio->knownProcessesForDeviceSnapshot(d.id);
        QHash<QString, QString> perNameByExe;
        perNameByExe.reserve(perDevKnown.size());
        for (const auto &p : perDevKnown) {
            if (!p.exePath.isEmpty())
                perNameByExe.insert(p.exePath, p.displayName);
        }
        // Include config-stored hidden per-device processes even if not currently known.
        const auto hiddenMap = m_config->hiddenProcessesPerDeviceMap();
        const auto itHiddenSet = hiddenMap.constFind(d.id);
        if (itHiddenSet != hiddenMap.constEnd()) {
            for (const auto &exe : itHiddenSet.value()) {
                if (exe.isEmpty())
                    continue;
                if (!perNameByExe.contains(exe)) {
                    const QString base = QFileInfo(exe).fileName();
                    perNameByExe.insert(exe, base.isEmpty() ? exe : base);
                }
            }
        }

        if (perNameByExe.isEmpty()) {
            QAction *a = devMenu->addAction(tr("(No processes)"));
            a->setEnabled(false);
            continue;
        }

        QVector<QString> exes;
        exes.reserve(perNameByExe.size());
        for (auto it = perNameByExe.begin(); it != perNameByExe.end(); ++it)
            exes.push_back(it.key());
        std::sort(exes.begin(), exes.end(), [&perNameByExe](const QString &a, const QString &b) {
            const QString an = perNameByExe.value(a).toCaseFolded();
            const QString bn = perNameByExe.value(b).toCaseFolded();
            if (an == bn)
                return a.toCaseFolded() < b.toCaseFolded();
            return an < bn;
        });

        for (const auto &exe : exes) {
            const QString label = perNameByExe.value(exe).isEmpty() ? exe : perNameByExe.value(exe);
            addCheckItem(devMenu, label, m_config->isProcessHiddenForDevice(d.id, exe), [this, devId = d.id, exe](bool checked) {
                m_config->setProcessHiddenForDevice(devId, exe, checked);
                m_audio->refresh();
                rebuildHiddenMenus();
                requestRelayout();
            });
        }
    }

    // Also show per-device hidden rules for devices that are currently disconnected/not visible,
    // so user can still unhide them.
    const auto hiddenMapAll = m_config->hiddenProcessesPerDeviceMap();
    if (!hiddenMapAll.isEmpty()) {
        QSet<QString> visibleDevIds;
        for (const auto &d : devicesVisible)
            visibleDevIds.insert(d.id);

        for (auto it = hiddenMapAll.begin(); it != hiddenMapAll.end(); ++it) {
            const QString devId = it.key();
            if (devId.isEmpty() || visibleDevIds.contains(devId))
                continue;
            const auto &exeSet = it.value();
            if (exeSet.isEmpty())
                continue;

            QMenu *devMenu = perDeviceMenu->addMenu(tr("[disconnected] %1").arg(devId));
            QHash<QString, QString> perNameByExe;
            for (const auto &exe : exeSet) {
                if (exe.isEmpty())
                    continue;
                const QString base = QFileInfo(exe).fileName();
                perNameByExe.insert(exe, base.isEmpty() ? exe : base);
            }

            QVector<QString> exes;
            exes.reserve(perNameByExe.size());
            for (auto it2 = perNameByExe.begin(); it2 != perNameByExe.end(); ++it2)
                exes.push_back(it2.key());
            std::sort(exes.begin(), exes.end(), [&perNameByExe](const QString &a, const QString &b) {
                const QString an = perNameByExe.value(a).toCaseFolded();
                const QString bn = perNameByExe.value(b).toCaseFolded();
                if (an == bn)
                    return a.toCaseFolded() < b.toCaseFolded();
                return an < bn;
            });

            for (const auto &exe : exes) {
                const QString label = perNameByExe.value(exe).isEmpty() ? exe : perNameByExe.value(exe);
                addCheckItem(devMenu, label, m_config->isProcessHiddenForDevice(devId, exe), [this, devId, exe](bool checked) {
                    m_config->setProcessHiddenForDevice(devId, exe, checked);
                    m_audio->refresh();
                    rebuildHiddenMenus();
                    requestRelayout();
                });
            }
        }
    }

    emit hiddenItemsChanged();
}


