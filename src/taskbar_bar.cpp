#include "taskbar_bar.h"
#include "game_mode.h"
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <strsafe.h>
#include <commctrl.h>
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

namespace TaskbarBar {

static const WCHAR* REG_SUBKEY = L"Software\\rapoo-tray";
static const WCHAR* VAL_ENABLED = L"ShowTaskbarBar";
static const WCHAR* VAL_POSITION = L"TaskbarBarPosition";
static const WCHAR* VAL_WIDTH = L"TaskbarBarWidth";
static const WCHAR* VAL_ICON_SIZE = L"TaskbarBarIconSize";
static const WCHAR* VAL_ICON_COLOR = L"TaskbarBarIconColor";
static const WCHAR* VAL_ICON_ALIGN = L"TaskbarBarIconAlign";
static const WCHAR* VAL_ICON_OFFSET_Y = L"TaskbarBarIconOffsetY";
static const WCHAR* VAL_BAR_SPACING = L"TaskbarBarSpacing";
static const WCHAR* VAL_BAR_COLOR = L"TaskbarBarColor";
static const WCHAR* VAL_FONT_SIZE = L"TaskbarBarFontSize";
static const UINT_PTR TIMER_UPDATE_POS = 2001;

static HWND g_hWnd = NULL;
static HWND g_hMainAppWnd = NULL;
static HINSTANCE g_hInst = NULL;
static ULONG_PTR g_gdiplusToken = 0;
static BarClickCallback s_barClickCallback = nullptr;
static BarRightClickCallback s_barRightClickCallback = nullptr;

static bool s_enabled = true;
static int s_position = POS_RIGHT; // 0 = left, 1 = center, 2 = right
static int s_width = 140;          // Base 96-DPI width (100 ~ 280)
static int s_barSpacing = 2;       // Base 96-DPI spacing between text and progress bar (0 ~ 8, default 2)
static int s_iconSize = 18;        // Base 96-DPI icon size (12 ~ 28, default 18)
static int s_iconOffsetY = 0;      // Base 96-DPI vertical offset for category icon (-8 ~ 8, default 0)
static int s_fontSize = 11;        // Base 96-DPI text font size (9 ~ 16, default 11)
static int s_iconColorMode = ICON_COLOR_THEME; // 0 = Theme auto (default), 1 = Black, 2 = Accent
static int s_iconAlign = ICON_ALIGN_LEFT;      // 0 = Left (default), 1 = Right
static int s_barColorMode = BAR_COLOR_THEME;   // 0 = Theme auto (default), 1 = Black, 2 = Accent
static bool s_isHover = false;
static bool s_inGameMode = false;
static Device::State s_currentState = {};
static int s_lastDpi = 96;
static int s_lastPosX = -9999;
static int s_lastPosY = -9999;
static int s_lastPillW = 0;
static int s_lastPillH = 0;
static bool s_lastDark = true;

static inline int ScaleDpi(int val, int dpi) {
    return MulDiv(val, dpi, 96);
}

static UINT GetCurrentDpi(HWND hWnd) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        typedef UINT (WINAPI *pfnGetDpiForWindow)(HWND);
        pfnGetDpiForWindow fnGetDpi = (pfnGetDpiForWindow)GetProcAddress(hUser, "GetDpiForWindow");
        if (fnGetDpi && hWnd) {
            UINT d = fnGetDpi(hWnd);
            if (d > 0) return d;
        }
    }
    return 96;
}

static bool IsSystemDarkTheme() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 1, size = sizeof(DWORD), type = 0;
        if (RegQueryValueExW(hKey, L"SystemUsesLightTheme", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val == 0);
        }
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val == 0);
        }
        RegCloseKey(hKey);
    }
    return true;
}

static COLORREF GetSystemAccentColor(bool isDark) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        BYTE palette[32] = {0};
        DWORD size = sizeof(palette), type = 0;
        if (RegQueryValueExW(hKey, L"AccentPalette", NULL, &type, palette, &size) == ERROR_SUCCESS && size >= 28) {
            RegCloseKey(hKey);
            int idx = isDark ? 2 : 3;
            return RGB(palette[idx * 4 + 0], palette[idx * 4 + 1], palette[idx * 4 + 2]);
        }
        RegCloseKey(hKey);
    }
    DWORD dColor = 0;
    BOOL bOpaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&dColor, &bOpaque))) {
        return RGB((dColor >> 16) & 0xFF, (dColor >> 8) & 0xFF, dColor & 0xFF);
    }
    return isDark ? RGB(96, 205, 255) : RGB(0, 120, 215);
}

static bool LoadEnabledRegistry() {
    HKEY hKey;
    DWORD val = 1;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_ENABLED, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val != 0);
        }
        RegCloseKey(hKey);
    }
    return true;
}

static void SaveEnabledRegistry(bool enabled) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = enabled ? 1 : 0;
        RegSetValueExW(hKey, VAL_ENABLED, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadPositionRegistry() {
    HKEY hKey;
    DWORD val = POS_RIGHT;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_POSITION, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val <= POS_RIGHT) return (int)val;
        }
        RegCloseKey(hKey);
    }
    return POS_RIGHT;
}

