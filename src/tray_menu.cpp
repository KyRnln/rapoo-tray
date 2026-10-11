#include "tray_menu.h"
#include "flyout_window.h"
#include "settings_window.h"
#include <dwmapi.h>
#include <strsafe.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>

namespace Tray {

static const WCHAR* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const WCHAR* APP_NAME = L"rapoo-tray";

static HWND g_hParentAppWnd = NULL;
static HWND g_hAcrylicMenu = NULL;
static HHOOK g_hMenuMouseHook = NULL;
static bool g_bModalLoop = false;

static int g_curDpi = 96;
static bool g_curDark = false;
static int g_mainHover = -1;

static inline int S(int val) {
    return MulDiv(val, g_curDpi, 96);
}

// Windows 11 / 10 Acrylic & Theme Hooks
typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_INVALID_STATE = 5
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL (WINAPI *pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
typedef BOOL (WINAPI *pfnShouldAppsUseDarkMode)();
typedef BOOL (WINAPI *pfnAllowDarkModeForWindow)(HWND, BOOL);
typedef void (WINAPI *pfnFlushMenuThemes)();

static pfnSetWindowCompositionAttribute fnSetWindowCompositionAttribute = NULL;
static pfnShouldAppsUseDarkMode fnShouldAppsUseDarkMode = NULL;
static pfnAllowDarkModeForWindow fnAllowDarkModeForWindow = NULL;
static pfnFlushMenuThemes fnFlushMenuThemes = NULL;

void InitTheme() {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        fnSetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
    }
    HMODULE hUx = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUx) {
        fnShouldAppsUseDarkMode = (pfnShouldAppsUseDarkMode)GetProcAddress(hUx, MAKEINTRESOURCEA(132));
        fnAllowDarkModeForWindow = (pfnAllowDarkModeForWindow)GetProcAddress(hUx, MAKEINTRESOURCEA(133));
        fnFlushMenuThemes = (pfnFlushMenuThemes)GetProcAddress(hUx, MAKEINTRESOURCEA(136));
    }
}

static bool IsSystemDarkMode() {
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
    if (fnShouldAppsUseDarkMode) return fnShouldAppsUseDarkMode();
    return false;
}

static void ApplyModernWindowStyle(HWND hWnd, bool isDark, int w = 0, int h = 0) {
    if (fnAllowDarkModeForWindow) {
        fnAllowDarkModeForWindow(hWnd, isDark ? TRUE : FALSE);
    }
    BOOL darkVal = isDark ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, 20, &darkVal, sizeof(darkVal)); // DWMWA_USE_IMMERSIVE_DARK_MODE

    // Windows 11 rounded corners (DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2)
    DWORD corner = 2;
    HRESULT hrCorner = DwmSetWindowAttribute(hWnd, 33, &corner, sizeof(corner));
    COLORREF borderCol = isDark ? RGB(88, 88, 94) : RGB(212, 212, 216);
    DwmSetWindowAttribute(hWnd, 34, &borderCol, sizeof(borderCol));

    if (FAILED(hrCorner)) {
        RECT rc = {0};
        GetWindowRect(hWnd, &rc);
        int width = (w > 0) ? w : (rc.right - rc.left);
        int height = (h > 0) ? h : (rc.bottom - rc.top);
        if (width > 0 && height > 0) {
            HRGN hRgn = CreateRoundRectRgn(0, 0, width + 1, height + 1, S(8), S(8));
            SetWindowRgn(hWnd, hRgn, TRUE);
        }
    }

    if (fnSetWindowCompositionAttribute) {
        ACCENT_POLICY policy = {};
        if (isDark) {
            policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
            policy.AccentFlags = 0;
            policy.GradientColor = 0xD8282828; // Windows 11 Dark Flyout Acrylic
        } else {
            policy.AccentState = ACCENT_DISABLED;
            policy.AccentFlags = 0;
            policy.GradientColor = 0;
        }
        WINDOWCOMPOSITIONATTRIBDATA data = { 19, &policy, sizeof(policy) };
        fnSetWindowCompositionAttribute(hWnd, &data);
    }
}

