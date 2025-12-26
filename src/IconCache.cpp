#include "IconCache.h"

#include <QFileInfo>
#include <QUrl>

#include <windows.h>
#include <shellapi.h>

static QImage qimageFromHICON(HICON hIcon)
{
    if (!hIcon)
        return QImage();

    ICONINFO info{};
    if (!GetIconInfo(hIcon, &info))
        return QImage();

    BITMAP bmp{};
    GetObject(info.hbmColor ? info.hbmColor : info.hbmMask, sizeof(bmp), &bmp);

    const int w = bmp.bmWidth;
    const int h = bmp.bmHeight;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    HDC hdc = GetDC(nullptr);
    GetDIBits(hdc, info.hbmColor, 0, h, img.bits(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    if (info.hbmColor)
        DeleteObject(info.hbmColor);
    if (info.hbmMask)
        DeleteObject(info.hbmMask);

    return img;
}

IconCache::IconCache()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QString IconCache::ensureIconForExePath(const QString &exePath)
{
    if (exePath.isEmpty())
        return QString();

    QMutexLocker lock(&m_mutex);
    if (m_cache.contains(exePath))
        return exePath;

    lock.unlock();
    QImage img = loadSmallIconForExePath(exePath);
    if (img.isNull()) {
        img = QImage(24, 24, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(80, 80, 80, 255));
    }

    lock.relock();
    m_cache.insert(exePath, img);
    return exePath;
}

QImage IconCache::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // QML uses encodeURIComponent(exePath) in the image:// URL.
    // Decode it back to a real filesystem path.
    const QString key = QUrl::fromPercentEncoding(id.toUtf8());
    QImage img;
    {
        QMutexLocker lock(&m_mutex);
        img = m_cache.value(key);
    }

    if (img.isNull() && !key.isEmpty()) {
        img = loadSmallIconForExePath(key);
        if (!img.isNull()) {
            QMutexLocker lock(&m_mutex);
            m_cache.insert(key, img);
        }
    }

    if (requestedSize.isValid() && !img.isNull())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    if (size)
        *size = img.size();
    return img;
}

QImage IconCache::loadSmallIconForExePath(const QString &exePath) const
{
    const QString path = QFileInfo(exePath).exists() ? exePath : QString();
    if (path.isEmpty())
        return {};

    SHFILEINFOW sfi{};
    const std::wstring wpath = path.toStdWString();
    if (!SHGetFileInfoW(wpath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON))
        return {};

    QImage img = qimageFromHICON(sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    return img;
}


