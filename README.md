# Earie (Qt 6 / Windows 11) — EarTrumpet-like per‑app audio mixer

This repo contains **one** app:

- `earie` (CMake): **tray-only** EarTrumpet-like flyout with **per-device** and **per-session** volume + mute, backed by **Windows Core Audio**.

## Build (Qt Creator, Windows 11, MSVC)

### Prereqs
- Windows 11
- Qt 6.8+ (tested with Qt 6.10 style APIs)
- Qt Creator
- **MSVC kit** (recommended) with:
  - Qt Quick
  - Qt Quick Controls 2
  - Qt Widgets (for `QSystemTrayIcon`)

### Build steps
1. In Qt Creator, open `CMakeLists.txt` (or “Open Project” on the repo folder)
2. Select an **MSVC** kit (e.g. “Desktop Qt 6.x MSVC 2022 64-bit”)
3. Build + Run

The app starts **tray-only**. Left-click the tray icon toggles the flyout; right-click shows the context menu.

## Config (JSON)

Stored at:
- `%APPDATA%/Earie/config.json`

Schema (v1):

```json
{
  "schemaVersion": 1,
  "mode": "default",                // "default" | "all"
  "showSystemSessions": false,      // default false
  "hiddenDevices": ["<deviceId>"],
  "hiddenProcessesGlobal": ["C:\\Path\\App.exe"],
  "hiddenProcessesPerDevice": {
    "<deviceId>": ["C:\\Path\\App.exe"]
  }
}
```

Notes:
- Devices are hidden **by IMMDevice id** (stable string from `IMMDevice::GetId`).
- Processes are hidden **by full exe path** (preferred), resolved from session PID.