static void SavePositionRegistry(int pos) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)pos;
        RegSetValueExW(hKey, VAL_POSITION, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadWidthRegistry() {
    HKEY hKey;
    DWORD val = 140;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_WIDTH, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val < 100) val = 100;
            if (val > 280) val = 280;
            return (int)val;
        }
        RegCloseKey(hKey);
    }
    return 140;
}

static void SaveWidthRegistry(int width) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)width;
        RegSetValueExW(hKey, VAL_WIDTH, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadIconSizeRegistry() {
    HKEY hKey;
    DWORD val = 18;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_ICON_SIZE, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val >= 12 && val <= 28) return (int)val;
        }
        RegCloseKey(hKey);
    }
    return 18;
}

static void SaveIconSizeRegistry(int size) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)size;
        RegSetValueExW(hKey, VAL_ICON_SIZE, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadIconColorRegistry() {
    HKEY hKey;
    DWORD val = ICON_COLOR_THEME; // Default: Follow theme
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_ICON_COLOR, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val <= 2) return (int)val;
        }
        RegCloseKey(hKey);
    }
    return ICON_COLOR_THEME;
}

static void SaveIconColorRegistry(int mode) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)mode;
        RegSetValueExW(hKey, VAL_ICON_COLOR, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadIconAlignRegistry() {
    HKEY hKey;
    DWORD val = ICON_ALIGN_LEFT;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_ICON_ALIGN, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val <= 1) return (int)val;
        }
        RegCloseKey(hKey);
    }
    return ICON_ALIGN_LEFT;
}

static void SaveIconAlignRegistry(int align) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)align;
        RegSetValueExW(hKey, VAL_ICON_ALIGN, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadIconOffsetYRegistry() {
    HKEY hKey;
    DWORD val = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_ICON_OFFSET_Y, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            int iv = (int)val;
            if (iv >= -8 && iv <= 8) return iv;
        }
        RegCloseKey(hKey);
    }
    return 0;
}

static void SaveIconOffsetYRegistry(int offset) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)offset;
        RegSetValueExW(hKey, VAL_ICON_OFFSET_Y, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadBarSpacingRegistry() {
    HKEY hKey;
    DWORD val = 2; // Default 2px
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_BAR_SPACING, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val <= 8) return (int)val;
        }
        RegCloseKey(hKey);
    }
    return 2;
}

static void SaveBarSpacingRegistry(int spacing) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)spacing;
        RegSetValueExW(hKey, VAL_BAR_SPACING, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadBarColorRegistry() {
    HKEY hKey;
    DWORD val = BAR_COLOR_THEME; // Default: Follow theme
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_BAR_COLOR, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val <= 2) return (int)val;
        }
        RegCloseKey(hKey);
    }
    return BAR_COLOR_THEME;
}

static void SaveBarColorRegistry(int mode) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)mode;
        RegSetValueExW(hKey, VAL_BAR_COLOR, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static int LoadFontSizeRegistry() {
    HKEY hKey;
    DWORD val = 11; // Default 11px
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, VAL_FONT_SIZE, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val >= 9 && val <= 16) return (int)val;
        }
        RegCloseKey(hKey);
    }
    return 11;
}

