#include "WinTrayPositioner.h"

#include <QGuiApplication>
#include <QScreen>

#include <windows.h>
#include <shellapi.h>

// Shell_NotifyIconGetRect is available on Win7+ but can fail in some contexts.
typedef HRESULT(WINAPI *pShell_NotifyIconGetRect)(const NOTIFYICONIDENTIFIER *identifier, RECT *iconLocation);

QRect WinTrayPositioner::trayRect()
{
    QRect out;
    HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
    if (!shell32)
        return out;

    auto fn = reinterpret_cast<pShell_NotifyIconGetRect>(GetProcAddress(shell32, "Shell_NotifyIconGetRect"));
    if (!fn)
        return out;

    NOTIFYICONIDENTIFIER id{};
    id.cbSize = sizeof(id);
    id.hWnd = nullptr; // Without a specific tray icon hwnd+id we can't resolve precisely.
    id.uID = 0;

    RECT r{};
    const HRESULT hr = fn(&id, &r);
    if (SUCCEEDED(hr)) {
        out = QRect(QPoint(r.left, r.top), QPoint(r.right, r.bottom));
    }
    return out;
}

QPoint WinTrayPositioner::suggestFlyoutTopLeft(int w, int h)
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect work = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    // Default: bottom-right with margin.
    const int margin = 12;
    int x = work.right() - w - margin;
    int y = work.bottom() - h - margin;

    // Clamp.
    x = qMax(work.left() + margin, qMin(x, work.right() - w - margin));
    y = qMax(work.top() + margin, qMin(y, work.bottom() - h - margin));
    return QPoint(x, y);
}


