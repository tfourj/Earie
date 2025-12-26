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

    m_hiddenDevices.clear();
    for (const auto &v : o.value(QStringLiteral("hiddenDevices")).toArray()) {
        const QString id = v.toString();
        if (!id.isEmpty())
            m_hiddenDevices.insert(id);
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
}

void ConfigStore::save() const
{
    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject o;
    o.insert(QStringLiteral("schemaVersion"), 1);
    o.insert(QStringLiteral("mode"), m_mode == Mode::AllDevices ? QStringLiteral("all") : QStringLiteral("default"));
    o.insert(QStringLiteral("showSystemSessions"), m_showSystemSessions);

    {
        QJsonArray arr;
        for (const auto &id : m_hiddenDevices)
            arr.append(id);
        o.insert(QStringLiteral("hiddenDevices"), arr);
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

bool ConfigStore::isDeviceHidden(const QString &deviceId) const
{
    return m_hiddenDevices.contains(deviceId);
}

QStringList ConfigStore::hiddenDevices() const
{
    return QStringList(m_hiddenDevices.begin(), m_hiddenDevices.end());
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
    else
        m_hiddenDevices.remove(deviceId);
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