static void SaveFontSizeRegistry(int size) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)size;
        RegSetValueExW(hKey, VAL_FONT_SIZE, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

// Add rounded rectangle to GDI+ path
static void AddRoundRect(Gdiplus::GraphicsPath& path, Gdiplus::RectF bounds, float radius) {
    float diameter = radius * 2.0f;
    if (diameter > bounds.Width) diameter = bounds.Width;
    if (diameter > bounds.Height) diameter = bounds.Height;

    Gdiplus::RectF arcRect(bounds.X, bounds.Y, diameter, diameter);

    // Top-left
    path.AddArc(arcRect, 180.0f, 90.0f);

    // Top-right
    arcRect.X = bounds.X + bounds.Width - diameter;
    path.AddArc(arcRect, 270.0f, 90.0f);

    // Bottom-right
    arcRect.Y = bounds.Y + bounds.Height - diameter;
    path.AddArc(arcRect, 0.0f, 90.0f);

    // Bottom-left
    arcRect.X = bounds.X;
    path.AddArc(arcRect, 90.0f, 90.0f);

    path.CloseFigure();
}

static Gdiplus::Font* CreateFluentIconFont(float sizePx) {
    Gdiplus::Font* f = new Gdiplus::Font(L"Segoe Fluent Icons", sizePx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    if (f->GetLastStatus() == Gdiplus::Ok) return f;
    delete f;
    f = new Gdiplus::Font(L"Segoe MDL2 Assets", sizePx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    if (f->GetLastStatus() == Gdiplus::Ok) return f;
    delete f;
    return new Gdiplus::Font(L"Microsoft YaHei UI", sizePx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
}

static Gdiplus::Font* CreateTextFont(float sizePx, Gdiplus::FontStyle style) {
    Gdiplus::Font* f = new Gdiplus::Font(L"Segoe UI Variable Text", sizePx, style, Gdiplus::UnitPixel);
    if (f->GetLastStatus() == Gdiplus::Ok) return f;
    delete f;
    f = new Gdiplus::Font(L"Segoe UI", sizePx, style, Gdiplus::UnitPixel);
    if (f->GetLastStatus() == Gdiplus::Ok) return f;
    delete f;
    return new Gdiplus::Font(L"Microsoft YaHei UI", sizePx, style, Gdiplus::UnitPixel);
}

// Detect full-screen D3D games or borderless full-screen games, avoiding false positives on shell overlays/task switching
static bool IsFullscreenGameActive() {
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);

    // 1. Check Windows Shell User Notification State for full-screen 3D games
    HMODULE hShell = GetModuleHandleW(L"shell32.dll");
    if (!hShell) hShell = LoadLibraryW(L"shell32.dll");
    if (hShell) {
        typedef enum {
            _QUNS_NOT_PRESENT = 1,
            _QUNS_BUSY = 2,
            _QUNS_RUNNING_D3D_FULL_SCREEN = 3,
            _QUNS_PRESENTATION_MODE = 4,
            _QUNS_ACCEPTS_NOTIFICATIONS = 5,
            _QUNS_QUIET_HOURS = 6,
            _QUNS_APP = 7
        } _QUNS;
        typedef HRESULT (WINAPI *pfnSHQueryUserNotificationState)(_QUNS*);
        auto pfn = (pfnSHQueryUserNotificationState)GetProcAddress(hShell, "SHQueryUserNotificationState");
        if (pfn) {
            _QUNS quns = _QUNS_NOT_PRESENT;
            if (SUCCEEDED(pfn(&quns))) {
                if (quns == _QUNS_RUNNING_D3D_FULL_SCREEN) {
                    return true;
                }
            }
        }
    }

    // 2. Fallback: Borderless fullscreen detection
    HWND hFore = GetForegroundWindow();
    if (!hFore || hFore == GetDesktopWindow() || hFore == GetShellWindow() || hFore == g_hWnd || hFore == hTaskbar) {
        return false;
    }

    // Filter out known Windows Shell windows, Start Menu, Alt-Tab switcher, Task View, etc.
    WCHAR className[128] = {0};
    GetClassNameW(hFore, className, ARRAYSIZE(className));
    if (_wcsicmp(className, L"Progman") == 0 ||
        _wcsicmp(className, L"WorkerW") == 0 ||
        _wcsicmp(className, L"Shell_TrayWnd") == 0 ||
        _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0 ||
        _wcsicmp(className, L"XamlExplorerHostIslandWindow") == 0 ||
        _wcsicmp(className, L"MultitaskingViewFrame") == 0 ||
        _wcsicmp(className, L"Windows.UI.Core.CoreWindow") == 0 ||
        _wcsicmp(className, L"ForegroundStaging") == 0 ||
        _wcsicmp(className, L"TopLevelWindowForOverflowXamlIsland") == 0) {
        return false;
    }

    // Check process name of foreground window: ignore Windows Shell processes
    DWORD pid = 0;
    GetWindowThreadProcessId(hFore, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) {
        return false;
    }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        WCHAR procPath[MAX_PATH] = {0};
        DWORD sz = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, procPath, &sz)) {
            const WCHAR* pSlash = wcsrchr(procPath, L'\\');
            const WCHAR* exe = pSlash ? (pSlash + 1) : procPath;
            if (_wcsicmp(exe, L"explorer.exe") == 0 ||
                _wcsicmp(exe, L"SearchHost.exe") == 0 ||
                _wcsicmp(exe, L"SearchApp.exe") == 0 ||
                _wcsicmp(exe, L"ShellExperienceHost.exe") == 0 ||
                _wcsicmp(exe, L"StartMenuExperienceHost.exe") == 0 ||
                _wcsicmp(exe, L"LockApp.exe") == 0 ||
                _wcsicmp(exe, L"LogonUI.exe") == 0 ||
                _wcsicmp(exe, L"Taskmgr.exe") == 0) {
                CloseHandle(hProc);
                return false;
            }
        }
        CloseHandle(hProc);
    }

    RECT rcApp = {0};
    if (GetWindowRect(hFore, &rcApp)) {
        HMONITOR hMon = MonitorFromWindow(hFore, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(hMon, &mi)) {
            if (rcApp.left <= mi.rcMonitor.left &&
                rcApp.top <= mi.rcMonitor.top &&
                rcApp.right >= mi.rcMonitor.right &&
                rcApp.bottom >= mi.rcMonitor.bottom) {
                return true;
            }
        }
    }
    return false;
}

