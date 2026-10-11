#include "settings_window.h"
#include "tray_menu.h"
#include "flyout_window.h"
#include "webview2_host.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <strsafe.h>
#include <algorithm>
#include <string>
#include <vector>

namespace Settings {

static const WCHAR* SETTINGS_CLASS = L"RapooWinUISettingsWnd";
static const int WIN_W = 800;
static const int WIN_H = 540;
static const int MIN_W = 640;
static const int MIN_H = 420;

static HINSTANCE g_hInst = NULL;
static HWND g_hMainAppWnd = NULL;
static HWND g_hWnd = NULL;
static WebView2::Host* g_wvHost = nullptr;
static int g_dpi = 96;

static inline int S(int v) {
    return MulDiv(v, g_dpi, 96);
}

static int GetWindowDpi(HWND hWnd) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        typedef UINT(WINAPI* pfnGetDpiForWindow)(HWND);
        auto fn = (pfnGetDpiForWindow)GetProcAddress(hUser, "GetDpiForWindow");
        if (fn && hWnd) {
            UINT d = fn(hWnd);
            if (d > 0) return (int)d;
        }
    }
    HDC hdc = GetDC(NULL);
    int d = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    return (d > 0) ? d : 96;
}

static bool IsSystemDarkTheme() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 1, size = sizeof(DWORD), type = 0;
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val == 0);
        }
        RegCloseKey(hKey);
    }
    return true;
}

static std::wstring GetSystemAccentHex(bool isDark) {
    // Read Windows 10/11 8-shade AccentPalette (same source used by Twinkle Tray & WinUI/UWP)
    // Index 0..6: AccentLight3, AccentLight2, AccentLight1, Accent (base), AccentDark1, AccentDark2, AccentDark3
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        BYTE palette[32] = {0};
        DWORD size = sizeof(palette), type = 0;
        if (RegQueryValueExW(hKey, L"AccentPalette", NULL, &type, palette, &size) == ERROR_SUCCESS && size >= 28) {
            RegCloseKey(hKey);
            int idx = isDark ? 2 : 3;
            BYTE r = palette[idx * 4 + 0];
            BYTE g = palette[idx * 4 + 1];
            BYTE b = palette[idx * 4 + 2];
            WCHAR buf[16] = {0};
            StringCchPrintfW(buf, ARRAYSIZE(buf), L"#%02X%02X%02X", r, g, b);
            return buf;
        }
        RegCloseKey(hKey);
    }

    DWORD argb = 0;
    BOOL opaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&argb, &opaque))) {
        BYTE r = (argb >> 16) & 0xFF;
        BYTE g = (argb >> 8) & 0xFF;
        BYTE b = argb & 0xFF;
        if (isDark) {
            r = (BYTE)std::min(255, (int)r + 42);
            g = (BYTE)std::min(255, (int)g + 42);
            b = (BYTE)std::min(255, (int)b + 42);
        }
        WCHAR buf[16] = {0};
        StringCchPrintfW(buf, ARRAYSIZE(buf), L"#%02X%02X%02X", r, g, b);
        return buf;
    }
    return isDark ? L"#60CDFF" : L"#0067C0";
}

static std::wstring EscapeJsString(const WCHAR* str) {
    if (!str) return L"";
    std::wstring out;
    for (const WCHAR* p = str; *p; ++p) {
        if (*p == L'\\') out += L"\\\\";
        else if (*p == L'\"') out += L"\\\"";
        else if (*p == L'\n') out += L"\\n";
        else if (*p == L'\r') continue;
        else out += *p;
    }
    return out;
}

static void ApplyWin11MicaFrame(HWND hWnd) {
    bool isDark = IsSystemDarkTheme();
    BOOL darkVal = isDark ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, 20, &darkVal, sizeof(darkVal)); // DWMWA_USE_IMMERSIVE_DARK_MODE

    DWORD corner = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hWnd, 33, &corner, sizeof(corner));

    // Hide titlebar caption text and icon
    WTA_OPTIONS wta = { WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON, WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON };
    SetWindowThemeAttribute(hWnd, WTA_NONCLIENT, &wta, sizeof(wta));

    // Extend DWM frame across client area and enable Windows 11 Mica (DWMSBT_MAINWINDOW = 2)
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hWnd, &margins);

    DWORD backdrop = 2; // DWMSBT_MAINWINDOW (Mica)
    DwmSetWindowAttribute(hWnd, 38, &backdrop, sizeof(backdrop));
}

