#include "WinAcrylic.h"

#include <dwmapi.h>

// SetWindowCompositionAttribute is undocumented but widely used for acrylic.
// We keep it optional: if unavailable, we fall back to transparent + dark QML background.
typedef BOOL(WINAPI *pSetWindowCompositionAttribute)(HWND, void *);

enum ACCENT_STATE
{
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_ENABLE_HOSTBACKDROP = 5
};

struct ACCENT_POLICY
{
    int AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    int Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

static void setAccent(HWND hwnd, ACCENT_STATE state, int gradientColorArgb)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32)
        return;

    auto fn = reinterpret_cast<pSetWindowCompositionAttribute>(GetProcAddress(user32, "SetWindowCompositionAttribute"));
    if (!fn)
        return;

    ACCENT_POLICY policy{};
    policy.AccentState = static_cast<int>(state);
    policy.AccentFlags = 2; // draw all borders
    policy.GradientColor = gradientColorArgb; // AABBGGRR

    WINDOWCOMPOSITIONATTRIBDATA data{};
    data.Attrib = 19; // WCA_ACCENT_POLICY
    data.pvData = &policy;
    data.cbData = sizeof(policy);

    fn(hwnd, &data);
}

namespace WinAcrylic {

void enableAcrylic(HWND hwnd)
{
    if (!hwnd)
        return;

    // Dark tint with ~60% alpha; acrylic/host backdrop does the blur.
    // Format is AABBGGRR.
    const int argb = (0x99 << 24) | (0x20 << 16) | (0x20 << 8) | (0x20);
    setAccent(hwnd, ACCENT_ENABLE_ACRYLICBLURBEHIND, argb);

    // Ensure DWM draws shadow where possible.
    enableShadow(hwnd);
    applyRoundedCorners(hwnd);
}

void disableAcrylic(HWND hwnd)
{
    if (!hwnd)
        return;
    setAccent(hwnd, ACCENT_DISABLED, 0);
}

void applyRoundedCorners(HWND hwnd)
{
    if (!hwnd)
        return;

    // Win11 rounded corners.
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
    const DWORD pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
}

void enableShadow(HWND hwnd)
{
    if (!hwnd)
        return;

    // Extending the frame can encourage shadow + blur region composition.
    const MARGINS m = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &m);
}

} // namespace WinAcrylic