static bool CalculateTaskbarPosition(int pillW, int pillH, int dpi, int* outX, int* outY) {
    auto S = [dpi](int v) { return ScaleDpi(v, dpi); };

    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!hTaskbar || !IsWindow(hTaskbar)) {
        return false;
    }

    RECT rcTaskbar = {0};
    RECT rcTray = {0};
    RECT rcStart = {0};
    RECT rcTasks = {0};

    GetWindowRect(hTaskbar, &rcTaskbar);

    HWND hTray = FindWindowExW(hTaskbar, NULL, L"TrayNotifyWnd", NULL);
    if (hTray && IsWindow(hTray)) {
        GetWindowRect(hTray, &rcTray);
    }

    HWND hStart = FindWindowExW(hTaskbar, NULL, L"Start", NULL);
    if (hStart && IsWindow(hStart)) {
        GetWindowRect(hStart, &rcStart);
    }

    HWND hRebar = FindWindowExW(hTaskbar, NULL, L"ReBarWindow32", NULL);
    if (hRebar && IsWindow(hRebar)) {
        GetWindowRect(hRebar, &rcTasks);
    } else {
        HWND hTaskSw = FindWindowExW(hTaskbar, NULL, L"MSTaskSwWClass", NULL);
        if (hTaskSw && IsWindow(hTaskSw)) {
            GetWindowRect(hTaskSw, &rcTasks);
        }
    }

    if (rcTaskbar.right <= rcTaskbar.left || rcTaskbar.bottom <= rcTaskbar.top) {
        APPBARDATA abd = { sizeof(APPBARDATA) };
        abd.hWnd = hTaskbar;
        if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
            rcTaskbar = abd.rc;
        }
    }

    if (rcTaskbar.right <= rcTaskbar.left || rcTaskbar.bottom <= rcTaskbar.top) {
        return false;
    }

    int taskbarW = rcTaskbar.right - rcTaskbar.left;
    int taskbarH = rcTaskbar.bottom - rcTaskbar.top;
    bool isHorizontal = (taskbarW >= taskbarH);

    int posX = 0, posY = 0;

    if (isHorizontal) {
        posY = rcTaskbar.top + (taskbarH - pillH) / 2;

        // Detect running apps / Start button cluster in the center or left
        int clusterLeft = rcTaskbar.right;
        int clusterRight = rcTaskbar.left;
        bool hasCluster = false;

        if (rcStart.right > rcStart.left) {
            clusterLeft = std::min(clusterLeft, (int)rcStart.left);
            clusterRight = std::max(clusterRight, (int)rcStart.right);
            hasCluster = true;
        }
        if (rcTasks.right > rcTasks.left) {
            clusterLeft = std::min(clusterLeft, (int)rcTasks.left);
            clusterRight = std::max(clusterRight, (int)rcTasks.right);
            hasCluster = true;
        }

        int trayLeft = rcTaskbar.right - S(160);
        if (rcTray.right > rcTray.left && rcTray.left > rcTaskbar.left && rcTray.left < rcTaskbar.right) {
            trayLeft = rcTray.left;
        }

        int leftBoundary = rcTaskbar.left + S(12);
        int rightBoundary = trayLeft - S(6);

        if (s_position == POS_LEFT) {
            // 1. 靠左侧 (Left)
            // In Windows 11 centered icons, the area from leftBoundary to clusterLeft is completely open.
            // If icons are left-aligned (cluster starts near left edge), place immediately after task icons.
            if (hasCluster && clusterLeft < rcTaskbar.left + S(80)) {
                // Left-aligned task icons: place right after the task buttons
                posX = clusterRight + S(8);
            } else {
                // Centered task icons: place on the far left, completely avoiding the center icons
                posX = leftBoundary;
                // Never overlap with the center task cluster
                if (hasCluster && posX + pillW > clusterLeft - S(8)) {
                    posX = clusterLeft - pillW - S(8);
                }
            }
            if (posX < leftBoundary) posX = leftBoundary;
        } else if (s_position == POS_CENTER) {
            // 2. 居中 (Center)
            // If task icons are centered, the screen center is occupied by Start and app icons.
            // In this case, center within the left open region, or within the right open region.
            if (hasCluster && clusterLeft > rcTaskbar.left + S(200)) {
                int leftFreeW = (clusterLeft - S(8)) - leftBoundary;
                if (leftFreeW >= pillW) {
                    posX = leftBoundary + (leftFreeW - pillW) / 2;
                } else {
                    int rightFreeW = rightBoundary - (clusterRight + S(8));
                    posX = clusterRight + S(8) + std::max(0, (rightFreeW - pillW) / 2);
                }
            } else {
                // Left-aligned task icons: dead center of the taskbar is wide open!
                int deadCenter = rcTaskbar.left + (taskbarW - pillW) / 2;
                if (hasCluster && deadCenter < clusterRight + S(8)) {
                    deadCenter = clusterRight + S(8);
                }
                if (deadCenter + pillW > rightBoundary) {
                    deadCenter = rightBoundary - pillW;
                }
                posX = deadCenter;
            }
        } else {
            // 3. 靠右侧 (Right) - Default
            posX = rightBoundary - pillW;
            // Never overlap with center task cluster on the left of it
            if (hasCluster && posX < clusterRight + S(8)) {
                posX = clusterRight + S(8);
            }
        }

        if (posX + pillW > rcTaskbar.right - S(6)) {
            posX = rcTaskbar.right - pillW - S(6);
        }
        if (posX < rcTaskbar.left + S(6)) {
            posX = rcTaskbar.left + S(6);
        }
    } else {
        // Vertical taskbar
        posX = rcTaskbar.left + (taskbarW - pillW) / 2;
        if (s_position == POS_LEFT) {
            posY = rcTaskbar.top + S(8);
        } else if (s_position == POS_CENTER) {
            posY = rcTaskbar.top + (taskbarH - pillH) / 2;
        } else {
            int trayTop = rcTray.top;
            if (trayTop <= rcTaskbar.top || trayTop >= rcTaskbar.bottom) {
                trayTop = rcTaskbar.bottom - S(160);
            }
            posY = trayTop - pillH - S(4);
            if (posY < rcTaskbar.top + S(8)) posY = rcTaskbar.top + S(8);
        }
    }

    *outX = posX;
    *outY = posY;
    return true;
}

