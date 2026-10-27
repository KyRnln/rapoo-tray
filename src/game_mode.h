#pragma once
#include <windows.h>
#include <shellapi.h>

namespace GameMode {

#ifndef QUNS_NOT_PRESENT
typedef enum {
    QUNS_NOT_PRESENT = 1,
    QUNS_BUSY = 2,
    QUNS_RUNNING_D3D_FULL_SCREEN = 3,
    QUNS_PRESENTATION_MODE = 4,
    QUNS_ACCEPTS_NOTIFICATIONS = 5,
    QUNS_QUIET_HOURS = 6,
    QUNS_APP = 7
} QUERY_USER_NOTIFICATION_STATE;
#endif

// Check if a full-screen 3D/DirectX game, presentation, or full-screen borderless window is active.
inline bool IsFullscreenOrGamingActive() {
    // 1. Check Windows Shell User Notification State (Direct3D full screen, presentation, quiet hours)
    HMODULE hShell = GetModuleHandleW(L"shell32.dll");
    if (!hShell) hShell = LoadLibraryW(L"shell32.dll");
    if (hShell) {
        typedef HRESULT (WINAPI *pfnSHQueryUserNotificationState)(QUERY_USER_NOTIFICATION_STATE*);
        pfnSHQueryUserNotificationState pfnQuery = 
            (pfnSHQueryUserNotificationState)GetProcAddress(hShell, "SHQueryUserNotificationState");
        if (pfnQuery) {
            QUERY_USER_NOTIFICATION_STATE quns = QUNS_NOT_PRESENT;
            if (SUCCEEDED(pfnQuery(&quns))) {
                switch (quns) {
                    case QUNS_RUNNING_D3D_FULL_SCREEN:
                    case QUNS_PRESENTATION_MODE:
                    case QUNS_BUSY:
                    case QUNS_QUIET_HOURS:
                    case QUNS_APP:
                        return true;
                    default:
                        break;
                }
            }
        }
    }

    // 2. Fallback: Borderless full-screen or Vulkan/OpenGL full-screen detection
    HWND hFore = GetForegroundWindow();
    if (hFore && hFore != GetDesktopWindow() && hFore != GetShellWindow()) {
        WCHAR className[64] = {0};
        GetClassNameW(hFore, className, ARRAYSIZE(className));
        if (_wcsicmp(className, L"Progman") != 0 &&
            _wcsicmp(className, L"WorkerW") != 0 &&
            _wcsicmp(className, L"Shell_TrayWnd") != 0 &&
            _wcsicmp(className, L"Shell_SecondaryTrayWnd") != 0) {
            
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
        }
    }

    return false;
}

} // namespace GameMode