bool IsAutoRunEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR path[MAX_PATH];
        DWORD len = sizeof(path), type = 0;
        LSTATUS st = RegQueryValueExW(hKey, APP_NAME, NULL, &type, (LPBYTE)path, &len);
        RegCloseKey(hKey);
        return (st == ERROR_SUCCESS);
    }
    return false;
}

void ToggleAutoRun() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (IsAutoRunEnabled()) {
            RegDeleteValueW(hKey, APP_NAME);
        } else {
            WCHAR path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ, (const BYTE*)path, (DWORD)((wcslen(path) + 1) * sizeof(WCHAR)));
        }
        RegCloseKey(hKey);
    }
}

void DismissAllMenus() {
    if (g_hMenuMouseHook) {
        UnhookWindowsHookEx(g_hMenuMouseHook);
        g_hMenuMouseHook = NULL;
    }
    if (g_hAcrylicMenu) {
        HWND h = g_hAcrylicMenu;
        g_hAcrylicMenu = NULL;
        DestroyWindow(h);
    }
    g_bModalLoop = false;
    if (g_hParentAppWnd) {
        PostMessageW(g_hParentAppWnd, WM_NULL, 0, 0);
    }
}

static LRESULT CALLBACK MenuMouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
            wParam == WM_NCLBUTTONDOWN || wParam == WM_NCRBUTTONDOWN ||
            wParam == WM_MBUTTONDOWN) {
            MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;
            POINT pt = p->pt;
            RECT rM = {0};
            if (g_hAcrylicMenu && IsWindow(g_hAcrylicMenu)) GetWindowRect(g_hAcrylicMenu, &rM);
            bool inM = (g_hAcrylicMenu && IsWindow(g_hAcrylicMenu)) && PtInRect(&rM, pt);
            if (!inM) {
                DismissAllMenus();
            }
        }
    }
    return CallNextHookEx(g_hMenuMouseHook, nCode, wParam, lParam);
}

// --------------------------------------------------------------------------
// Windows 11 Native 3-Item Context Menu (设置 / 开机自启动 / 退出程序)
// --------------------------------------------------------------------------

struct MenuItemData {
    int id;
    const WCHAR* iconGlyph;
    const WCHAR* label;
    bool isSeparator;
    bool isChecked;
};

static std::vector<MenuItemData> g_mainItems;

static int GetItemH(int idx) {
    if (idx < 0 || idx >= (int)g_mainItems.size()) return 0;
    if (g_mainItems[idx].isSeparator) return S(9);
    return S(34);
}

static int HitTestMain(int my) {
    int y = S(6);
    for (size_t i = 0; i < g_mainItems.size(); i++) {
        int h = GetItemH((int)i);
        if (my >= y && my < y + h) {
            if (g_mainItems[i].isSeparator) return -1;
            return (int)i;
        }
        y += h;
    }
    return -1;
}

