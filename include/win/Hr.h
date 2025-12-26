#pragma once

#include <windows.h>

#include <QString>

inline QString hrToString(HRESULT hr)
{
    wchar_t *buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    const DWORD len = FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), lang, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString out;
    if (len && buffer) {
        out = QString::fromWCharArray(buffer).trimmed();
        LocalFree(buffer);
    } else {
        out = QStringLiteral("HRESULT 0x%1").arg(static_cast<qulonglong>(static_cast<unsigned long>(hr)), 8, 16, QLatin1Char('0'));
    }
    return out;
}

#define HR_RET(hrcall) \
    do { \
        const HRESULT _hr = (hrcall); \
        if (FAILED(_hr)) { \
            return _hr; \
        } \
    } while (0)


