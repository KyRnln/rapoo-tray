#include <windows.h>
#include <shellapi.h>
#include <dbt.h>
extern "C" {
#include <hidsdi.h>
}
#include <strsafe.h>
#include "rapoo_protocol.h"
#include "device_manager.h"
#include "alert_window.h"
#include "tray_menu.h"
#include "flyout_window.h"
#include "settings_window.h"
#include "game_mode.h"
#include "taskbar_bar.h"

static const UINT WM_APP_TRAYMSG = WM_APP + 1;
static const UINT WM_APP_STATE_UPDATE = WM_APP + 2;

static HWND g_hMainWnd = NULL;
static NOTIFYICONDATAW g_nid = {0};
static UINT g_uTaskbarRestartMsg = 0;
static HICON g_hCurrentTrayIcon = NULL;

static void RefreshTrayUI(const Device::State& state) {
    if (!g_hMainWnd) return;

    // The system tray icon displays the project icon instead of battery icon.
    // Dynamic battery and connection info are presented in the rich tooltip.
    Tray::UpdateTooltip(g_nid, state);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);

    Flyout::Invalidate();
    Settings::Invalidate();
    TaskbarBar::UpdateState(state);
    Alert::CheckBattery(state);
}

static void OnDeviceStateChanged(const Device::State& state, DWORD changeMask) {
    if (!g_hMainWnd) return;
    PostMessageW(g_hMainWnd, WM_APP_STATE_UPDATE, 0, 0);
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == g_uTaskbarRestartMsg) {
        Shell_NotifyIconW(NIM_ADD, &g_nid);
        RefreshTrayUI(Device::GetCurrentState());
        TaskbarBar::Invalidate();
        return 0;
    }

    switch (msg) {
        case WM_APP_TRAYMSG: {
            if (lParam == WM_LBUTTONUP) {
                // 点击图标不再弹出电量面板 (User requirement: Click icon no longer pops up panel)
            } else if (lParam == WM_RBUTTONUP) {
                SetForegroundWindow(hWnd);
                Tray::ShowMenu(hWnd);
            }
            return 0;
        }

        case WM_APP_STATE_UPDATE: {
            RefreshTrayUI(Device::GetCurrentState());
            return 0;
        }

        case WM_DWMCOLORIZATIONCOLORCHANGED:
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED: {
            RefreshTrayUI(Device::GetCurrentState());
            TaskbarBar::Invalidate();
            return 0;
        }

        case WM_DEVICECHANGE: {
            if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE || wParam == DBT_DEVNODES_CHANGED) {
                Device::NotifyDeviceChange();
            }
            return 0;
        }

        case WM_DESTROY: {
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            if (g_hCurrentTrayIcon) {
                DestroyIcon(g_hCurrentTrayIcon);
                g_hCurrentTrayIcon = NULL;
            }
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Single instance protection
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Local\\rapoo-traySingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    // Ensure WebView2 engine defaults to fully transparent background across all windows
    // (Prevents Chromium white flash and eliminates opaque white canvas blocking Windows 11 Mica)
    SetEnvironmentVariableW(L"WEBVIEW2_DEFAULT_BACKGROUND_COLOR", L"00000000");
    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", L"--disable-features=msWebView2DefaultBackgroundWhite");

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    Tray::InitTheme();

    // Enable modern Per-Monitor V2 DPI awareness
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        typedef BOOL (WINAPI *pfnSetDpiAwareV2)(DPI_AWARENESS_CONTEXT);
        pfnSetDpiAwareV2 fnSetDpiAwareV2 = (pfnSetDpiAwareV2)GetProcAddress(hUser, "SetProcessDpiAwarenessContext");
        if (fnSetDpiAwareV2) {
            fnSetDpiAwareV2(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"RapooTrayMessageWnd";
    RegisterClassExW(&wc);

    g_hMainWnd = CreateWindowExW(
        0, wc.lpszClassName, L"RapooTray",
        WS_POPUP, 0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );

    g_uTaskbarRestartMsg = RegisterWindowMessageW(L"TaskbarCreated");

    // Start background HID Device Manager
    Device::Start(g_hMainWnd, OnDeviceStateChanged);

    // Initialize Flyout Notification, WinUI Settings, Low-Battery Alert, and Taskbar Bar Windows
    Flyout::Init(hInstance);
    Settings::Init(hInstance, g_hMainWnd);
    Alert::Init(hInstance);
    TaskbarBar::Init(hInstance, g_hMainWnd);

    // Register PnP Device notifications
    DEV_BROADCAST_DEVICEINTERFACE_W dbFilter = {0};
    dbFilter.dbcc_size = sizeof(dbFilter);
    dbFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    HidD_GetHidGuid(&dbFilter.dbcc_classguid);
    HDEVNOTIFY hDevNotify = RegisterDeviceNotificationW(g_hMainWnd, &dbFilter, DEVICE_NOTIFY_WINDOW_HANDLE | 0x00000004);

    // Register Notify Icon with sharpest project icon at system notification resolution
    int smIcon = GetSystemMetrics(SM_CXSMICON);
    if (smIcon <= 0) smIcon = 16;
    HICON hAppIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, smIcon, smIcon, LR_DEFAULTCOLOR);
    if (!hAppIcon) hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hMainWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAYMSG;
    g_nid.hIcon = hAppIcon;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"外设电量监控 (正在连接...)");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Run message pump
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Clean exit
    if (hDevNotify) {
        UnregisterDeviceNotification(hDevNotify);
        hDevNotify = NULL;
    }
    Device::Stop();
    TaskbarBar::Cleanup();
    Settings::Cleanup();
    Flyout::Cleanup();
    Alert::Cleanup();

    CloseHandle(hMutex);
    return 0;
}