static LRESULT CALLBACK AcrylicMainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                DismissAllMenus();
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE: {
            int my = HIWORD(lParam);
            int newH = HitTestMain(my);
            if (newH != g_mainHover) {
                g_mainHover = newH;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_mainHover != -1) {
                g_mainHover = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            int idx = HitTestMain(HIWORD(lParam));
            if (idx >= 0 && !g_mainItems[idx].isSeparator) {
                int cmd = g_mainItems[idx].id;
                DismissAllMenus();
                if (cmd == 1000) {        // 设置
                    Settings::Show();
                } else if (cmd == 1001) { // 开机自启动
                    ToggleAutoRun();
                    Settings::Invalidate();
                } else if (cmd == 1002) { // 退出程序
                    PostQuitMessage(0);
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBm = (HBITMAP)SelectObject(memDC, hbm);

            // Windows 11 Native Context Menu Palette
            COLORREF bgCol     = g_curDark ? RGB(44, 44, 44)    : RGB(249, 249, 249);
            COLORREF hoverCol  = g_curDark ? RGB(58, 58, 58)    : RGB(237, 237, 237);
            COLORREF textCol   = g_curDark ? RGB(255, 255, 255) : RGB(26, 26, 26);
            COLORREF iconCol   = g_curDark ? RGB(210, 210, 210) : RGB(92, 92, 92);
            COLORREF sepCol    = g_curDark ? RGB(62, 62, 62)    : RGB(229, 229, 229);

            HBRUSH bgBrush = CreateSolidBrush(bgCol);
            FillRect(memDC, &rc, bgBrush);
            DeleteObject(bgBrush);

            HFONT hFontText = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
            HFONT hFontIcon = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");

            SetBkMode(memDC, TRANSPARENT);
            HGDIOBJ oldFont = SelectObject(memDC, hFontText);

            int y = S(6);
            for (size_t i = 0; i < g_mainItems.size(); i++) {
                int h = GetItemH((int)i);
                if (g_mainItems[i].isSeparator) {
                    HPEN pSep = CreatePen(PS_SOLID, 1, sepCol);
                    HGDIOBJ oP = SelectObject(memDC, pSep);
                    MoveToEx(memDC, S(12), y + h / 2, NULL);
                    LineTo(memDC, rc.right - S(12), y + h / 2);
                    SelectObject(memDC, oP);
                    DeleteObject(pSep);
                    y += h;
                } else {
                    RECT rItem = { S(5), y + S(1), rc.right - S(5), y + h - S(1) };
                    if ((int)i == g_mainHover) {
                        HBRUSH hH = CreateSolidBrush(hoverCol);
                        HPEN hP = CreatePen(PS_SOLID, 1, hoverCol);
                        HGDIOBJ oP = SelectObject(memDC, hP);
                        HGDIOBJ oB = SelectObject(memDC, hH);
                        RoundRect(memDC, rItem.left, rItem.top, rItem.right, rItem.bottom, S(6), S(6));
                        SelectObject(memDC, oP);
                        SelectObject(memDC, oB);
                        DeleteObject(hP);
                        DeleteObject(hH);
                    }

                    // Draw leading Segoe Fluent Icon
                    if (g_mainItems[i].iconGlyph && g_mainItems[i].iconGlyph[0]) {
                        SelectObject(memDC, hFontIcon);
                        SetTextColor(memDC, iconCol);
                        RECT rIcon = { S(14), y, S(38), y + h };
                        DrawTextW(memDC, g_mainItems[i].iconGlyph, -1, &rIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }

                    // Draw label
                    SelectObject(memDC, hFontText);
                    SetTextColor(memDC, textCol);
                    RECT rLabel = { S(42), y, rc.right - S(36), y + h };
                    DrawTextW(memDC, g_mainItems[i].label, -1, &rLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                    // Draw trailing CheckMark when checked
                    if (g_mainItems[i].isChecked) {
                        SelectObject(memDC, hFontIcon);
                        SetTextColor(memDC, textCol);
                        RECT rCheck = { rc.right - S(34), y, rc.right - S(10), y + h };
                        DrawTextW(memDC, L"\xE73E", -1, &rCheck, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }

                    y += h;
                }
            }

            SelectObject(memDC, oldFont);
            DeleteObject(hFontText);
            DeleteObject(hFontIcon);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBm);
            DeleteObject(hbm);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void ShowMenu(HWND hWndOwner) {
    if (g_bModalLoop) return;
    Flyout::Hide();
    g_hParentAppWnd = hWndOwner;
    g_curDark = IsSystemDarkMode();

    POINT pt;
    GetCursorPos(&pt);
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);

    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        typedef UINT (WINAPI *pfnGetDpiForWindow)(HWND);
        pfnGetDpiForWindow fnGetDpi = (pfnGetDpiForWindow)GetProcAddress(hUser, "GetDpiForWindow");
        if (fnGetDpi && hWndOwner) g_curDpi = fnGetDpi(hWndOwner);
    }
    if (g_curDpi == 0) g_curDpi = 96;

    // Strictly 3 items as requested: 设置, 开机自启动, 退出程序
    g_mainItems.clear();

    bool autoRun = IsAutoRunEnabled();

    MenuItemData itemSettings = {};
    itemSettings.id = 1000;
    itemSettings.iconGlyph = L"\xE713"; // Segoe Fluent Settings icon
    itemSettings.label = L"设置";
    g_mainItems.push_back(itemSettings);

    MenuItemData itemAuto = {};
    itemAuto.id = 1001;
    itemAuto.iconGlyph = L"\xE768"; // Segoe Fluent Play/Startup icon
    itemAuto.label = L"开机自启动";
    itemAuto.isChecked = autoRun;
    g_mainItems.push_back(itemAuto);

    MenuItemData sep = {};
    sep.isSeparator = true;
    g_mainItems.push_back(sep);

    MenuItemData itemExit = {};
    itemExit.id = 1002;
    itemExit.iconGlyph = L"\xE7E8"; // Segoe Fluent PowerButton icon
    itemExit.label = L"退出程序";
    g_mainItems.push_back(itemExit);

    int totalH = S(12);
    for (size_t i = 0; i < g_mainItems.size(); i++) {
        totalH += GetItemH((int)i);
    }
    int menuW = S(176);

    int posX = pt.x - menuW / 2;
    int posY = pt.y - totalH - S(8);
    if (posX + menuW > mi.rcWork.right - S(8)) posX = mi.rcWork.right - menuW - S(8);
    if (posX < mi.rcWork.left + S(8)) posX = mi.rcWork.left + S(8);
    if (posY < mi.rcWork.top + S(8)) posY = pt.y + S(8);
    if (posY + totalH > mi.rcWork.bottom - S(8)) posY = mi.rcWork.bottom - totalH - S(8);

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWndOwner, GWLP_HINSTANCE);
    static bool s_mainRegistered = false;
    if (!s_mainRegistered) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = AcrylicMainWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"RapooModernMainMenuWnd";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassExW(&wc);
        s_mainRegistered = true;
    }

    g_mainHover = -1;

    g_hAcrylicMenu = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"RapooModernMainMenuWnd",
        L"",
        WS_POPUP,
        posX, posY, menuW, totalH,
        hWndOwner, NULL, hInst, NULL
    );

    ApplyModernWindowStyle(g_hAcrylicMenu, g_curDark, menuW, totalH);
    SetForegroundWindow(g_hAcrylicMenu);
    ShowWindow(g_hAcrylicMenu, SW_SHOW);
    UpdateWindow(g_hAcrylicMenu);

    g_hMenuMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MenuMouseHookProc, hInst, 0);

    g_bModalLoop = true;
    MSG msg;
    while (g_bModalLoop && GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            DismissAllMenus();
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (msg.message == WM_QUIT) {
        PostQuitMessage((int)msg.wParam);
    }
}

// --------------------------------------------------------------------------
// Windows 11 Native System Tray Battery Icon & Tooltip
// --------------------------------------------------------------------------

HICON CreateBatteryIcon(int battery, bool isCharging, bool isConnected, int size, bool isDark) {
    if (size <= 0) size = 16;
    const int SS = 4; // 4x Supersampling for razor-sharp edges at any DPI
    int W = size * SS;
    int H = size * SS;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvPixels = NULL;
    HBITMAP hbmColor = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvPixels, NULL, 0);
    if (!hbmColor || !pvPixels) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    uint32_t* hi = (uint32_t*)pvPixels;
    std::memset(hi, 0, W * H * 4);

    auto HiPixel = [&](int x, int y, uint32_t c) {
        if (x >= 0 && x < W && y >= 0 && y < H) {
            hi[y * W + x] = c;
        }
    };

    auto FillRoundRect = [&](int x0, int y0, int x1, int y1, uint32_t col, int rad) {
        if (rad > (x1 - x0) / 2) rad = (x1 - x0) / 2;
        if (rad > (y1 - y0) / 2) rad = (y1 - y0) / 2;
        if (rad < 0) rad = 0;
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                int dx = (x < x0 + rad) ? (x0 + rad - x) : (x > x1 - rad) ? (x - (x1 - rad)) : 0;
                int dy = (y < y0 + rad) ? (y0 + rad - y) : (y > y1 - rad) ? (y - (y1 - rad)) : 0;
                if (dx * dx + dy * dy <= rad * rad) HiPixel(x, y, col);
            }
        }
    };

    auto L2S = [&](float v) {
        return (int)(v * (float)(size * SS) / 16.0f + 0.5f);
    };

    // Snap all horizontal/vertical straight edges to exact physical pixel boundaries (multiples of SS)
    // so top, bottom, left, and right borders and the 1px inner gap are 100% razor-sharp without subpixel blur,
    // while 4x4 supersampling (SS) smoothly anti-aliases the rounded corners.
    int stroke_px   = (size >= 28) ? 2 : 1;
    int pad_px      = (size >= 28) ? 2 : 1;
    int nub_w_px    = 1;
    int margin_y_px = std::max(2, (size + 4) / 8);

    int bx0_px = 0;
    int bx1_px = size - nub_w_px; // Maximum possible body width (0 .. size-1), with 1px terminal nub at the far right
    int by0_px = margin_y_px;
    int by1_px = size - margin_y_px;

    int bx0 = bx0_px * SS;
    int by0 = by0_px * SS;
    int bx1 = bx1_px * SS - 1;
    int by1 = by1_px * SS - 1;
    int brad = L2S(2.2f);

    // Windows 11 System Tray monochrome foreground + semantic status fill
    uint32_t c_shell = isDark ? 0xFFFFFFFF : 0xFF1A1A1A;
    uint32_t c_fill  = c_shell;
    if (isCharging) {
        c_fill = isDark ? 0xFF4ADE80 : 0xFF16A34A; // Windows 11 Charging Green
    } else if (battery <= 20) {
        c_fill = 0xFFEF4444; // Windows 11 Critical Battery Red
    } else if (battery <= 30) {
        c_fill = 0xFFF59E0B; // Windows 11 Low Battery Amber
    }

    // 1. Outer rounded battery shell (pixel-snapped edges)
    FillRoundRect(bx0, by0, bx1, by1, c_shell, brad);

    // 2. Right positive terminal cap (1px attached nub at the extreme right edge)
    int nub_v_inset_px = std::max(2, (by1_px - by0_px) / 4);
    int cx0 = (bx1_px - stroke_px) * SS;
    int cx1 = size * SS - 1;
    int cy0 = (by0_px + nub_v_inset_px) * SS;
    int cy1 = (by1_px - nub_v_inset_px) * SS - 1;
    FillRoundRect(cx0, cy0, cx1, cy1, c_shell, L2S(0.6f));

    // 3. Hollow out interior to form a razor-sharp 1px Win11 stroke
    int stroke_t = stroke_px * SS;
    int cav_x0 = bx0 + stroke_t;
    int cav_y0 = by0 + stroke_t;
    int cav_x1 = bx1 - stroke_t;
    int cav_y1 = by1 - stroke_t;
    FillRoundRect(cav_x0, cav_y0, cav_x1, cav_y1, 0, std::max(0, brad - stroke_t));

    // 4. Inner battery level bar (exact 1px physical gap from shell border on all 4 sides)
    int pad_t = pad_px * SS;
    int in_x0 = cav_x0 + pad_t;
    int in_y0 = cav_y0 + pad_t;
    int in_x1 = cav_x1 - pad_t;
    int in_y1 = cav_y1 - pad_t;
    int in_w  = in_x1 - in_x0 + 1;
    int in_h  = in_y1 - in_y0 + 1;

    if (isConnected && battery > 0) {
        float ratio = std::clamp(battery / 100.0f, 0.10f, 1.0f);
        int max_fill_px = in_w / SS;
        int fill_px = std::max(1, (int)(max_fill_px * ratio + 0.5f));
        int fill_w = fill_px * SS;
        FillRoundRect(in_x0, in_y0, in_x0 + fill_w - 1, in_y1, c_fill, L2S(0.9f));
    } else if (!isConnected) {
        // Disconnected / Sleeping subtle dash inside battery
        int dw = L2S(4.6f);
        int dh = stroke_t;
        int sx = in_x0 + (in_w - dw) / 2;
        int sy = ((by0_px + by1_px) / 2) * SS;
        uint32_t c_dash = isDark ? 0x99FFFFFF : 0x991A1A1A;
        FillRoundRect(sx, sy, sx + dw - 1, sy + dh - 1, c_dash, dh / 2);
    }

    // 5. Windows 11 Charging Lightning Bolt overlay (DPI-scaled with clean transparent halo cutout)
    if (isConnected && isCharging) {
        static const char* boltPattern[10] = {
            "....##..",
            "...##...",
            "..##....",
            ".######.",
            ".######.",
            "######..",
            "...##...",
            "..##....",
            ".##.....",
            "........"
        };
        int cell = std::max(SS, L2S(1.1f));
        int bw_px = 8 * cell;
        int bh_px = 10 * cell;
        int bsx = bx0 + (bx1 - bx0 + 1 - bw_px) / 2;
        int bsy = by0 + (by1 - by0 + 1 - bh_px) / 2;

        // First pass: carve transparent halo around bolt for crisp Win11 legibility
        int halo = std::max(2, L2S(0.9f));
        for (int r = 0; r < 10; ++r) {
            for (int c = 0; c < 8; ++c) {
                if (boltPattern[r][c] == '#') {
                    for (int dy = -halo; dy < cell + halo; ++dy) {
                        for (int dx = -halo; dx < cell + halo; ++dx) {
                            HiPixel(bsx + c * cell + dx, bsy + r * cell + dy, 0);
                        }
                    }
                }
            }
        }
        // Second pass: draw crisp lightning bolt
        for (int r = 0; r < 10; ++r) {
            for (int c = 0; c < 8; ++c) {
                if (boltPattern[r][c] == '#') {
                    for (int dy = 0; dy < cell; ++dy) {
                        for (int dx = 0; dx < cell; ++dx) {
                            HiPixel(bsx + c * cell + dx, bsy + r * cell + dy, c_shell);
                        }
                    }
                }
            }
        }
    }

    // Downsample SSxSS to size x size with premultiplied box filter
    BITMAPINFO bomi = {0};
    bomi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bomi.bmiHeader.biWidth = size;
    bomi.bmiHeader.biHeight = -size;
    bomi.bmiHeader.biPlanes = 1;
    bomi.bmiHeader.biBitCount = 32;
    bomi.bmiHeader.biCompression = BI_RGB;

    void* pvOut = NULL;
    HBITMAP hbmOut = CreateDIBSection(hdcMem, &bomi, DIB_RGB_COLORS, &pvOut, NULL, 0);
    if (!hbmOut || !pvOut) {
        DeleteObject(hbmColor);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    uint32_t* out = (uint32_t*)pvOut;
    const int SS2 = SS * SS;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            uint32_t sumA = 0, sumR = 0, sumG = 0, sumB = 0;
            for (int sy = 0; sy < SS; ++sy) {
                const uint32_t* row = hi + (y * SS + sy) * W + x * SS;
                for (int sx = 0; sx < SS; ++sx) {
                    uint32_t c = row[sx];
                    uint32_t a = (c >> 24) & 0xFF;
                    if (a > 0) {
                        sumA += a;
                        sumR += (c >> 16) & 0xFF;
                        sumG += (c >>  8) & 0xFF;
                        sumB += (c      ) & 0xFF;
                    }
                }
            }
            uint8_t avgA = (uint8_t)(sumA / SS2);
            if (avgA == 0) {
                out[y * size + x] = 0;
            } else {
                uint8_t avgR = (uint8_t)(sumR / SS2);
                uint8_t avgG = (uint8_t)(sumG / SS2);
                uint8_t avgB = (uint8_t)(sumB / SS2);
                uint8_t prR = (uint8_t)((avgR * avgA) / 255);
                uint8_t prG = (uint8_t)((avgG * avgA) / 255);
                uint8_t prB = (uint8_t)((avgB * avgA) / 255);
                out[y * size + x] = ((uint32_t)avgA << 24) | ((uint32_t)prR << 16) | ((uint32_t)prG << 8) | (uint32_t)prB;
            }
        }
    }

    int maskPitch = ((size + 15) / 16) * 2;
    BYTE maskBits[256] = {0};
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            uint32_t px = out[y * size + x];
            uint8_t a = (px >> 24) & 0xFF;
            if (a < 32) {
                maskBits[y * maskPitch + (x / 8)] |= (1 << (7 - (x % 8)));
            }
        }
    }
    HBITMAP hbmMask = CreateBitmap(size, size, 1, 1, maskBits);

    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmColor = hbmOut;
    ii.hbmMask = hbmMask;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmOut);
    DeleteObject(hbmMask);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return hIcon;
}