static void PushStateToWebView() {
    if (!g_wvHost || !g_wvHost->IsReady()) return;

    std::vector<Device::State> devs = Device::GetAllDevices();
    bool isDark = IsSystemDarkTheme();
    bool autoRun = Tray::IsAutoRunEnabled();
    std::wstring accent = GetSystemAccentHex(isDark);
    std::wstring rawPinned = Device::GetPinnedDeviceId();
    std::wstring pinnedId = EscapeJsString(rawPinned.c_str());

    std::wstring json = L"window.updateSettingsState && window.updateSettingsState({";
    json += L"isDark:";
    json += isDark ? L"true" : L"false";
    json += L",autoRun:";
    json += autoRun ? L"true" : L"false";
    json += L",accent:\"" + accent + L"\"";
    json += L",pinnedId:\"" + pinnedId + L"\"";
    json += L",devices:[";

    for (size_t i = 0; i < devs.size(); ++i) {
        const auto& st = devs[i];
        if (i > 0) json += L",";

        WCHAR itemBuf[1280] = {0};
        StringCchPrintfW(
            itemBuf, ARRAYSIZE(itemBuf),
            L"{\"id\":\"%s\",\"modelName\":\"%s\",\"defaultModelName\":\"%s\",\"brandName\":\"%s\",\"category\":\"%s\","
            L"\"connType\":\"%s\",\"isConnected\":%s,\"isWired\":%s,\"isCharging\":%s,"
            L"\"hasBattery\":%s,\"battery\":%d,\"hasDpi\":%s,\"dpiLevel\":%d,\"dpiX\":%d,\"dpiY\":%d,"
            L"\"hasPolling\":%s,\"pollingHz\":%d,\"isPinned\":%s}",
            EscapeJsString(st.id).c_str(),
            EscapeJsString(st.modelName[0] ? st.modelName : L"无线外设").c_str(),
            EscapeJsString(st.defaultModelName[0] ? st.defaultModelName : (st.modelName[0] ? st.modelName : L"无线外设")).c_str(),
            EscapeJsString(st.brandName[0] ? st.brandName : L"外设").c_str(),
            EscapeJsString(st.category[0] ? st.category : L"device").c_str(),
            EscapeJsString(st.connType[0] ? st.connType : L"无线").c_str(),
            st.isConnected ? L"true" : L"false",
            st.isWired ? L"true" : L"false",
            st.isCharging ? L"true" : L"false",
            st.hasBattery ? L"true" : L"false",
            st.battery,
            st.hasDpi ? L"true" : L"false",
            st.dpiLevel,
            st.dpiX,
            st.dpiY,
            st.hasPolling ? L"true" : L"false",
            st.pollingHz,
            st.isPinned ? L"true" : L"false"
        );
        json += itemBuf;
    }

    json += L"]});";
    g_wvHost->ExecuteScript(json);
}

