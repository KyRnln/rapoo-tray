#include "flyout_window.h"
#include "tray_menu.h"
#include "webview2_host.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <algorithm>

namespace Flyout {

static const WCHAR* FLYOUT_CLASS = L"RapooNativeToastFlyoutWnd";

// Viewport width in logical pixels (96 DPI)
static const int VIEWPORT_W = 356;
static const int VIEWPORT_H_DEFAULT = 118;

static HWND g_hWnd = NULL;
static HWND g_hOwnerWnd = NULL;
static HHOOK g_hMouseHook = NULL;
static WebView2::Host* g_wvHost = nullptr;

static int g_dpi = 96;
static int g_currentLogicalH = VIEWPORT_H_DEFAULT;
static bool g_isShowing = false;
static bool g_isClosing = false;

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

static std::wstring GetSystemAccentHex(bool isDark) {
    // Read Windows 10/11 8-shade AccentPalette (same source used by Twinkle Tray & WinUI/UWP)
    // Index 0..6: AccentLight3, AccentLight2, AccentLight1, Accent (base), AccentDark1, AccentDark2, AccentDark3
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        BYTE palette[32] = {0};
        DWORD size = sizeof(palette), type = 0;
        if (RegQueryValueExW(hKey, L"AccentPalette", NULL, &type, palette, &size) == ERROR_SUCCESS && size >= 28) {
            RegCloseKey(hKey);
            // Dark mode uses AccentLight1 (index 2) for high contrast on dark surfaces;
            // Light mode uses Accent (index 3) for accurate theme color on light surfaces.
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

// Compute ideal logical viewport height based on device list (full-width device dividers only)
static int ComputeIdealLogicalHeight(const std::vector<Device::State>& devs) {
    if (devs.empty()) return 84;

    int totalH = 2; // Top + bottom 1px card border
    for (size_t i = 0; i < devs.size(); ++i) {
        bool hasBat = devs[i].hasBattery;
        bool hasMetrics = devs[i].hasDpi || devs[i].hasPolling;
        if (!hasBat && !hasMetrics) {
            totalH += 56; // 16px top + 24px row + 16px bottom
        } else if (hasBat && hasMetrics) {
            totalH += 104; // 16px top + 24px row + 12px + 4px bar + 12px + 20px metrics + 16px bottom
        } else if (hasBat) {
            totalH += 72; // 16px top + 24px row + 12px + 4px bar + 16px bottom
        } else {
            totalH += 88; // 16px top + 24px row + 12px + 20px metrics + 16px bottom
        }
        if (i + 1 < devs.size()) {
            totalH += 1; // 1px full-width device divider
        }
    }
    return std::min(460, std::max(56, totalH));
}

static void UpdateFlyoutWindowGeometry(int logicalH) {
    g_currentLogicalH = logicalH;
    if (!g_hWnd || !IsWindowVisible(g_hWnd)) return;

    HMONITOR hMon = MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);

    g_dpi = GetWindowDpi(g_hWnd);
    int w = S(VIEWPORT_W);
    int h = S(g_currentLogicalH);
    int targetX = mi.rcWork.right - w - S(12);
    int targetY = mi.rcWork.bottom - h - S(12);

    RECT rcCur = {0};
    GetWindowRect(g_hWnd, &rcCur);
    if ((rcCur.bottom - rcCur.top) != h || (rcCur.right - rcCur.left) != w) {
        SetWindowPos(g_hWnd, HWND_TOPMOST, targetX, targetY, w, h, SWP_NOACTIVATE);
        if (g_wvHost) {
            g_wvHost->Resize();
            g_wvHost->NotifyPositionChanged();
        }
    }
}

static void PushStateToWebView() {
    if (!g_wvHost || !g_wvHost->IsReady()) return;

    std::vector<Device::State> devs = Device::GetAllDevices();
    int idealH = ComputeIdealLogicalHeight(devs);
    if (idealH != g_currentLogicalH) {
        UpdateFlyoutWindowGeometry(idealH);
    }

    bool isDark = IsSystemDarkTheme();
    std::wstring accent = GetSystemAccentHex(isDark);

    std::wstring json = L"window.updateDeviceState && window.updateDeviceState({";
    json += L"isDark:";
    json += isDark ? L"true" : L"false";
    json += L",accent:\"" + accent + L"\",devices:[";

    for (size_t i = 0; i < devs.size(); ++i) {
        const auto& st = devs[i];
        if (i > 0) json += L",";

        WCHAR itemBuf[1024] = {0};
        StringCchPrintfW(
            itemBuf, ARRAYSIZE(itemBuf),
            L"{\"id\":\"%s\",\"modelName\":\"%s\",\"brandName\":\"%s\",\"category\":\"%s\","
            L"\"connType\":\"%s\",\"isConnected\":%s,\"isWired\":%s,\"isCharging\":%s,"
            L"\"hasBattery\":%s,\"battery\":%d,\"hasDpi\":%s,\"dpiLevel\":%d,\"dpiX\":%d,\"dpiY\":%d,"
            L"\"hasPolling\":%s,\"pollingHz\":%d,\"isPinned\":%s}",
            EscapeJsString(st.id).c_str(),
            EscapeJsString(st.modelName[0] ? st.modelName : L"无线外设").c_str(),
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

static void InstallMouseHook() {
    if (!g_hMouseHook) {
        g_hMouseHook = SetWindowsHookExW(
            WH_MOUSE_LL,
            [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
                if (nCode >= 0 && g_isShowing && !g_isClosing && g_hWnd && IsWindowVisible(g_hWnd)) {
                    if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                        wParam == WM_NCLBUTTONDOWN || wParam == WM_NCRBUTTONDOWN ||
                        wParam == WM_MBUTTONDOWN) {
                        MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;
                        RECT rc = {0};
                        GetWindowRect(g_hWnd, &rc);
                        if (!PtInRect(&rc, p->pt)) {
                            Hide();
                        }
                    }
                }
                return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
            },
            GetModuleHandleW(NULL),
            0
        );
    }
}

static void RemoveMouseHook() {
    if (g_hMouseHook) {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = NULL;
    }
}

static void ConfigureTransparentContainer(HWND hWnd) {
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hWnd, &margins);

    DWORD corner = 1; // DWMWCP_DONOTROUND
    DwmSetWindowAttribute(hWnd, 33, &corner, sizeof(corner));
    COLORREF noBorder = 0xFFFFFFFE; // DWMWA_COLOR_NONE
    DwmSetWindowAttribute(hWnd, 34, &noBorder, sizeof(noBorder));
    DWORD backdrop = 1; // DWMSBT_NONE
    DwmSetWindowAttribute(hWnd, 38, &backdrop, sizeof(backdrop));
}

static std::wstring GetNotificationHtml() {
    return LR"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  :root {
    --bg-card: #242424;
    --border-card: rgba(255, 255, 255, 0.22);
    --divider: rgba(255, 255, 255, 0.16);
    --text-primary: #FFFFFF;
    --text-secondary: rgba(255, 255, 255, 0.72);
    --text-muted: rgba(255, 255, 255, 0.48);
    --track-bg: rgba(255, 255, 255, 0.12);
    --accent: #60CDFF;
  }

  body.light {
    --bg-card: #F9F9FB;
    --border-card: #D4D4D8;
    --divider: #DFDFE3;
    --text-primary: #1A1A1A;
    --text-secondary: rgba(0, 0, 0, 0.65);
    --text-muted: rgba(0, 0, 0, 0.46);
    --track-bg: rgba(0, 0, 0, 0.10);
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
    overflow: hidden;
    font-family: "Segoe UI Variable Text", "Segoe UI", "Microsoft YaHei UI", sans-serif;
    -webkit-font-smoothing: antialiased;
  }

  /* Pure Windows 11 Notification Card: No glow/shadow, crisp 1px light-gray line border, zero inner padding so device dividers span full width */
  .toast-card {
    width: 100%;
    height: 100%;
    background: var(--bg-card);
    border: 1px solid var(--border-card);
    border-radius: 8px;
    padding: 0;
    overflow: hidden;
    display: flex;
    flex-direction: column;
    transform: translate3d(calc(100% + 16px), 0, 0);
    opacity: 0;
    will-change: transform, opacity;
  }

  .toast-card.enter {
    transform: translate3d(0, 0, 0);
    opacity: 1;
    transition: transform 0.34s cubic-bezier(0.16, 1, 0.3, 1), opacity 0.2s ease-out;
  }

  .toast-card.exit {
    transform: translate3d(calc(100% + 16px), 0, 0);
    opacity: 0;
    transition: transform 0.24s cubic-bezier(0.4, 0, 1, 1), opacity 0.16s ease-in;
  }

  .fluent-icon {
    font-family: "Segoe Fluent Icons", "Segoe MDL2 Assets";
    font-style: normal;
    font-weight: normal;
    line-height: 1;
  }

  /* Scrollable Multi-Device Container: Dividers ONLY between devices, spanning full card width */
  .devices-scroll {
    flex: 1;
    width: 100%;
    min-height: 0;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
  }

  .devices-scroll::-webkit-scrollbar {
    width: 4px;
  }

  .devices-scroll::-webkit-scrollbar-track {
    background: transparent;
  }

  .devices-scroll::-webkit-scrollbar-thumb {
    background: var(--divider);
    border-radius: 2px;
  }

  .device-item {
    width: 100%;
    padding: 16px 18px;
    display: flex;
    flex-direction: column;
    gap: 12px;
  }

  .device-item + .device-item {
    border-top: 1px solid var(--divider);
  }

  /* Device Row: Pure Windows Fluent Icon + Name + Battery Percentage */
  .hero-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 8px;
    height: 24px;
  }

  .device-title-wrap {
    display: flex;
    align-items: center;
    gap: 9px;
    min-width: 0;
    flex: 1;
  }

  .device-cat-icon {
    font-size: 16px;
    color: var(--text-secondary);
    flex-shrink: 0;
  }

  .model-name {
    font-family: "Segoe UI Variable Display", "Segoe UI", "Microsoft YaHei UI", sans-serif;
    font-size: 15px;
    font-weight: 600;
    color: var(--text-primary);
    letter-spacing: -0.01em;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .conn-sub {
    font-size: 11.5px;
    color: var(--text-muted);
    flex-shrink: 0;
  }

  .battery-hero {
    display: flex;
    align-items: center;
    gap: 6px;
    flex-shrink: 0;
  }

  .charging-icon {
    font-size: 12px;
    color: #22C55E;
  }

  .battery-pct {
    font-family: "Segoe UI Variable Display", "Segoe UI", "Microsoft YaHei UI", sans-serif;
    font-size: 17px;
    font-weight: 600;
    color: var(--text-primary);
    font-variant-numeric: tabular-nums;
  }

  /* Windows 11 Fluent Progress Bar */
  .progress-track {
    width: 100%;
    height: 4px;
    border-radius: 2px;
    background: var(--track-bg);
    overflow: hidden;
  }

  .progress-fill {
    height: 100%;
    width: 100%;
    border-radius: 2px;
    background: var(--accent);
    transition: width 0.35s cubic-bezier(0.16, 1, 0.3, 1), background-color 0.25s ease;
  }

  /* Clean Telemetry Row: DPI left-aligned, Polling Rate right-aligned */
  .metrics-line-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    height: 20px;
  }

  .metric-col {
    display: inline-flex;
    align-items: center;
    gap: 6px;
  }

  .metric-col:first-child {
    justify-content: flex-start;
    text-align: left;
  }

  .metric-col:last-child {
    justify-content: flex-end;
    text-align: right;
  }

  .metric-label {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    font-size: 11.5px;
    color: var(--text-muted);
  }

  .metric-icon {
    font-size: 12.5px;
    color: var(--text-secondary);
  }

  .metric-value {
    font-size: 12.5px;
    font-weight: 600;
    color: var(--text-primary);
    font-variant-numeric: tabular-nums;
    white-space: nowrap;
  }
</style>
</head>
<body>
  <div id="card" class="toast-card">
    <div id="devicesList" class="devices-scroll"></div>
  </div>

<script>
  const card = document.getElementById('card');
  const devicesList = document.getElementById('devicesList');

  let dismissTimer = null;
  let exitTimeout = null;
  let isHovered = false;

  function postMsg(msg) {
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage(msg);
    }
  }

