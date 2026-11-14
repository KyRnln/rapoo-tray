#define RAPOO_CORE_EXPORTS
#include "rapoo_core.h"
#include "device_manager.h"
#include "taskbar_bar.h"
#include <windows.h>
#include <shellapi.h>
#include <dbt.h>
#include <strsafe.h>
#include <vector>
#include <string>

extern "C" {
#include <hidsdi.h>
}

static const UINT WM_APP_TRAYMSG = WM_APP + 1;
static const UINT WM_APP_STATE_UPDATE = WM_APP + 2;

static HWND g_hCoreWnd = NULL;
static NOTIFYICONDATAW g_nid = {0};
static UINT g_uTaskbarRestartMsg = 0;
static HICON g_hAppIcon = NULL;
static HDEVNOTIFY g_hDevNotify = NULL;

static CoreStateChangeCallback g_pfnStateChange = NULL;
static CoreBarClickCallback g_pfnBarClick = NULL;
static CoreTrayRightClickCallback g_pfnTrayRightClick = NULL;

static void CopyStateToExport(const Device::State& in, DeviceStateExport* out) {
    if (!out) return;
    memset(out, 0, sizeof(DeviceStateExport));
    StringCchCopyW(out->id, ARRAYSIZE(out->id), in.id);
    StringCchCopyW(out->modelName, ARRAYSIZE(out->modelName), in.modelName);
    StringCchCopyW(out->defaultModelName, ARRAYSIZE(out->defaultModelName), in.defaultModelName);
    StringCchCopyW(out->brandName, ARRAYSIZE(out->brandName), in.brandName);
    StringCchCopyW(out->category, ARRAYSIZE(out->category), in.category);
    StringCchCopyW(out->connType, ARRAYSIZE(out->connType), in.connType);
    out->isConnected = in.isConnected;
    out->isWired = in.isWired;
    out->isCharging = in.isCharging;
    out->hasBattery = in.hasBattery;
    out->battery = in.battery;
    out->hasDpi = in.hasDpi;
    out->dpiLevel = in.dpiLevel;
    out->dpiX = in.dpiX;
    out->dpiY = in.dpiY;
    out->hasPolling = in.hasPolling;
    out->pollingHz = in.pollingHz;
    out->isPinned = in.isPinned;
}

static void UpdateTrayTooltipInternal(const Device::State& state) {
    if (!g_hCoreWnd) return;
    WCHAR tip[128] = {0};
    if (!state.isConnected) {
        StringCchPrintfW(tip, ARRAYSIZE(tip), L"Rapoo 外设电量监控\n状态: 设备未连接");
    } else {
        StringCchPrintfW(tip, ARRAYSIZE(tip), L"%s (%s)\n电量: %d%%%s",
            state.modelName[0] ? state.modelName : L"无线外设",
            state.connType[0] ? state.connType : L"连接",
            state.battery,
            state.isCharging ? L" ⚡充电中" : L"");
    }
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), tip);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void OnDeviceStateChanged(const Device::State& state, DWORD changeMask) {
    if (g_hCoreWnd) {
        PostMessageW(g_hCoreWnd, WM_APP_STATE_UPDATE, 0, 0);
    }
}

static void OnInternalBarClick() {
    if (g_pfnBarClick) {
        g_pfnBarClick();
    }
}

static void OnInternalBarRightClick(int x, int y) {
    if (g_pfnTrayRightClick) {
        g_pfnTrayRightClick(x, y);
    }
}

static LRESULT CALLBACK CoreWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == g_uTaskbarRestartMsg) {
        Shell_NotifyIconW(NIM_ADD, &g_nid);
        Device::State st = Device::GetCurrentState();
        UpdateTrayTooltipInternal(st);
        TaskbarBar::Invalidate();
        return 0;
    }

    switch (msg) {
        case WM_APP_TRAYMSG: {
            if (lParam == WM_LBUTTONUP) {
                // 点击图标不再弹出电量面板 (Clicking tray icon doesn't pop up panel)
            } else if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                if (g_pfnTrayRightClick) {
                    g_pfnTrayRightClick(pt.x, pt.y);
                }
            }
            return 0;
        }

        case WM_APP_STATE_UPDATE: {
            Device::State st = Device::GetCurrentState();
            UpdateTrayTooltipInternal(st);
            TaskbarBar::UpdateState(st);
            if (g_pfnStateChange) {
                g_pfnStateChange();
            }
            return 0;
        }

        case WM_DWMCOLORIZATIONCOLORCHANGED:
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED: {
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
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

RAPOO_API bool RapooCore_Init() {
    if (g_hCoreWnd) return true;

    HINSTANCE hInst = GetModuleHandleW(NULL);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = CoreWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"RapooCoreMsgWnd";
    RegisterClassExW(&wc);

    g_hCoreWnd = CreateWindowExW(
        0, wc.lpszClassName, L"RapooCore",
        WS_POPUP, 0, 0, 0, 0,
        NULL, NULL, hInst, NULL
    );

    if (!g_hCoreWnd) return false;

    g_uTaskbarRestartMsg = RegisterWindowMessageW(L"TaskbarCreated");

    // Register PnP Device notifications
    DEV_BROADCAST_DEVICEINTERFACE_W dbFilter = {0};
    dbFilter.dbcc_size = sizeof(dbFilter);
    dbFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    HidD_GetHidGuid(&dbFilter.dbcc_classguid);
    g_hDevNotify = RegisterDeviceNotificationW(g_hCoreWnd, &dbFilter, DEVICE_NOTIFY_WINDOW_HANDLE | 0x00000004);

    // Initialize System Tray Icon
    int smIcon = GetSystemMetrics(SM_CXSMICON);
    if (smIcon <= 0) smIcon = 16;
    g_hAppIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, smIcon, smIcon, LR_DEFAULTCOLOR);
    if (!g_hAppIcon) {
        g_hAppIcon = (HICON)LoadImageW(NULL, L"res\\app.ico", IMAGE_ICON, smIcon, smIcon, LR_LOADFROMFILE);
    }
    if (!g_hAppIcon) {
        g_hAppIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hCoreWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAYMSG;
    g_nid.hIcon = g_hAppIcon;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"Rapoo 外设电量监控");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Set Taskbar Bar Click Callbacks
    TaskbarBar::SetClickCallbacks(OnInternalBarClick, OnInternalBarRightClick);

    // Start HID background device manager
    Device::Start(g_hCoreWnd, OnDeviceStateChanged);

    // Initialize Taskbar resident bar
    TaskbarBar::Init(hInst, g_hCoreWnd);

    return true;
}

