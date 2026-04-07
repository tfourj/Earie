#include "ConfigStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

ConfigStore::ConfigStore(QObject *parent)
    : QObject(parent)
{
    connect(this, &ConfigStore::changed, this, [this]() { save(); });
}

QString ConfigStore::configPath() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("config.json"));
}

void ConfigStore::load()
{
    const QString path = configPath();
    QFile f(path);
    if (!f.exists())
        return;
    if (!f.open(QIODevice::ReadOnly))
        return;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;

    const QJsonObject o = doc.object();

    const QString modeStr = o.value(QStringLiteral("mode")).toString(QStringLiteral("default"));
    m_mode = (modeStr == QLatin1String("all")) ? Mode::AllDevices : Mode::DefaultDeviceOnly;

    m_showSystemSessions = o.value(QStringLiteral("showSystemSessions")).toBool(false);
    m_showInputDevices = o.value(QStringLiteral("showInputDevices")).toBool(false);
    m_showProcessStatusOnHover = o.value(QStringLiteral("showProcessStatusOnHover")).toBool(false);
    m_scrollWheelVolumeOnHover = o.value(QStringLiteral("scrollWheelVolumeOnHover")).toBool(false);
    m_debugMode = o.value(QStringLiteral("debugMode")).toBool(false);
    m_startWithWindows = o.value(QStringLiteral("startWithWindows")).toBool(false);
    m_useNativeTrayIcon = o.value(QStringLiteral("useNativeTrayIcon")).toBool(false);
    const QString trayMode = o.value(QStringLiteral("trayIconMode")).toString();
    if (trayMode == QLatin1String("black"))
        m_trayIconMode = TrayIconMode::Black;
    else
        m_trayIconMode = TrayIconMode::White;

    m_hiddenDevices.clear();
    for (const auto &v : o.value(QStringLiteral("hiddenDevices")).toArray()) {
        const QString id = v.toString();
        if (!id.isEmpty())
            m_hiddenDevices.insert(id);
    }
    m_hiddenDeviceNames.clear();
    const QJsonObject hiddenNames = o.value(QStringLiteral("hiddenDeviceNames")).toObject();
    for (auto it = hiddenNames.begin(); it != hiddenNames.end(); ++it) {
        const QString id = it.key();
        const QString name = it.value().toString().trimmed();
        if (!id.isEmpty() && !name.isEmpty())
            m_hiddenDeviceNames.insert(id, name);
    }
    for (auto it = m_hiddenDeviceNames.begin(); it != m_hiddenDeviceNames.end();) {
        if (!m_hiddenDevices.contains(it.key()))
            it = m_hiddenDeviceNames.erase(it);
        else
            ++it;
    }

    m_hiddenProcessesGlobal.clear();
    for (const auto &v : o.value(QStringLiteral("hiddenProcessesGlobal")).toArray()) {
        const QString exe = v.toString();
        if (!exe.isEmpty())
            m_hiddenProcessesGlobal.insert(exe);
    }

    m_hiddenProcessesPerDevice.clear();
    const QJsonObject perDev = o.value(QStringLiteral("hiddenProcessesPerDevice")).toObject();
    for (auto it = perDev.begin(); it != perDev.end(); ++it) {
        const QString devId = it.key();
        QSet<QString> set;
        for (const auto &v : it.value().toArray()) {
            const QString exe = v.toString();
            if (!exe.isEmpty())
                set.insert(exe);
        }
        if (!devId.isEmpty() && !set.isEmpty())
            m_hiddenProcessesPerDevice.insert(devId, set);
    }

    m_deviceOrder.clear();
    for (const auto &v : o.value(QStringLiteral("deviceOrder")).toArray()) {
        const QString id = v.toString();
        if (!id.isEmpty())
            m_deviceOrder.append(id);
    }
}