static void RepaintBar() {
    if (!g_hWnd || !IsWindow(g_hWnd) || !s_enabled) return;

    if (s_inGameMode) {
        ShowWindow(g_hWnd, SW_HIDE);
        return;
    }

    int dpi = GetCurrentDpi(g_hWnd);
    s_lastDpi = dpi;
    auto S = [dpi](int v) { return ScaleDpi(v, dpi); };

    int pillW = ScaleDpi(s_width, dpi);
    int pillH = S(32);

    int posX = 0, posY = 0;
    if (!CalculateTaskbarPosition(pillW, pillH, dpi, &posX, &posY)) {
        s_lastPosX = -9999;
        s_lastPosY = -9999;
        return;
    }

    HDC hdcScreen = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = pillW;
    bmi.bmiHeader.biHeight = -pillH; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = NULL;
    HBITMAP hbm = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (!hbm || !pvBits) {
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return;
    }

    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hbm);

    bool isDark = IsSystemDarkTheme();
    COLORREF accentCr = GetSystemAccentColor(isDark);
    BYTE aR = GetRValue(accentCr), aG = GetGValue(accentCr), aB = GetBValue(accentCr);

    {
        Gdiplus::Graphics g(memDC);
        // Razor-sharp anti-aliasing without blur on transparent 32-bit DIB
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeDefault);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // 1. Background: Seamlessly blend into taskbar (no border / card background by default)
        // Soft hover feedback pill only when user moves mouse over it
        if (s_isHover) {
            Gdiplus::GraphicsPath hoverPath;
            Gdiplus::RectF hoverRect(0.5f, 0.5f, (float)pillW - 1.0f, (float)pillH - 1.0f);
            AddRoundRect(hoverPath, hoverRect, (float)S(5));
            Gdiplus::Color hoverBg = isDark ? Gdiplus::Color(24, 255, 255, 255) : Gdiplus::Color(16, 0, 0, 0);
            Gdiplus::SolidBrush hoverBrush(hoverBg);
            g.FillPath(&hoverBrush, &hoverPath);
        }

        // 2. Battery status & colors
        const auto& st = s_currentState;
        bool hasBat = (st.hasBattery != false);
        int battery = st.battery;
        if (battery < 0) battery = 0;
        if (battery > 100) battery = 100;

        // Progress bar color: DEFAULT IS FOLLOW THEME (0 = Theme auto, 1 = Black, 2 = Accent)
        Gdiplus::Color barColor(255, 0, 0, 0);
        if (s_barColorMode == BAR_COLOR_THEME) {
            barColor = isDark ? Gdiplus::Color(255, 240, 244, 248) : Gdiplus::Color(255, 28, 33, 40);
        } else if (s_barColorMode == BAR_COLOR_BLACK) {
            barColor = Gdiplus::Color(255, 0, 0, 0);
        } else if (s_barColorMode == BAR_COLOR_ACCENT) {
            barColor = Gdiplus::Color(255, aR, aG, aB);
        }
        if (!st.isConnected) {
            barColor = Gdiplus::Color(120, barColor.GetR(), barColor.GetG(), barColor.GetB());
        }

        // 3. Device Category Icon (Mouse, Keyboard, Headset) & Layout
        int iconBox = S(s_iconSize);
        float glyphPt = (float)S((int)(s_iconSize * 0.82f + 0.5f));
        if (glyphPt < 8.0f) glyphPt = 8.0f;
        std::unique_ptr<Gdiplus::Font> iconFont(CreateFluentIconFont(glyphPt));

        int iconX = 0, iconY = (pillH - iconBox) / 2 + S(s_iconOffsetY);
        int contentX = 0, contentW = 0;

        if (s_iconAlign == ICON_ALIGN_RIGHT) {
            // Content on Left, Icon on Right
            contentX = S(6);
            contentW = pillW - contentX - iconBox - S(8);
            if (contentW < S(24)) contentW = S(24);
            iconX = contentX + contentW + S(4);
        } else {
            // Default: Icon on Left, Content on Right
            iconX = S(6);
            contentX = iconX + iconBox + S(6);
            contentW = pillW - contentX - S(6);
            if (contentW < S(24)) contentW = S(24);
        }

        Gdiplus::RectF iconRect((float)iconX, (float)iconY, (float)iconBox, (float)iconBox);

        Gdiplus::StringFormat sfCenter;
        sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        // Device icon color: DEFAULT IS FOLLOW THEME (0 = Theme auto, 1 = Black, 2 = Accent)
        Gdiplus::Color iconColor(255, 0, 0, 0);
        if (s_iconColorMode == ICON_COLOR_THEME) {
            iconColor = isDark ? Gdiplus::Color(255, 240, 244, 248) : Gdiplus::Color(255, 28, 33, 40);
        } else if (s_iconColorMode == ICON_COLOR_BLACK) {
            iconColor = Gdiplus::Color(255, 0, 0, 0);
        } else if (s_iconColorMode == ICON_COLOR_ACCENT) {
            iconColor = Gdiplus::Color(255, aR, aG, aB);
        }
        if (!st.isConnected) {
            iconColor = Gdiplus::Color(120, iconColor.GetR(), iconColor.GetG(), iconColor.GetB());
        }

        // Draw Device Category Icon
        WCHAR szIcon[2] = { (WCHAR)0xE962, 0 }; // Default Mouse
        if (wcscmp(st.category, L"keyboard") == 0) {
            szIcon[0] = (WCHAR)0xE92C;
        } else if (wcscmp(st.category, L"headset") == 0) {
            szIcon[0] = (WCHAR)0xE95B;
        } else if (wcscmp(st.category, L"gamepad") == 0) {
            szIcon[0] = (WCHAR)0xE7FC;
        }

        Gdiplus::SolidBrush iconBrush(iconColor);
        g.DrawString(szIcon, -1, iconFont.get(), iconRect, &sfCenter, &iconBrush);

        // 4. Content: Top row = Device Name + Battery %, Bottom row = Battery Progress Bar
        int topH = S((int)(s_fontSize * 1.30f + 0.5f));
        int barH = S(4);
        int spacing = S(s_barSpacing);
        int totalContentH = topH + spacing + barH;
        int topY = (pillH - totalContentH) / 2;
        if (topY < S(1)) topY = S(1);

        // Top row font scaled by personalized font size
        float namePt = (float)S(s_fontSize);
        float pctPt = (float)S((int)(s_fontSize * 0.95f + 0.5f));
        if (pctPt < 8.0f) pctPt = 8.0f;
        std::unique_ptr<Gdiplus::Font> nameFont(CreateTextFont(namePt, Gdiplus::FontStyleRegular));
        std::unique_ptr<Gdiplus::Font> pctFont(CreateTextFont(pctPt, Gdiplus::FontStyleBold));

        const WCHAR* displayName = st.modelName[0] ? st.modelName : L"无线设备";

        WCHAR szPct[32] = {0};
        if (st.isConnected) {
            if (st.isCharging) {
                StringCchPrintfW(szPct, ARRAYSIZE(szPct), L"⚡%d%%", battery);
            } else {
                StringCchPrintfW(szPct, ARRAYSIZE(szPct), L"%d%%", battery);
            }
        } else {
            StringCchCopyW(szPct, ARRAYSIZE(szPct), L"未连接");
        }

        // Measure percentage text width for precise right-alignment
        Gdiplus::RectF pctBound;
        g.MeasureString(szPct, -1, pctFont.get(), Gdiplus::PointF(0, 0), &pctBound);
        float pctW = pctBound.Width + (float)S(2);
        float nameW = (float)contentW - pctW - (float)S(4);
        if (nameW < (float)S(24)) nameW = (float)S(24);

        // Draw Device Name
        Gdiplus::RectF nameRect((float)contentX, (float)topY, nameW, (float)topH);
        Gdiplus::StringFormat sfLeft;
        sfLeft.SetAlignment(Gdiplus::StringAlignmentNear);
        sfLeft.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfLeft.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sfLeft.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        Gdiplus::Color nameColor = isDark ? Gdiplus::Color(255, 240, 244, 248) : Gdiplus::Color(255, 28, 33, 40);
        Gdiplus::SolidBrush nameBrush(nameColor);
        g.DrawString(displayName, -1, nameFont.get(), nameRect, &sfLeft, &nameBrush);

        // Draw Percentage text
        Gdiplus::RectF pctRect((float)contentX + (float)contentW - pctW, (float)topY, pctW, (float)topH);
        Gdiplus::StringFormat sfRight;
        sfRight.SetAlignment(Gdiplus::StringAlignmentFar);
        sfRight.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfRight.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        Gdiplus::SolidBrush pctBrush(barColor);
        g.DrawString(szPct, -1, pctFont.get(), pctRect, &sfRight, &pctBrush);

        // 5. Bottom row: Battery Progress Bar
        int barX = contentX;
        int barW = contentW;
        int barY = topY + topH + spacing;

        // Progress bar background track
        Gdiplus::GraphicsPath trackPath;
        Gdiplus::RectF trackRect((float)barX, (float)barY, (float)barW, (float)barH);
        AddRoundRect(trackPath, trackRect, (float)S(2));
        Gdiplus::Color trackColor = isDark ? Gdiplus::Color(45, 255, 255, 255) : Gdiplus::Color(28, 0, 0, 0);
        Gdiplus::SolidBrush trackBrush(trackColor);
        g.FillPath(&trackBrush, &trackPath);

        // Progress bar fill
        if (hasBat && st.isConnected && battery > 0) {
            float fillW = (float)barW * ((float)battery / 100.0f);
            if (fillW < (float)S(4)) fillW = (float)S(4);
            if (fillW > (float)barW) fillW = (float)barW;

            Gdiplus::GraphicsPath fillPath;
            Gdiplus::RectF fillRect((float)barX, (float)barY, fillW, (float)barH);
            AddRoundRect(fillPath, fillRect, (float)S(2));
            Gdiplus::SolidBrush fillBrush(barColor);
            g.FillPath(&fillBrush, &fillPath);
        }
    }

    POINT ptSrc = { 0, 0 };
    SIZE sizeWnd = { pillW, pillH };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    POINT ptDst = { posX, posY };

    UpdateLayeredWindow(g_hWnd, hdcScreen, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &bf, ULW_ALPHA);

    SetWindowPos(g_hWnd, HWND_TOPMOST, posX, posY, pillW, pillH, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    s_lastPosX = posX;
    s_lastPosY = posY;
    s_lastPillW = pillW;
    s_lastPillH = pillH;
    s_lastDark = isDark;

    SelectObject(memDC, oldBmp);
    DeleteObject(hbm);
    DeleteDC(memDC);
    ReleaseDC(NULL, hdcScreen);
}

