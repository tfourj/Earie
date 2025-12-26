#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

class IconCache final : public QQuickImageProvider
{
public:
    IconCache();

    // Key is stable (exePath). QML uses: image://appicon/<url-escaped-key>
    QString ensureIconForExePath(const QString &exePath);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QImage loadSmallIconForExePath(const QString &exePath) const;

    mutable QMutex m_mutex;
    QHash<QString, QImage> m_cache; // exePath -> image
};