static std::wstring GetSettingsHtml() {
    return LR"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  :root {
    --bg-fallback: #202020;
    --card-bg: rgba(255, 255, 255, 0.052);
    --card-sub-bg: rgba(255, 255, 255, 0.032);
    --card-hover: rgba(255, 255, 255, 0.075);
    --card-border: rgba(255, 255, 255, 0.075);
    --card-border-top: rgba(255, 255, 255, 0.11);
    --top-slot-bg: rgba(255, 255, 255, 0.138);
    --top-slot-border: rgba(255, 255, 255, 0.20);
    --nav-hover: rgba(255, 255, 255, 0.06);
    --nav-active: rgba(255, 255, 255, 0.085);
    --text-primary: #FFFFFF;
    --text-secondary: rgba(255, 255, 255, 0.72);
    --text-muted: rgba(255, 255, 255, 0.50);
    --track-bg: rgba(255, 255, 255, 0.14);
    --btn-bg: rgba(255, 255, 255, 0.06);
    --btn-hover: rgba(255, 255, 255, 0.10);
    --btn-active: rgba(255, 255, 255, 0.04);
    --btn-border: rgba(255, 255, 255, 0.09);
    --input-bg: rgba(0, 0, 0, 0.28);
    --switch-off-bg: rgba(255, 255, 255, 0.05);
    --switch-off-border: rgba(255, 255, 255, 0.58);
    --switch-off-thumb: rgba(255, 255, 255, 0.78);
    --switch-on-thumb: #000000;
    --accent: #60CDFF;
  }

  body.light {
    --bg-fallback: #F3F3F3;
    --card-bg: rgba(255, 255, 255, 0.76);
    --card-sub-bg: rgba(255, 255, 255, 0.52);
    --card-hover: rgba(255, 255, 255, 0.90);
    --card-border: rgba(0, 0, 0, 0.065);
    --card-border-top: rgba(255, 255, 255, 0.90);
    --top-slot-bg: #E2E2E7;
    --top-slot-border: rgba(0, 0, 0, 0.13);
    --nav-hover: rgba(0, 0, 0, 0.04);
    --nav-active: rgba(0, 0, 0, 0.065);
    --text-primary: #1A1A1A;
    --text-secondary: rgba(0, 0, 0, 0.65);
    --text-muted: rgba(0, 0, 0, 0.46);
    --track-bg: rgba(0, 0, 0, 0.12);
    --btn-bg: rgba(255, 255, 255, 0.78);
    --btn-hover: rgba(255, 255, 255, 0.95);
    --btn-active: rgba(0, 0, 0, 0.04);
    --btn-border: rgba(0, 0, 0, 0.10);
    --input-bg: rgba(255, 255, 255, 0.92);
    --switch-off-bg: rgba(0, 0, 0, 0.02);
    --switch-off-border: rgba(0, 0, 0, 0.55);
    --switch-off-thumb: rgba(0, 0, 0, 0.68);
    --switch-on-thumb: #FFFFFF;
  }

  * {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
    user-select: none;
    -webkit-user-select: none;
  }

  html, body {
    width: 100%;
    height: 100%;
    background: transparent !important;
    color: var(--text-primary);
    font-family: "Segoe UI Variable Text", "Segoe UI", "Microsoft YaHei UI", sans-serif;
    overflow: hidden;
    -webkit-font-smoothing: antialiased;
  }

  .fluent-icon {
    font-family: "Segoe Fluent Icons", "Segoe MDL2 Assets";
    font-style: normal;
    font-weight: normal;
    line-height: 1;
  }

  /* WinUI 3 / Twinkle Tray Native Loading ProgressRing Overlay */
  .app-loader {
    position: fixed;
    inset: 0;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 14px;
    z-index: 200;
    pointer-events: none;
    opacity: 0;
    transition: opacity 0.2s cubic-bezier(0.16, 1, 0.3, 1);
  }

  .app-loader.active {
    opacity: 1;
  }

  .fluent-ring {
    width: 38px;
    height: 38px;
    animation: ringRotate 1.8s linear infinite;
  }

  .fluent-ring circle {
    fill: none;
    stroke: var(--accent);
    stroke-width: 3.2;
    stroke-linecap: round;
    stroke-dasharray: 1, 150;
    stroke-dashoffset: 0;
    animation: ringArc 1.4s ease-in-out infinite;
  }

  @keyframes ringRotate {
    100% { transform: rotate(360deg); }
  }

  @keyframes ringArc {
    0% { stroke-dasharray: 1, 150; stroke-dashoffset: 0; }
    50% { stroke-dasharray: 68, 150; stroke-dashoffset: -22; }
    100% { stroke-dasharray: 68, 150; stroke-dashoffset: -90; }
  }

  /* WinUI 3 NavigationView Layout */
  .app-shell {
    display: flex;
    width: 100%;
    height: 100%;
    opacity: 1;
    transform: translateY(0);
    transition: opacity 0.28s cubic-bezier(0.16, 1, 0.3, 1),
                transform 0.38s cubic-bezier(0.16, 1, 0.3, 1);
  }

  .app-shell.pre-enter {
    opacity: 0;
    transform: translateY(18px);
    transition: none;
  }

  .app-shell.entering .page.active .page-header-row {
    animation: cardCascadeIn 0.36s cubic-bezier(0.16, 1, 0.3, 1) both;
  }

  .app-shell.entering .page.active .device-group,
  .app-shell.entering .page.active .settings-card {
    animation: cardCascadeIn 0.42s cubic-bezier(0.16, 1, 0.3, 1) both;
    animation-delay: calc(var(--card-idx, 0) * 45ms + 30ms);
  }

  @keyframes cardCascadeIn {
    from {
      opacity: 0;
      transform: translate3d(0, 22px, 0);
    }
    to {
      opacity: 1;
      transform: translate3d(0, 0, 0);
    }
  }

  @keyframes spinOnce {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
  }

  #refreshBtn.spinning .fluent-icon {
    display: inline-block;
    animation: spinOnce 0.5s cubic-bezier(0.16, 1, 0.3, 1);
  }

  .nav-pane {
    width: 204px;
    padding: 20px 10px 16px 12px;
    display: flex;
    flex-direction: column;
    gap: 4px;
    flex-shrink: 0;
  }

  .nav-item {
    position: relative;
    height: 36px;
    padding: 0 12px;
    border-radius: 6px;
    display: flex;
    align-items: center;
    gap: 12px;
    cursor: pointer;
    color: var(--text-secondary);
    font-size: 13.5px;
    transition: background 0.12s ease, color 0.12s ease;
  }

  .nav-item:hover {
    background: var(--nav-hover);
    color: var(--text-primary);
  }

  .nav-item.active {
    background: var(--nav-active);
    color: var(--text-primary);
    font-weight: 600;
  }

  .nav-item::before {
    content: "";
    position: absolute;
    left: 0;
    top: 10px;
    bottom: 10px;
    width: 3px;
    border-radius: 2px;
    background: var(--accent);
    transform: scaleY(0);
    transition: transform 0.2s cubic-bezier(0.16, 1, 0.3, 1);
  }

  .nav-item.active::before {
    transform: scaleY(1);
  }

  .nav-icon {
    font-size: 16px;
  }

  /* Content Pane */
  .content-pane {
    flex: 1;
    padding: 24px 32px 28px 20px;
    overflow-y: auto;
  }

  .content-pane::-webkit-scrollbar {
    width: 6px;
  }

  .content-pane::-webkit-scrollbar-thumb {
    background: var(--card-border);
    border-radius: 3px;
  }

  .page {
    display: none;
    max-width: 920px;
    animation: pageEnter 0.28s cubic-bezier(0.16, 1, 0.3, 1);
  }

  .page.active {
    display: block;
  }

  @keyframes pageEnter {
    from {
      opacity: 0;
      transform: translateY(14px);
    }
    to {
      opacity: 1;
      transform: translateY(0);
    }
  }

  .page-header-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 18px;
    gap: 12px;
  }

  .page-title {
    font-family: "Segoe UI Variable Display", "Segoe UI", "Microsoft YaHei UI", sans-serif;
    font-size: 24px;
    font-weight: 600;
    letter-spacing: -0.01em;
  }

  /* WinUI 3 SettingsCard */
  .settings-card {
    background: var(--card-bg);
    border: 1px solid var(--card-border);
    border-top-color: var(--card-border-top);
    border-radius: 8px;
    padding: 15px 18px;
    margin-bottom: 8px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 16px;
  }

  .card-left {
    display: flex;
    align-items: center;
    gap: 15px;
    min-width: 0;
    flex: 1;
  }

  /* Pure Windows-style Icon (no background box, no border) */
  .card-icon {
    font-size: 20px;
    color: var(--text-secondary);
    width: 24px;
    text-align: center;
    flex-shrink: 0;
  }

  .card-titles {
    display: flex;
    flex-direction: column;
    gap: 2px;
    min-width: 0;
  }

  .card-title {
    font-size: 14px;
    font-weight: 500;
    color: var(--text-primary);
  }

  .device-card-title {
    font-size: 14.5px;
    font-weight: 600;
    color: var(--text-primary);
  }

  .card-desc {
    font-size: 12px;
    color: var(--text-muted);
  }

  .card-right {
    display: flex;
    align-items: center;
    gap: 12px;
    flex-shrink: 0;
  }

  .value-badge {
    font-size: 13.5px;
    font-weight: 600;
    color: var(--text-primary);
    font-variant-numeric: tabular-nums;
  }

  /* WinUI 3 Button */
  .winui-btn {
    height: 32px;
    padding: 0 12px;
    border-radius: 6px;
    border: 1px solid var(--btn-border);
    background: var(--btn-bg);
    color: var(--text-primary);
    font-family: inherit;
    font-size: 12.5px;
    display: inline-flex;
    align-items: center;
    gap: 6px;
    cursor: pointer;
    transition: background 0.12s ease, transform 0.08s ease, border-color 0.12s ease;
  }

  .winui-btn:hover:not(:disabled) {
    background: var(--btn-hover);
  }

  .winui-btn:active:not(:disabled) {
    background: var(--btn-active);
    transform: scale(0.98);
  }

  .winui-btn:disabled {
    opacity: 0.35;
    cursor: default;
  }

  .icon-btn {
    width: 28px;
    height: 28px;
    padding: 0;
    justify-content: center;
  }

  /* Drag Handle (Right-side position) */
  .drag-handle {
    width: 28px;
    height: 28px;
    border-radius: 6px;
    font-size: 15px;
    color: var(--text-muted);
    cursor: grab;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    transition: background 0.12s ease, color 0.12s ease;
    flex-shrink: 0;
    touch-action: none;
  }

  .drag-handle:hover {
    background: var(--nav-hover);
    color: var(--text-primary);
  }

  .drag-handle:active {
    cursor: grabbing;
    background: var(--nav-active);
  }

  /* Multi-Device Group Card */
  .device-group {
    position: relative;
    margin-bottom: 8px;
    border-radius: 8px;
    overflow: hidden;
    border: 1px solid var(--card-border);
    border-top-color: var(--card-border-top);
    background: var(--card-bg);
    will-change: transform;
    transition: background 0.2s ease, border-color 0.2s ease, box-shadow 0.2s ease;
  }

  /* Top Device Slot: Separate Light-Gray Card Slot Background */
  .device-group.top-slot-card {
    background: var(--top-slot-bg);
    border-color: var(--top-slot-border);
    border-top-color: var(--top-slot-border);
    box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.08);
  }

  /* Active dragged card follows pointer immediately while elevating with shadow */
  .device-group.dragging {
    z-index: 30;
    border-color: var(--accent);
    box-shadow: 0 14px 32px rgba(0, 0, 0, 0.30);
    transition: background 0.2s ease, border-color 0.15s ease, box-shadow 0.18s ease;
  }

  /* Sibling cards smoothly slide up/down when crossed */
  .device-group.sibling-animating {
    z-index: 1;
    transition: transform 0.26s cubic-bezier(0.2, 0, 0, 1), background 0.2s ease, border-color 0.2s ease;
  }

  /* Dragged card smoothly glides into its final slot upon release */
  .device-group.settling {
    z-index: 30;
    transition: transform 0.22s cubic-bezier(0.16, 1, 0.3, 1), box-shadow 0.2s ease, background 0.2s ease, border-color 0.2s ease;
  }

  .device-group-header {
    padding: 15px 18px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 16px;
  }

  .title-inline-row {
    display: flex;
    align-items: center;
    gap: 6px;
    min-width: 0;
  }

  .rename-trigger-btn {
    border: none;
    background: transparent;
    color: var(--text-muted);
    width: 22px;
    height: 22px;
    border-radius: 4px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    font-size: 12px;
    opacity: 0.65;
    transition: background 0.12s ease, color 0.12s ease, opacity 0.12s ease;
    flex-shrink: 0;
  }

  .device-group:hover .rename-trigger-btn {
    opacity: 1;
  }

  .rename-trigger-btn:hover {
    background: var(--nav-hover);
    color: var(--text-primary);
  }

  .rename-edit-wrap {
    display: inline-flex;
    align-items: center;
    gap: 6px;
  }

  .rename-input {
    height: 26px;
    width: 190px;
    padding: 0 8px;
    border-radius: 5px;
    border: 1px solid var(--accent);
    background: var(--input-bg);
    color: var(--text-primary);
    font-family: inherit;
    font-size: 13.5px;
    font-weight: 500;
    outline: none;
    user-select: text;
    -webkit-user-select: text;
  }

  .device-metrics-list {
    display: flex;
    align-items: center;
    border-top: 1px solid var(--card-border);
  }

  .device-group.top-slot-card .device-metrics-list {
    border-top-color: var(--top-slot-border);
  }

  .device-metric-cell {
    flex: 1;
    padding: 11px 18px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }

  .device-metric-cell + .device-metric-cell {
    border-left: 1px solid var(--card-border);
  }

  .device-group.top-slot-card .device-metric-cell + .device-metric-cell {
    border-left-color: var(--top-slot-border);
  }

  .metric-cell-label {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    font-size: 12.5px;
    color: var(--text-secondary);
  }

  .metric-cell-icon {
    font-size: 14px;
    color: var(--text-secondary);
  }

  /* Mini Battery Bar inside Device Card */
  .bat-bar-wrap {
    display: flex;
    align-items: center;
    gap: 10px;
  }

  .bat-track {
    width: 76px;
    height: 5px;
    border-radius: 3px;
    background: var(--track-bg);
    overflow: hidden;
  }

  .bat-fill {
    height: 100%;
    width: 100%;
    border-radius: 3px;
    background: var(--accent);
    transition: width 0.35s cubic-bezier(0.16, 1, 0.3, 1);
  }

  /* Authentic WinUI 3 ToggleSwitch */
  .toggle-wrap {
    display: flex;
    align-items: center;
    gap: 10px;
    cursor: pointer;
  }

  .toggle-label {
    font-size: 13px;
    color: var(--text-secondary);
    min-width: 28px;
    text-align: right;
  }

  .winui-switch {
    width: 40px;
    height: 20px;
    border-radius: 10px;
    background: var(--switch-off-bg);
    border: 1px solid var(--switch-off-border);
    position: relative;
    transition: background 0.16s ease, border-color 0.16s ease;
  }

  .winui-switch-thumb {
    width: 12px;
    height: 12px;
    border-radius: 6px;
    background: var(--switch-off-thumb);
    position: absolute;
    top: 3px;
    left: 3px;
    transition: transform 0.2s cubic-bezier(0.16, 1, 0.3, 1), width 0.12s ease, background 0.16s ease;
  }

  .toggle-wrap:active .winui-switch-thumb {
    width: 15px;
  }

  .winui-switch.on {
    background: var(--accent);
    border-color: var(--accent);
  }

  .winui-switch.on .winui-switch-thumb {
    background: var(--switch-on-thumb);
    transform: translateX(20px);
  }

  .toggle-wrap:active .winui-switch.on .winui-switch-thumb {
    transform: translateX(17px);
  }