RAPOO_API void RapooCore_Cleanup() {
    if (g_hDevNotify) {
        UnregisterDeviceNotification(g_hDevNotify);
        g_hDevNotify = NULL;
    }
    Device::Stop();
    TaskbarBar::Cleanup();
    if (g_hCoreWnd) {
        DestroyWindow(g_hCoreWnd);
        g_hCoreWnd = NULL;
    }
    if (g_hAppIcon) {
        DestroyIcon(g_hAppIcon);
        g_hAppIcon = NULL;
    }
}

RAPOO_API void RapooCore_SetCallbacks(
    CoreStateChangeCallback onStateChange,
    CoreBarClickCallback onBarClick,
    CoreTrayRightClickCallback onTrayRightClick
) {
    g_pfnStateChange = onStateChange;
    g_pfnBarClick = onBarClick;
    g_pfnTrayRightClick = onTrayRightClick;
}

RAPOO_API int RapooCore_GetDeviceCount() {
    return (int)Device::GetAllDevices().size();
}

RAPOO_API bool RapooCore_GetDevice(int index, DeviceStateExport* outState) {
    std::vector<Device::State> devs = Device::GetAllDevices();
    if (index < 0 || index >= (int)devs.size()) return false;
    CopyStateToExport(devs[index], outState);
    return true;
}

RAPOO_API bool RapooCore_GetPrimaryDevice(DeviceStateExport* outState) {
    Device::State st = Device::GetCurrentState();
    CopyStateToExport(st, outState);
    return true;
}

RAPOO_API void RapooCore_Refresh() {
    Device::ForceRefresh();
    Device::RefreshPollingRate();
}

RAPOO_API void RapooCore_SetPinned(const wchar_t* deviceId) {
    Device::SetPinnedDeviceId(deviceId);
}

RAPOO_API void RapooCore_SetCustomName(const wchar_t* deviceId, const wchar_t* customName) {
    Device::SetDeviceCustomName(deviceId, customName);
}

RAPOO_API bool RapooCore_GetBarRect(RECT* outRect) {
    return TaskbarBar::GetBarRect(outRect);
}

RAPOO_API void RapooCore_UpdateBar() {
    TaskbarBar::Invalidate();
}

RAPOO_API void RapooCore_SetBarEnabled(bool enabled) {
    TaskbarBar::SetEnabled(enabled);
}

RAPOO_API bool RapooCore_IsBarEnabled() {
    return TaskbarBar::IsEnabled();
}

RAPOO_API void RapooCore_SetBarPosition(int pos) {
    TaskbarBar::SetPosition(pos);
}

RAPOO_API int RapooCore_GetBarPosition() {
    return TaskbarBar::GetPosition();
}

RAPOO_API void RapooCore_SetBarWidth(int width) {
    TaskbarBar::SetWidth(width);
}

RAPOO_API int RapooCore_GetBarWidth() {
    return TaskbarBar::GetWidth();
}

RAPOO_API void RapooCore_SetBarSpacing(int spacing) {
    TaskbarBar::SetBarSpacing(spacing);
}

RAPOO_API int RapooCore_GetBarSpacing() {
    return TaskbarBar::GetBarSpacing();
}

RAPOO_API void RapooCore_SetBarIconSize(int size) {
    TaskbarBar::SetIconSize(size);
}

RAPOO_API int RapooCore_GetBarIconSize() {
    return TaskbarBar::GetIconSize();
}

RAPOO_API void RapooCore_SetBarIconOffsetY(int offset) {
    TaskbarBar::SetIconOffsetY(offset);
}

RAPOO_API int RapooCore_GetBarIconOffsetY() {
    return TaskbarBar::GetIconOffsetY();
}

RAPOO_API void RapooCore_SetBarFontSize(int size) {
    TaskbarBar::SetFontSize(size);
}

RAPOO_API int RapooCore_GetBarFontSize() {
    return TaskbarBar::GetFontSize();
}

RAPOO_API void RapooCore_SetBarIconColorMode(int mode) {
    TaskbarBar::SetIconColorMode(mode);
}

RAPOO_API int RapooCore_GetBarIconColorMode() {
    return TaskbarBar::GetIconColorMode();
}

RAPOO_API void RapooCore_SetBarColorMode(int mode) {
    TaskbarBar::SetBarColorMode(mode);
}

RAPOO_API int RapooCore_GetBarColorMode() {
    return TaskbarBar::GetBarColorMode();
}

RAPOO_API void RapooCore_UpdateTrayTooltip(const wchar_t* tip) {
    if (!g_hCoreWnd || !tip) return;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), tip);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