void CheckAndEnsureVisible();

static LRESULT CALLBACK TaskbarBarWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_CREATE: {
            SetTimer(hWnd, TIMER_UPDATE_POS, 1000, NULL);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == TIMER_UPDATE_POS) {
                CheckAndEnsureVisible();
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!s_isHover) {
                s_isHover = true;
                RepaintBar();
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&tme);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (s_isHover) {
                s_isHover = false;
                RepaintBar();
            }
            return 0;
        }

        case WM_SETCURSOR: {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }

        case WM_LBUTTONUP: {
            // Left click: Toggle the Windows 11 Native Notification Card (Flyout)
            if (s_barClickCallback) {
                s_barClickCallback();
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            // Right click: Open the acrylic tray context menu
            if (s_barRightClickCallback) {
                POINT pt;
                GetCursorPos(&pt);
                s_barRightClickCallback(pt.x, pt.y);
            }
            return 0;
        }

        case WM_DWMCOLORIZATIONCOLORCHANGED:
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED: {
            RepaintBar();
            return 0;
        }

        case WM_DESTROY: {
            KillTimer(hWnd, TIMER_UPDATE_POS);
            g_hWnd = NULL;
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

static bool s_classRegistered = false;

static bool EnsureBarWindowCreated() {
    if (g_hWnd && IsWindow(g_hWnd)) {
        return true;
    }

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = TaskbarBarWndProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = L"RapooTaskbarBarWindow";
        wc.hCursor = LoadCursor(NULL, IDC_HAND);
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        L"RapooTaskbarBarWindow",
        L"RapooTaskbarBar",
        WS_POPUP,
        0, 0, 140, 32,
        NULL, NULL, g_hInst, NULL
    );

    if (!g_hWnd) {
        return false;
    }

    s_lastPosX = -9999;
    s_lastPosY = -9999;
    s_lastPillW = 0;
    s_lastPillH = 0;

    return true;
}

bool Init(HINSTANCE hInstance, HWND hMainWnd) {
    g_hInst = hInstance;
    g_hMainAppWnd = hMainWnd;

    Gdiplus::GdiplusStartupInput gpsi;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gpsi, NULL) != Gdiplus::Ok) {
        return false;
    }

    s_enabled = LoadEnabledRegistry();
    s_position = LoadPositionRegistry();
    s_width = LoadWidthRegistry();
    s_barSpacing = LoadBarSpacingRegistry();
    s_iconSize = LoadIconSizeRegistry();
    s_iconOffsetY = LoadIconOffsetYRegistry();
    s_fontSize = LoadFontSizeRegistry();
    s_iconColorMode = LoadIconColorRegistry();
    s_iconAlign = LoadIconAlignRegistry();
    s_barColorMode = LoadBarColorRegistry();

    EnsureBarWindowCreated();

    if (s_enabled) {
        s_currentState = Device::GetCurrentState();
        RepaintBar();
    } else if (g_hWnd && IsWindow(g_hWnd)) {
        ShowWindow(g_hWnd, SW_HIDE);
    }

    return true;
}