</style>
</head>
<body>
  <div id="appLoader" class="app-loader active">
    <svg class="fluent-ring" viewBox="0 0 36 36">
      <circle cx="18" cy="18" r="14"></circle>
    </svg>
  </div>

  <div id="appShell" class="app-shell pre-enter">
    <!-- Left NavigationView Pane -->
    <aside class="nav-pane">
      <div class="nav-item active" data-page="page-device">
        <span class="fluent-icon nav-icon">&#xE962;</span>
        <span>设备</span>
      </div>
      <div class="nav-item" data-page="page-general">
        <span class="fluent-icon nav-icon">&#xE713;</span>
        <span>设置</span>
      </div>
    </aside>

    <!-- Right Content Area -->
    <main class="content-pane">
      <!-- Page 1: Multi-Device Read-Only Telemetry -->
      <section id="page-device" class="page active">
        <div class="page-header-row">
          <h1 class="page-title">设备</h1>
          <button id="refreshBtn" class="winui-btn">
            <span class="fluent-icon">&#xE72C;</span>
            <span>刷新</span>
          </button>
        </div>

        <div id="devicesContainer"></div>
      </section>

      <!-- Page 2: General & About -->
      <section id="page-general" class="page">
        <div class="page-header-row">
          <h1 class="page-title">设置</h1>
        </div>

        <div class="settings-card" style="--card-idx:0">
          <div class="card-left">
            <span class="fluent-icon card-icon">&#xE768;</span>
            <div class="card-titles">
              <div class="card-title">开机自启动</div>
              <div class="card-desc">登录 Windows 时自动在系统托盘后台运行</div>
            </div>
          </div>
          <div class="card-right">
            <div id="autoRunToggle" class="toggle-wrap">
              <span id="autoRunText" class="toggle-label">关</span>
              <div id="autoRunSwitch" class="winui-switch">
                <div class="winui-switch-thumb"></div>
              </div>
            </div>
          </div>
        </div>

        <div class="settings-card" style="--card-idx:1">
          <div class="card-left">
            <span class="fluent-icon card-icon">&#xE946;</span>
            <div class="card-titles">
              <div class="card-title">关于</div>
              <div class="card-desc">只读硬件信息监控 · Windows 11 Fluent UI</div>
            </div>
          </div>
        </div>
      </section>
    </main>
  </div>