  function resetDismissTimer() {
    if (dismissTimer) clearTimeout(dismissTimer);
    if (!isHovered) {
      dismissTimer = setTimeout(() => {
        window.slideOut();
      }, 5000);
    }
  }

  card.addEventListener('mouseenter', () => {
    isHovered = true;
    if (dismissTimer) clearTimeout(dismissTimer);
  });

  card.addEventListener('mouseleave', () => {
    isHovered = false;
    resetDismissTimer();
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

  window.updateDeviceState = function(payload) {
    if (!payload) return;
    document.body.classList.toggle('light', !payload.isDark);
    if (payload.accent) {
      document.documentElement.style.setProperty('--accent', payload.accent);
      document.body.style.setProperty('--accent', payload.accent);
    }

    const devs = Array.isArray(payload.devices) ? payload.devices : [];
    let html = '';

    devs.forEach(st => {
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

      const pctText = st.isConnected
        ? (st.battery + '%')
        : (st.battery ? (st.battery + '%') : '--');

      const connLabel = st.isConnected
        ? (st.connType || (st.isWired ? 'USB 有线' : '2.4G 无线'))
        : '离线';

      let metricsHtml = '';
      if (st.hasDpi || st.hasPolling) {
        const dpiText = (st.isConnected && st.hasDpi && st.dpiX > 0)
          ? ((st.dpiX === st.dpiY ? (st.dpiX + ' DPI') : (st.dpiX + '/' + st.dpiY)) + (st.dpiLevel > 0 ? ` · 第 ${st.dpiLevel} 档` : ''))
          : '--';
        const pollText = (st.isConnected && st.hasPolling && st.pollingHz > 0)
          ? (st.pollingHz + ' Hz')
          : '--';

        metricsHtml = `
          <div class="metrics-line-row">
            <div class="metric-col">
              <span class="metric-label"><span class="fluent-icon metric-icon">&#xEC4A;</span><span>DPI</span></span>
              <span class="metric-value">${escapeHtml(dpiText)}</span>
            </div>
            <div class="metric-col">
              <span class="metric-label"><span class="fluent-icon metric-icon">&#xE8CB;</span><span>回报率</span></span>
              <span class="metric-value">${escapeHtml(pollText)}</span>
            </div>
          </div>`;
      }

      const rightSideHtml = hasBat ? `
        <div class="battery-hero">
          ${(st.isConnected && st.isCharging) ? '<span class="fluent-icon charging-icon" title="充电中">&#xE945;</span>' : ''}
          <span class="battery-pct">${escapeHtml(pctText)}</span>
        </div>` : `<span class="conn-sub">${escapeHtml(connLabel)}</span>`;

      const progressTrackHtml = hasBat ? `
        <div class="progress-track">
          <div class="progress-fill" style="width:${pct}%;background-color:${barColor}"></div>
        </div>` : '';

      html += `
        <div class="device-item">
          <div class="hero-row">
            <div class="device-title-wrap">
              <span class="fluent-icon device-cat-icon">${categoryGlyph(st.category)}</span>
              <div class="model-name" title="${escapeHtml(st.modelName)}">${escapeHtml(st.modelName || '无线外设')}</div>
              ${hasBat ? `<span class="conn-sub">${escapeHtml(connLabel)}</span>` : ''}
            </div>
            ${rightSideHtml}
          </div>
          ${progressTrackHtml}
          ${metricsHtml}
        </div>`;
    });

    devicesList.innerHTML = html;
  };

  window.slideIn = function() {
    if (exitTimeout) {
      clearTimeout(exitTimeout);
      exitTimeout = null;
    }
    card.classList.remove('exit');
    if (!card.classList.contains('enter')) {
      void card.offsetWidth;
      card.classList.add('enter');
    }
    resetDismissTimer();
  };

  window.slideOut = function() {
    if (dismissTimer) {
      clearTimeout(dismissTimer);
      dismissTimer = null;
    }
    card.classList.remove('enter');
    card.classList.add('exit');
    if (exitTimeout) clearTimeout(exitTimeout);
    exitTimeout = setTimeout(() => {
      postMsg('hidden');
    }, 250);
  };
</script>
</body>
</html>)HTML";
}

static LRESULT CALLBACK FlyoutWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return 1;
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

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE && g_isShowing && !g_isClosing) {
                Hide();
            }
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