void Recreate() {
    if (g_hWnd && IsWindow(g_hWnd)) {
        DestroyWindow(g_hWnd);
        g_hWnd = NULL;
    }
    s_lastPosX = -9999;
    s_lastPosY = -9999;
    s_lastPillW = 0;
    s_lastPillH = 0;

    if (EnsureBarWindowCreated() && s_enabled) {
        s_currentState = Device::GetCurrentState();
        RepaintBar();
    }
}

void CheckAndEnsureVisible() {
    if (!s_enabled) return;

    // 1. Ensure bar window handle is valid
    if (!g_hWnd || !IsWindow(g_hWnd)) {
        if (!EnsureBarWindowCreated()) {
            return;
        }
    }

    // 2. Fullscreen game check
    bool inGame = IsFullscreenGameActive();
    if (inGame != s_inGameMode) {
        s_inGameMode = inGame;
        if (s_inGameMode) {
            ShowWindow(g_hWnd, SW_HIDE);
            return;
        } else {
            RepaintBar();
            return;
        }
    }

    if (s_inGameMode) {
        return;
    }

    // 3. Check Taskbar readiness
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!hTaskbar || !IsWindow(hTaskbar)) {
        return;
    }

    // 4. Check geometry, theme and visibility
    int dpi = GetCurrentDpi(g_hWnd);
    int pillW = ScaleDpi(s_width, dpi);
    int pillH = ScaleDpi(32, dpi);
    int curX = 0, curY = 0;
    if (!CalculateTaskbarPosition(pillW, pillH, dpi, &curX, &curY)) {
        return;
    }

    bool dark = IsSystemDarkTheme();
    bool needRepaint = (curX != s_lastPosX || curY != s_lastPosY ||
                        pillW != s_lastPillW || pillH != s_lastPillH ||
                        dark != s_lastDark || !IsWindowVisible(g_hWnd));

    if (needRepaint) {
        RepaintBar();
    } else {
        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void Cleanup() {
    if (g_hWnd && IsWindow(g_hWnd)) {
        DestroyWindow(g_hWnd);
        g_hWnd = NULL;
    }
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

void UpdateState(const Device::State& state) {
    s_currentState = state;
    if (s_enabled && !s_inGameMode) {
        if (!g_hWnd || !IsWindow(g_hWnd)) {
            EnsureBarWindowCreated();
        }
        RepaintBar();
    }
}

void SetEnabled(bool enabled) {
    s_enabled = enabled;
    SaveEnabledRegistry(enabled);
    if (!g_hWnd || !IsWindow(g_hWnd)) return;

    if (s_enabled && !s_inGameMode) {
        s_currentState = Device::GetCurrentState();
        RepaintBar();
    } else {
        ShowWindow(g_hWnd, SW_HIDE);
    }
}

bool IsEnabled() {
    return s_enabled;
}

void SetPosition(int pos) {
    if (pos < POS_LEFT || pos > POS_RIGHT) pos = POS_RIGHT;
    s_position = pos;
    SavePositionRegistry(s_position);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetPosition() {
    return s_position;
}

void SetWidth(int width) {
    if (width < 100) width = 100;
    if (width > 280) width = 280;
    s_width = width;
    SaveWidthRegistry(s_width);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetWidth() {
    return s_width;
}

void SetBarSpacing(int spacing) {
    if (spacing < 0) spacing = 0;
    if (spacing > 8) spacing = 8;
    s_barSpacing = spacing;
    SaveBarSpacingRegistry(s_barSpacing);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetBarSpacing() {
    return s_barSpacing;
}

void SetIconSize(int size) {
    if (size < 12) size = 12;
    if (size > 28) size = 28;
    s_iconSize = size;
    SaveIconSizeRegistry(s_iconSize);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetIconSize() {
    return s_iconSize;
}

void SetIconOffsetY(int offset) {
    if (offset < -8) offset = -8;
    if (offset > 8) offset = 8;
    s_iconOffsetY = offset;
    SaveIconOffsetYRegistry(s_iconOffsetY);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetIconOffsetY() {
    return s_iconOffsetY;
}

void SetFontSize(int size) {
    if (size < 9) size = 9;
    if (size > 16) size = 16;
    s_fontSize = size;
    SaveFontSizeRegistry(s_fontSize);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetFontSize() {
    return s_fontSize;
}

void SetIconColorMode(int mode) {
    if (mode < 0 || mode > 2) mode = ICON_COLOR_THEME;
    s_iconColorMode = mode;
    SaveIconColorRegistry(s_iconColorMode);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetIconColorMode() {
    return s_iconColorMode;
}

void SetBarColorMode(int mode) {
    if (mode < 0 || mode > 2) mode = BAR_COLOR_THEME;
    s_barColorMode = mode;
    SaveBarColorRegistry(s_barColorMode);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetBarColorMode() {
    return s_barColorMode;
}

void SetIconAlign(int align) {
    if (align < 0 || align > 1) align = ICON_ALIGN_LEFT;
    s_iconAlign = align;
    SaveIconAlignRegistry(s_iconAlign);
    if (g_hWnd && IsWindow(g_hWnd) && s_enabled && !s_inGameMode) {
        RepaintBar();
    }
}

int GetIconAlign() {
    return s_iconAlign;
}

bool GetBarRect(RECT* pRect) {
    if (!pRect) return false;
    if (!g_hWnd || !IsWindow(g_hWnd) || !s_enabled) return false;
    return GetWindowRect(g_hWnd, pRect) != FALSE;
}

void Show() {
    if (!s_enabled) {
        SetEnabled(true);
    } else if (g_hWnd && IsWindow(g_hWnd) && !s_inGameMode) {
        RepaintBar();
    }
}

void Hide() {
    if (g_hWnd && IsWindow(g_hWnd)) {
        ShowWindow(g_hWnd, SW_HIDE);
    }
}

void Invalidate() {
    if (s_enabled && !s_inGameMode) {
        if (!g_hWnd || !IsWindow(g_hWnd)) {
            EnsureBarWindowCreated();
        }
        s_lastPosX = -9999;
        s_lastPosY = -9999;
        RepaintBar();
    }
}

HWND GetWindowHandle() {
    return g_hWnd;
}

void SetClickCallbacks(BarClickCallback onClick, BarRightClickCallback onRightClick) {
    s_barClickCallback = onClick;
    s_barRightClickCallback = onRightClick;
}

} // namespace TaskbarBar