<script>
  let currentDevices = [];
  let editingDeviceId = null;
  let draggedIdx = null;
  let cascadeTimer = null;
  let openAnimTimer = null;

  const appLoader = document.getElementById('appLoader');
  const appShell = document.getElementById('appShell');
  const refreshBtn = document.getElementById('refreshBtn');

  function postMsg(msg) {
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage(msg);
    }
  }

  function triggerPageCascade() {
    if (!appShell) return;
    if (cascadeTimer) clearTimeout(cascadeTimer);
    appShell.classList.remove('entering');
    void appShell.offsetWidth;
    appShell.classList.add('entering');
    cascadeTimer = setTimeout(() => {
      appShell.classList.remove('entering');
      cascadeTimer = null;
    }, 560);
  }

  window.prepareForOpen = function() {
    if (openAnimTimer) {
      clearTimeout(openAnimTimer);
      openAnimTimer = null;
    }
    if (appLoader) appLoader.classList.add('active');
    if (appShell) {
      appShell.classList.remove('entering');
      appShell.classList.add('pre-enter');
    }
  };

  window.playOpenAnimation = function() {
    if (openAnimTimer) clearTimeout(openAnimTimer);
    if (appLoader) appLoader.classList.add('active');
    if (appShell) {
      appShell.classList.remove('entering');
      appShell.classList.add('pre-enter');
    }
    openAnimTimer = setTimeout(() => {
      if (appLoader) appLoader.classList.remove('active');
      if (appShell) {
        void appShell.offsetWidth;
        appShell.classList.remove('pre-enter');
      }
      triggerPageCascade();
      openAnimTimer = null;
    }, 260);
  };

  // Navigation switching
  const navItems = document.querySelectorAll('.nav-item');
  const pages = document.querySelectorAll('.page');
  navItems.forEach(item => {
    item.addEventListener('click', () => {
      if (item.classList.contains('active')) return;
      navItems.forEach(n => n.classList.remove('active'));
      pages.forEach(p => p.classList.remove('active'));
      item.classList.add('active');
      const target = document.getElementById(item.getAttribute('data-page'));
      if (target) target.classList.add('active');
      triggerPageCascade();
    });
  });

  refreshBtn.addEventListener('click', () => {
    refreshBtn.classList.remove('spinning');
    void refreshBtn.offsetWidth;
    refreshBtn.classList.add('spinning');
    setTimeout(() => refreshBtn.classList.remove('spinning'), 520);
    postMsg('refresh');
    triggerPageCascade();
  });

  document.getElementById('autoRunToggle').addEventListener('click', () => {
    postMsg('toggleAutoRun');
  });

  function categoryGlyph(cat) {
    if (cat === 'mouse') return '&#xE962;';
    if (cat === 'keyboard') return '&#xE765;';
    if (cat === 'headset') return '&#xE7F6;';
    if (cat === 'gamepad') return '&#xE7FC;';
    if (cat === 'dock') return '&#xE839;';
    return '&#xE703;';
  }

  function escapeHtml(s) {
    const d = document.createElement('div');
    d.textContent = s || '';
    return d.innerHTML;
  }

  function escapeAttr(s) {
    return (s || '').replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  window.startRename = function(devId) {
    editingDeviceId = devId;
    renderDevices();
    const inp = document.getElementById('renameInputField');
    if (inp) {
      inp.focus();
      inp.select();
    }
  };

  window.commitRename = function(devId) {
    const inp = document.getElementById('renameInputField');
    const newName = inp ? inp.value.trim() : '';
    editingDeviceId = null;
    postMsg('rename:' + devId + '\t' + newName);
  };

  window.cancelRename = function() {
    editingDeviceId = null;
    renderDevices();
  };

  window.handleRenameKey = function(ev, devId) {
    if (ev.key === 'Enter') {
      ev.preventDefault();
      window.commitRename(devId);
    } else if (ev.key === 'Escape') {
      ev.preventDefault();
      window.cancelRename();
    }
  };

  // Animated pointer-based drag reordering
  function bindDragEvents() {
    const allCards = Array.from(document.querySelectorAll('.device-group'));
    allCards.forEach((card, fromIdx) => {
      const handle = card.querySelector('.drag-handle');
      if (!handle) return;

      handle.addEventListener('pointerdown', (e) => {
        if (editingDeviceId !== null || currentDevices.length < 2 || draggedIdx !== null) return;
        e.preventDefault();
        draggedIdx = fromIdx;

        const startY = e.clientY;
        const rects = allCards.map(c => c.getBoundingClientRect());
        let dragStride = rects[fromIdx].height + 8;
        if (fromIdx + 1 < rects.length) {
          dragStride = rects[fromIdx + 1].top - rects[fromIdx].top;
        } else if (fromIdx > 0) {
          dragStride = rects[fromIdx].bottom - rects[fromIdx - 1].bottom;
        }

        let currentTargetIdx = fromIdx;

        card.classList.add('dragging');
        allCards.forEach((c, i) => {
          if (i !== fromIdx) c.classList.add('sibling-animating');
        });

        handle.setPointerCapture(e.pointerId);

        const onMove = (me) => {
          if (draggedIdx === null) return;
          const deltaY = me.clientY - startY;
          card.style.transform = `translate3d(0, ${deltaY}px, 0) scale(1.012)`;

          const draggedCenterY = rects[fromIdx].top + rects[fromIdx].height / 2 + deltaY;
          let newTarget = fromIdx;

          if (deltaY < 0) {
            for (let i = 0; i < fromIdx; i++) {
              const midY = rects[i].top + rects[i].height / 2;
              if (draggedCenterY < midY) {
                newTarget = i;
                break;
              }
            }
          } else if (deltaY > 0) {
            for (let i = allCards.length - 1; i > fromIdx; i--) {
              const midY = rects[i].top + rects[i].height / 2;
              if (draggedCenterY > midY) {
                newTarget = i;
                break;
              }
            }
          }

          currentTargetIdx = newTarget;

          // Smoothly displace crossed sibling cards and live-update top slot highlight
          const topVisualIdx = (currentTargetIdx === 0)
            ? fromIdx
            : (fromIdx === 0 && currentTargetIdx > 0 ? 1 : 0);

          allCards.forEach((c, i) => {
            c.classList.toggle('top-slot-card', i === topVisualIdx);
            if (i === fromIdx) return;

            let shift = 0;
            if (fromIdx < currentTargetIdx && i > fromIdx && i <= currentTargetIdx) {
              shift = -dragStride;
            } else if (fromIdx > currentTargetIdx && i >= currentTargetIdx && i < fromIdx) {
              shift = dragStride;
            }
            c.style.transform = shift !== 0 ? `translate3d(0, ${shift}px, 0)` : 'translate3d(0, 0, 0)';
          });
        };

        const onUp = () => {
          handle.removeEventListener('pointermove', onMove);
          handle.removeEventListener('pointerup', onUp);
          handle.removeEventListener('pointercancel', onUp);

          const toIdx = currentTargetIdx;
          let finalOffsetY = 0;
          if (toIdx < fromIdx) {
            finalOffsetY = rects[toIdx].top - rects[fromIdx].top;
          } else if (toIdx > fromIdx) {
            finalOffsetY = rects[toIdx].bottom - rects[fromIdx].bottom;
          }

          card.classList.remove('dragging');
          card.classList.add('settling');
          card.style.transform = `translate3d(0, ${finalOffsetY}px, 0) scale(1)`;

          setTimeout(() => {
            draggedIdx = null;
            if (toIdx !== fromIdx && toIdx >= 0 && toIdx < currentDevices.length) {
              const moved = currentDevices.splice(fromIdx, 1)[0];
              currentDevices.splice(toIdx, 0, moved);
              const orderedIds = currentDevices.map(d => d.id).join('|');
              renderDevices();
              postMsg('order:' + orderedIds);
            } else {
              renderDevices();
            }
          }, 210);
        };

        handle.addEventListener('pointermove', onMove);
        handle.addEventListener('pointerup', onUp);
        handle.addEventListener('pointercancel', onUp);
      });
    });
  }

  function renderDevices() {
    const container = document.getElementById('devicesContainer');
    let cardsHtml = '';

    currentDevices.forEach((st, idx) => {
      const isTopSlot = (idx === 0);
      const hasBat = (st.hasBattery !== false);
      const pct = st.isConnected
        ? Math.max(4, Math.min(100, st.battery || 0))
        : (st.battery ? Math.max(4, Math.min(100, st.battery)) : 0);

      let barColor = 'var(--accent)';
      if (!st.isConnected) {
        barColor = 'var(--text-muted)';
      } else if (st.isCharging) {
        barColor = '#22C55E';
      } else if (st.battery <= 20) {
        barColor = '#EF4444';
      } else if (st.battery <= 30) {
        barColor = '#F59E0B';
      }

      const batText = st.isConnected
        ? (st.battery + (st.isCharging ? '% · 充电中' : '%'))
        : (st.battery ? (st.battery + '%') : '--');

      const connPart = st.isConnected
        ? ((st.connType || (st.isWired ? 'USB 有线' : '2.4G 无线')) + ' · 已连接')
        : '未连接';
      const subtitleText = isTopSlot ? (connPart + ' · 状态栏电量显示') : connPart;

      const safeIdJs = (st.id || '').replace(/\\/g, '\\\\').replace(/'/g, "\\'");
      const isEditingThis = (editingDeviceId === st.id);

      let titleAreaHtml = '';
      if (isEditingThis) {
        titleAreaHtml = `
          <div class="rename-edit-wrap">
            <input id="renameInputField" class="rename-input" type="text" maxlength="48"
              value="${escapeAttr(st.modelName || '')}"
              placeholder="${escapeAttr(st.defaultModelName || '留空恢复默认名称')}"
              onkeydown="window.handleRenameKey(event, '${safeIdJs}')" />
            <button class="winui-btn icon-btn" title="保存名称（留空恢复默认）" onclick="window.commitRename('${safeIdJs}')">
              <span class="fluent-icon">&#xE8FB;</span>
            </button>
            <button class="winui-btn icon-btn" title="取消" onclick="window.cancelRename()">
              <span class="fluent-icon">&#xE711;</span>
            </button>
          </div>`;
      } else {
        titleAreaHtml = `
          <div class="title-inline-row">
            <span class="device-card-title" ondblclick="window.startRename('${safeIdJs}')">${escapeHtml(st.modelName || '无线外设')}</span>
            <button class="rename-trigger-btn" title="重命名设备" onclick="window.startRename('${safeIdJs}')">
              <span class="fluent-icon">&#xE70F;</span>
            </button>
          </div>`;
      }

      let cellsHtml = '';
      if (st.hasDpi) {
        const dpiVal = (st.isConnected && st.dpiX > 0)
          ? ((st.dpiX === st.dpiY ? (st.dpiX + ' DPI') : (st.dpiX + ' / ' + st.dpiY + ' DPI')) + (st.dpiLevel > 0 ? ` · 第 ${st.dpiLevel} 档` : ''))
          : '--';
        cellsHtml += `
          <div class="device-metric-cell">
            <span class="metric-cell-label"><span class="fluent-icon metric-cell-icon">&#xEC4A;</span><span>DPI 灵敏度</span></span>
            <span class="value-badge">${escapeHtml(dpiVal)}</span>
          </div>`;
      }

      if (st.hasPolling) {
        const intervalMs = (st.isConnected && st.pollingHz > 0)
          ? (1000 / st.pollingHz).toFixed(st.pollingHz >= 1000 ? 2 : 1)
          : '';
        const pollVal = (st.isConnected && st.pollingHz > 0)
          ? `${st.pollingHz} Hz (${intervalMs} ms)`
          : '--';
        cellsHtml += `
          <div class="device-metric-cell">
            <span class="metric-cell-label"><span class="fluent-icon metric-cell-icon">&#xE8CB;</span><span>回报率</span></span>
            <span class="value-badge">${escapeHtml(pollVal)}</span>
          </div>`;
      }

      const batHtml = hasBat ? `
        <div class="bat-bar-wrap">
          <div class="bat-track">
            <div class="bat-fill" style="width:${pct}%;background-color:${barColor}"></div>
          </div>
          <span class="value-badge">${escapeHtml(batText)}</span>
        </div>` : '';

      const dragHandleHtml = currentDevices.length > 1 ? `
        <span class="fluent-icon drag-handle" title="按住上下拖动调整顺序">&#xE784;</span>` : '';

      const metricsGridHtml = cellsHtml ? `<div class="device-metrics-list">${cellsHtml}</div>` : '';
      const headerBorderStyle = cellsHtml ? '' : 'style="border-bottom:none"';

      cardsHtml += `
        <div class="device-group ${isTopSlot ? 'top-slot-card' : ''}" style="--card-idx:${idx}">
          <div class="device-group-header" ${headerBorderStyle}>
            <div class="card-left">
              <span class="fluent-icon card-icon">${categoryGlyph(st.category)}</span>
              <div class="card-titles">
                ${titleAreaHtml}
                <span class="card-desc">${escapeHtml(subtitleText)}</span>
              </div>
            </div>
            <div class="card-right">
              ${batHtml}
              ${dragHandleHtml}
            </div>
          </div>
          ${metricsGridHtml}
        </div>`;
    });

    container.innerHTML = cardsHtml;
    bindDragEvents();
  }

  window.updateSettingsState = function(payload) {
    if (!payload) return;
    document.body.classList.toggle('light', !payload.isDark);
    if (payload.accent) {
      document.documentElement.style.setProperty('--accent', payload.accent);
      document.body.style.setProperty('--accent', payload.accent);
    }

    currentDevices = Array.isArray(payload.devices) ? payload.devices : [];
    if (editingDeviceId === null && draggedIdx === null) {
      renderDevices();
    }

    const sw = document.getElementById('autoRunSwitch');
    const swText = document.getElementById('autoRunText');
    sw.classList.toggle('on', !!payload.autoRun);
    swText.textContent = payload.autoRun ? '开' : '关';
  };
</script>
</body>
</html>)HTML";
}

static LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return 1;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (mmi) {
                mmi->ptMinTrackSize.x = S(MIN_W);
                mmi->ptMinTrackSize.y = S(MIN_H);
            }
            return 0;
        }

        case WM_SIZE: {
            if (g_wvHost) {
                g_wvHost->Resize();
            }
            return 0;
        }

        case WM_MOVE: {
            if (g_wvHost) {
                g_wvHost->NotifyPositionChanged();
            }
            return 0;
        }

        case WM_DPICHANGED: {
            g_dpi = HIWORD(wParam);
            RECT* const prcNewWindow = (RECT*)lParam;
            SetWindowPos(
                hWnd, NULL,
                prcNewWindow->left, prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
            return 0;
        }

        case WM_DWMCOLORIZATIONCOLORCHANGED:
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED: {
            ApplyWin11MicaFrame(hWnd);
            PushStateToWebView();
            return 0;
        }

        case WM_CLOSE: {
            Hide();
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

static void EnsureWindowCreated() {
    if (g_hWnd) return;

    g_dpi = GetWindowDpi(NULL);
    int w = S(WIN_W);
    int h = S(WIN_H);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (scrW - w) / 2;
    int posY = (scrH - h) / 2;

    g_hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        SETTINGS_CLASS,
        L"",
        WS_OVERLAPPEDWINDOW,
        posX, posY, w, h,
        NULL, NULL, g_hInst, NULL
    );

    if (!g_hWnd) return;

    ApplyWin11MicaFrame(g_hWnd);

    g_wvHost = new WebView2::Host();
    g_wvHost->Initialize(
        g_hWnd,
        true /* transparentBg so Windows 11 Mica shines through */,
        GetSettingsHtml(),
        [](const std::wstring& msg) {
            if (msg == L"refresh") {
                Device::ForceRefresh();
                PushStateToWebView();
            } else if (msg == L"toggleAutoRun") {
                Tray::ToggleAutoRun();
                PushStateToWebView();
            } else if (msg.rfind(L"order:", 0) == 0) {
                std::wstring rawOrder = msg.substr(6);
                std::vector<std::wstring> ids;
                size_t start = 0;
                while (start < rawOrder.size()) {
                    size_t pos = rawOrder.find(L'|', start);
                    std::wstring token = (pos == std::wstring::npos) ? rawOrder.substr(start) : rawOrder.substr(start, pos - start);
                    if (!token.empty()) ids.push_back(token);
                    if (pos == std::wstring::npos) break;
                    start = pos + 1;
                }
                Device::SetDeviceOrder(ids);
                PushStateToWebView();
                Flyout::Invalidate();
                if (g_hMainAppWnd) {
                    PostMessageW(g_hMainAppWnd, WM_APP + 2, 0, 0);
                }
            } else if (msg.rfind(L"rename:", 0) == 0) {
                std::wstring payload = msg.substr(7);
                size_t tabPos = payload.find(L'\t');
                if (tabPos != std::wstring::npos) {
                    std::wstring devId = payload.substr(0, tabPos);
                    std::wstring newName = payload.substr(tabPos + 1);
                    Device::SetDeviceCustomName(devId.c_str(), newName.c_str());
                    PushStateToWebView();
                    Flyout::Invalidate();
                    if (g_hMainAppWnd) {
                        PostMessageW(g_hMainAppWnd, WM_APP + 2, 0, 0);
                    }
                }
            }
        },
        []() {
            PushStateToWebView();
            if (g_hWnd && IsWindowVisible(g_hWnd) && g_wvHost) {
                g_wvHost->ExecuteScript(L"window.playOpenAnimation && window.playOpenAnimation();");
            }
        }
    );
}

bool Init(HINSTANCE hInstance, HWND hMainAppWnd) {
    g_hInst = hInstance;
    g_hMainAppWnd = hMainAppWnd;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hIconSm = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = SETTINGS_CLASS;
    RegisterClassExW(&wc);

    // Pre-create hidden window and warm up WebView2 so opening Settings has zero cold-start delay
    EnsureWindowCreated();

    return true;
}

void Cleanup() {
    if (g_wvHost) {
        g_wvHost->Shutdown();
        g_wvHost->Release();
        g_wvHost = nullptr;
    }
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
        g_hWnd = NULL;
    }
}

