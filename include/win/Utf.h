#pragma once

#include <QString>

#include <windows.h>

inline QString fromWide(const wchar_t *s)
{
    return s ? QString::fromWCharArray(s) : QString();
}

inline std::wstring toWide(const QString &s)
{
    return s.toStdWString();
}