bool Init(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = FlyoutWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = FLYOUT_CLASS;
    RegisterClassExW(&wc);

    g_dpi = GetWindowDpi(NULL);
    int w = S(VIEWPORT_W);
    int h = S(VIEWPORT_H_DEFAULT);

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        FLYOUT_CLASS,
        L"",
        WS_POPUP,
        0, 0, w, h,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) return false;

    ConfigureTransparentContainer(g_hWnd);

    g_wvHost = new WebView2::Host();
    g_wvHost->Initialize(
        g_hWnd,
        true /* transparentBg */,
        GetNotificationHtml(),
        [](const std::wstring& msg) {
            if (msg == L"hidden") {
                g_isShowing = false;
                g_isClosing = false;
                RemoveMouseHook();
                if (g_hWnd) {
                    ShowWindow(g_hWnd, SW_HIDE);
                }
            }
        },
        []() {
            PushStateToWebView();
            if (g_isShowing && !g_isClosing && g_wvHost) {
                g_wvHost->ExecuteScript(L"window.slideIn && window.slideIn();");
            }
        }
    );

    return true;
}

void Cleanup() {
    RemoveMouseHook();
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

void Show(HWND hOwnerWnd) {
    if (!g_hWnd) return;
    g_hOwnerWnd = hOwnerWnd;

    Tray::DismissAllMenus();
    Device::RefreshPollingRate();

    std::vector<Device::State> devs = Device::GetAllDevices();
    g_currentLogicalH = ComputeIdealLogicalHeight(devs);

    POINT pt;
    GetCursorPos(&pt);
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);

    g_dpi = GetWindowDpi(g_hWnd);
    int w = S(VIEWPORT_W);
    int h = S(g_currentLogicalH);

    int targetX = mi.rcWork.right - w - S(12);
    int targetY = mi.rcWork.bottom - h - S(12);

    g_isShowing = true;
    g_isClosing = false;

    SetWindowPos(g_hWnd, HWND_TOPMOST, targetX, targetY, w, h, SWP_SHOWWINDOW);
    if (g_wvHost) {
        g_wvHost->Resize();
        g_wvHost->NotifyPositionChanged();
    }
    SetForegroundWindow(g_hWnd);
    InstallMouseHook();

    PushStateToWebView();
    if (g_wvHost && g_wvHost->IsReady()) {
        g_wvHost->ExecuteScript(L"window.slideIn && window.slideIn();");
    }
}

void Hide() {
    if (!g_hWnd || !g_isShowing || g_isClosing) return;
    g_isClosing = true;
    RemoveMouseHook();

    if (g_wvHost && g_wvHost->IsReady()) {
        g_wvHost->ExecuteScript(L"window.slideOut && window.slideOut();");
    } else {
        g_isShowing = false;
        g_isClosing = false;
        ShowWindow(g_hWnd, SW_HIDE);
    }
}

void Toggle(HWND hOwnerWnd) {
    if (IsVisible()) {
        Hide();
    } else {
        Show(hOwnerWnd);
    }
}

bool IsVisible() {
    return (g_hWnd != NULL && IsWindowVisible(g_hWnd) && g_isShowing && !g_isClosing);
}

void Invalidate() {
    if (g_hWnd && IsWindowVisible(g_hWnd)) {
        PushStateToWebView();
    }
}

} // namespace Flyout