void Show() {
    Flyout::Hide();
    Tray::DismissAllMenus();
    Device::RefreshPollingRate();

    EnsureWindowCreated();
    if (!g_hWnd) return;

    bool wasVisible = IsWindowVisible(g_hWnd) && !IsIconic(g_hWnd);

    ApplyWin11MicaFrame(g_hWnd);
    PushStateToWebView();

    if (!wasVisible && g_wvHost && g_wvHost->IsReady()) {
        g_wvHost->ExecuteScript(L"window.prepareForOpen && window.prepareForOpen();");
    }

    if (IsIconic(g_hWnd)) {
        ShowWindow(g_hWnd, SW_RESTORE);
    } else {
        ShowWindow(g_hWnd, SW_SHOW);
    }
    SetForegroundWindow(g_hWnd);

    if (!wasVisible && g_wvHost && g_wvHost->IsReady()) {
        g_wvHost->ExecuteScript(L"window.playOpenAnimation && window.playOpenAnimation();");
    }
}

void Hide() {
    if (g_hWnd && IsWindowVisible(g_hWnd)) {
        if (g_wvHost && g_wvHost->IsReady()) {
            g_wvHost->ExecuteScript(L"window.prepareForOpen && window.prepareForOpen();");
        }
        ShowWindow(g_hWnd, SW_HIDE);
    }
}

bool IsVisible() {
    return (g_hWnd != NULL && IsWindowVisible(g_hWnd));
}

void Invalidate() {
    if (IsVisible()) {
        PushStateToWebView();
    }
}

} // namespace Settings