void UpdateTooltip(NOTIFYICONDATAW& nid, const Device::State& state) {
    std::vector<Device::State> devs = Device::GetAllDevices();
    int onlineCount = 0;
    for (const auto& d : devs) {
        if (d.isConnected) onlineCount++;
    }

    if (state.isConnected) {
        const WCHAR* connStr = state.connType[0] ? state.connType : (state.isWired ? L"USB" : L"2.4G");
        const WCHAR* pinTag = state.isPinned ? L" [已固定]" : L"";
        WCHAR line1[96] = {0};
        StringCchPrintfW(line1, ARRAYSIZE(line1), L"%s (%s)%s", state.modelName, connStr, pinTag);

        WCHAR line2[64] = {0};
        StringCchPrintfW(
            line2, ARRAYSIZE(line2),
            L"\n电量: %d%%%s",
            state.battery,
            state.isCharging ? L" (充电中)" : L""
        );

        WCHAR line3[64] = {0};
        if (state.hasDpi && state.dpiX > 0) {
            StringCchPrintfW(line3, ARRAYSIZE(line3), L"\nDPI: %d (第 %d 档)", state.dpiX, state.dpiLevel);
        }

        WCHAR line4[64] = {0};
        if (state.hasPolling && state.pollingHz > 0) {
            StringCchPrintfW(line4, ARRAYSIZE(line4), L"\n回报率: %d Hz", state.pollingHz);
        }

        WCHAR line5[64] = {0};
        if (devs.size() > 1) {
            StringCchPrintfW(line5, ARRAYSIZE(line5), L"\n在线设备: %d / %d 台", onlineCount, (int)devs.size());
        }

        StringCchPrintfW(
            nid.szTip,
            ARRAYSIZE(nid.szTip),
            L"%s%s%s%s%s",
            line1, line2, line3, line4, line5
        );
    } else {
        StringCchPrintfW(
            nid.szTip,
            ARRAYSIZE(nid.szTip),
            L"%s\n设备休眠 / 未连接",
            state.modelName[0] ? state.modelName : L"外设电量中心"
        );
    }
}

} // namespace Tray
