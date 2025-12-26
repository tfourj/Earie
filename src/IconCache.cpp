#include "IconCache.h"

#include <QFileInfo>
#include <QUrl>

#include <windows.h>
#include <shellapi.h>

static QImage qimageFromHICON(HICON hIcon, int desiredPx)
{
    if (!hIcon)
        return QImage();

    if (desiredPx <= 0)
        desiredPx = 32;

    // Render icon into a 32-bit ARGB DIB using DrawIconEx to preserve alpha.
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = desiredPx;
    bi.bmiHeader.biHeight = -desiredPx; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP dib = CreateDIBSection(screenDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits || !memDc) {
        if (dib) DeleteObject(dib);
        if (memDc) DeleteDC(memDc);
        if (screenDc) ReleaseDC(nullptr, screenDc);
        return {};
    }

    HGDIOBJ oldBmp = SelectObject(memDc, dib);
    // Clear to transparent.
    memset(bits, 0, static_cast<size_t>(desiredPx) * static_cast<size_t>(desiredPx) * 4);

    DrawIconEx(memDc, 0, 0, hIcon, desiredPx, desiredPx, 0, nullptr, DI_NORMAL);

    QImage img(reinterpret_cast<uchar *>(bits), desiredPx, desiredPx, QImage::Format_ARGB32_Premultiplied);
    QImage out = img.copy(); // detach from DIB memory

    SelectObject(memDc, oldBmp);
    DeleteObject(dib);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    return out;
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
    // Prefer larger icon for better downscaling quality.
    if (!SHGetFileInfoW(wpath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON))
        return {};

    QImage img = qimageFromHICON(sfi.hIcon, 64);
    DestroyIcon(sfi.hIcon);
    return img;
}


