#pragma once

#include <QPoint>
#include <QRect>

class WinTrayPositioner
{
public:
    // Returns suggested top-left position for a flyout of size (w,h) near the system tray
    // within the screen work area. Falls back to bottom-right of primary availableGeometry.
    static QPoint suggestFlyoutTopLeft(int w, int h);
    static QRect trayRect(); // may be invalid if not available
};


