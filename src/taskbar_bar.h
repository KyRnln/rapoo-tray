#pragma once
#include <windows.h>
#include "device_manager.h"

namespace TaskbarBar {

// Initialize the taskbar bar component (GDI+, window class, creation)
bool Init(HINSTANCE hInstance, HWND hMainWnd);

// Cleanup window and GDI+ resources
void Cleanup();

// Recreate the taskbar bar window (used after Explorer restart or if watchdog detects destroyed window)
void Recreate();

// Heartbeat watchdog called periodically from core message loop
void CheckAndEnsureVisible();

// Update display with latest device telemetry
void UpdateState(const Device::State& state);

// Enable or disable the taskbar bar
void SetEnabled(bool enabled);
bool IsEnabled();

// Docking position: 0 = Left (Start / Widgets side), 1 = Center (blank area), 2 = Right (Tray side)
enum Position {
    POS_LEFT = 0,
    POS_CENTER = 1,
    POS_RIGHT = 2
};
void SetPosition(int pos);
int GetPosition();

// Bar width adjustment (in 96-DPI base pixels: 100 ~ 280, default 140)
void SetWidth(int width);
int GetWidth();

// Vertical spacing between name/percentage and progress bar (0 ~ 8, default 2)
void SetBarSpacing(int spacing);
int GetBarSpacing();

// Device Category Icon size adjustment (12 ~ 28, default 18)
void SetIconSize(int size);
int GetIconSize();

// Device Category Icon vertical offset (-8 ~ 8, default 0)
void SetIconOffsetY(int offset);
int GetIconOffsetY();

// Taskbar bar text font size (9 ~ 16, default 11)
void SetFontSize(int size);
int GetFontSize();

// Device Category Icon color mode (0 = Theme auto [default], 1 = Black, 2 = Accent)
enum IconColorMode {
    ICON_COLOR_THEME = 0,
    ICON_COLOR_BLACK = 1,
    ICON_COLOR_ACCENT = 2
};
void SetIconColorMode(int mode);
int GetIconColorMode();

// Battery Progress Bar color mode (0 = Theme auto [default], 1 = Black, 2 = Accent)
enum BarColorMode {
    BAR_COLOR_THEME = 0,
    BAR_COLOR_BLACK = 1,
    BAR_COLOR_ACCENT = 2
};
void SetBarColorMode(int mode);
int GetBarColorMode();

// Device Category Icon alignment (0 = Left [default], 1 = Right)
enum IconAlign {
    ICON_ALIGN_LEFT = 0,
    ICON_ALIGN_RIGHT = 1
};
void SetIconAlign(int align);
int GetIconAlign();

// Get current taskbar bar window bounding box in screen pixels
bool GetBarRect(RECT* pRect);

// Show or hide the window
void Show();
void Hide();

// Invalidate and force repaint
void Invalidate();

// Get HWND of the taskbar bar window
HWND GetWindowHandle();

typedef void (*BarClickCallback)();
typedef void (*BarRightClickCallback)(int x, int y);
void SetClickCallbacks(BarClickCallback onClick, BarRightClickCallback onRightClick);

} // namespace TaskbarBar