void ConfigStore::save() const
{
    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject o;
    o.insert(QStringLiteral("schemaVersion"), 1);
    o.insert(QStringLiteral("mode"), m_mode == Mode::AllDevices ? QStringLiteral("all") : QStringLiteral("default"));
    o.insert(QStringLiteral("showSystemSessions"), m_showSystemSessions);
    o.insert(QStringLiteral("showInputDevices"), m_showInputDevices);
    o.insert(QStringLiteral("showProcessStatusOnHover"), m_showProcessStatusOnHover);
    o.insert(QStringLiteral("scrollWheelVolumeOnHover"), m_scrollWheelVolumeOnHover);
    o.insert(QStringLiteral("debugMode"), m_debugMode);
    o.insert(QStringLiteral("startWithWindows"), m_startWithWindows);
    o.insert(QStringLiteral("useNativeTrayIcon"), m_useNativeTrayIcon);
    o.insert(QStringLiteral("trayIconMode"),
        m_trayIconMode == TrayIconMode::Black ? QStringLiteral("black") : QStringLiteral("white"));

    {
        QJsonArray arr;
        for (const auto &id : m_hiddenDevices)
            arr.append(id);
        o.insert(QStringLiteral("hiddenDevices"), arr);
    }
    {
        QJsonObject names;
        for (const auto &id : m_hiddenDevices) {
            const QString name = m_hiddenDeviceNames.value(id).trimmed();
            if (!name.isEmpty())
                names.insert(id, name);
        }
        o.insert(QStringLiteral("hiddenDeviceNames"), names);
    }
    {
        QJsonArray arr;
        for (const auto &exe : m_hiddenProcessesGlobal)
            arr.append(exe);
        o.insert(QStringLiteral("hiddenProcessesGlobal"), arr);
    }
    {
        QJsonObject perDev;
        for (auto it = m_hiddenProcessesPerDevice.begin(); it != m_hiddenProcessesPerDevice.end(); ++it) {
            QJsonArray arr;
            for (const auto &exe : it.value())
                arr.append(exe);
            perDev.insert(it.key(), arr);
        }
        o.insert(QStringLiteral("hiddenProcessesPerDevice"), perDev);
    }
    {
        QJsonArray arr;
        for (const auto &id : m_deviceOrder)
            arr.append(id);
        o.insert(QStringLiteral("deviceOrder"), arr);
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

void ConfigStore::setMode(Mode m)
{
    if (m_mode == m)
        return;
    m_mode = m;
    emit changed();
}

void ConfigStore::setShowSystemSessions(bool v)
{
    if (m_showSystemSessions == v)
        return;
    m_showSystemSessions = v;
    emit changed();
}

void ConfigStore::setShowInputDevices(bool v)
{
    if (m_showInputDevices == v)
        return;
    m_showInputDevices = v;
    emit changed();
}

void ConfigStore::setShowProcessStatusOnHover(bool v)
{
    if (m_showProcessStatusOnHover == v)
        return;
    m_showProcessStatusOnHover = v;
    emit changed();
}

void ConfigStore::setScrollWheelVolumeOnHover(bool v)
{
    if (m_scrollWheelVolumeOnHover == v)
        return;
    m_scrollWheelVolumeOnHover = v;
    emit changed();
}

void ConfigStore::setDebugMode(bool v)
{
    if (m_debugMode == v)
        return;
    m_debugMode = v;
    emit changed();
}

void ConfigStore::setStartWithWindows(bool v)
{
    if (m_startWithWindows == v)
        return;
    m_startWithWindows = v;
    emit changed();
}

void ConfigStore::setUseNativeTrayIcon(bool v)
{
    if (m_useNativeTrayIcon == v)
        return;
    m_useNativeTrayIcon = v;
    emit changed();
}

void ConfigStore::setTrayIconMode(TrayIconMode v)
{
    if (m_trayIconMode == v)
        return;
    m_trayIconMode = v;
    emit changed();
}

bool ConfigStore::isDeviceHidden(const QString &deviceId) const
{
    return m_hiddenDevices.contains(deviceId);
}

QStringList ConfigStore::hiddenDevices() const
{
    return QStringList(m_hiddenDevices.begin(), m_hiddenDevices.end());
}

QString ConfigStore::hiddenDeviceName(const QString &deviceId) const
{
    return m_hiddenDeviceNames.value(deviceId);
}

void ConfigStore::rememberDeviceName(const QString &deviceId, const QString &deviceName)
{
    if (deviceId.isEmpty() || !m_hiddenDevices.contains(deviceId))
        return;
    const QString trimmed = deviceName.trimmed();
    if (trimmed.isEmpty())
        return;
    if (m_hiddenDeviceNames.value(deviceId) == trimmed)
        return;
    m_hiddenDeviceNames.insert(deviceId, trimmed);
    emit changed();
}

void ConfigStore::setDeviceHidden(const QString &deviceId, bool hidden)
{
    if (deviceId.isEmpty())
        return;
    const bool had = m_hiddenDevices.contains(deviceId);
    if (hidden == had)
        return;
    if (hidden)
        m_hiddenDevices.insert(deviceId);
    else {
        m_hiddenDevices.remove(deviceId);
        m_hiddenDeviceNames.remove(deviceId);
    }
    emit changed();
}

bool ConfigStore::remapDeviceId(const QString &oldDeviceId, const QString &newDeviceId, const QString &newDeviceName)
{
    if (oldDeviceId.isEmpty() || newDeviceId.isEmpty() || oldDeviceId == newDeviceId) {
        if (!newDeviceId.isEmpty()) {
            const QString trimmed = newDeviceName.trimmed();
            if (!trimmed.isEmpty() && m_hiddenDevices.contains(newDeviceId) && m_hiddenDeviceNames.value(newDeviceId) != trimmed) {
                m_hiddenDeviceNames.insert(newDeviceId, trimmed);
                emit changed();
                return true;
            }
        }
        return false;
    }

    bool didChange = false;
    const QString oldName = m_hiddenDeviceNames.value(oldDeviceId).trimmed();
    const QString existingNewName = m_hiddenDeviceNames.value(newDeviceId).trimmed();
    const QString preferredName = !newDeviceName.trimmed().isEmpty() ? newDeviceName.trimmed()
                                : (!existingNewName.isEmpty() ? existingNewName : oldName);

    if (m_hiddenDevices.remove(oldDeviceId) > 0) {
        didChange = true;
        if (!m_hiddenDevices.contains(newDeviceId)) {
            m_hiddenDevices.insert(newDeviceId);
        }
    }
    if (m_hiddenDeviceNames.remove(oldDeviceId) > 0)
        didChange = true;
    if (m_hiddenDevices.contains(newDeviceId) && !preferredName.isEmpty() && m_hiddenDeviceNames.value(newDeviceId) != preferredName) {
        m_hiddenDeviceNames.insert(newDeviceId, preferredName);
        didChange = true;
    }

    bool orderChanged = false;
    for (auto &id : m_deviceOrder) {
        if (id == oldDeviceId) {
            id = newDeviceId;
            orderChanged = true;
        }
    }
    if (orderChanged) {
        QStringList deduped;
        deduped.reserve(m_deviceOrder.size());
        QSet<QString> seen;
        for (const auto &id : m_deviceOrder) {
            if (id.isEmpty() || seen.contains(id))
                continue;
            seen.insert(id);
            deduped.append(id);
        }
        if (deduped != m_deviceOrder)
            m_deviceOrder = deduped;
        didChange = true;
    }

    const auto itOld = m_hiddenProcessesPerDevice.constFind(oldDeviceId);
    if (itOld != m_hiddenProcessesPerDevice.constEnd()) {
        QSet<QString> merged = m_hiddenProcessesPerDevice.value(newDeviceId);
        merged.unite(itOld.value());
        m_hiddenProcessesPerDevice.remove(oldDeviceId);
        if (merged.isEmpty())
            m_hiddenProcessesPerDevice.remove(newDeviceId);
        else
            m_hiddenProcessesPerDevice.insert(newDeviceId, merged);
        didChange = true;
    }

    if (didChange)
        emit changed();
    return didChange;
}

void ConfigStore::setDeviceOrder(const QStringList &order)
{
    if (m_deviceOrder == order)
        return;
    m_deviceOrder = order;
    emit changed();
}

bool ConfigStore::isProcessHiddenGlobal(const QString &exePath) const
{
    return m_hiddenProcessesGlobal.contains(exePath);
}

void ConfigStore::setProcessHiddenGlobal(const QString &exePath, bool hidden)
{
    if (exePath.isEmpty())
        return;
    const bool had = m_hiddenProcessesGlobal.contains(exePath);
    if (hidden == had)
        return;
    if (hidden)
        m_hiddenProcessesGlobal.insert(exePath);
    else
        m_hiddenProcessesGlobal.remove(exePath);
    emit changed();
}

bool ConfigStore::isProcessHiddenForDevice(const QString &deviceId, const QString &exePath) const
{
    auto it = m_hiddenProcessesPerDevice.constFind(deviceId);
    if (it == m_hiddenProcessesPerDevice.constEnd())
        return false;
    return it.value().contains(exePath);
}

void ConfigStore::setProcessHiddenForDevice(const QString &deviceId, const QString &exePath, bool hidden)
{
    if (deviceId.isEmpty() || exePath.isEmpty())
        return;

    auto set = m_hiddenProcessesPerDevice.value(deviceId);
    const bool had = set.contains(exePath);
    if (hidden == had)
        return;
    if (hidden)
        set.insert(exePath);
    else
        set.remove(exePath);

    if (set.isEmpty())
        m_hiddenProcessesPerDevice.remove(deviceId);
    else
        m_hiddenProcessesPerDevice.insert(deviceId, set);

    emit changed();
}


