#pragma once
#include <windows.h>
#include "device_manager.h"

namespace Settings {

bool Init(HINSTANCE hInstance, HWND hMainWnd);
void Cleanup();

// Open and bring the standalone WinUI 3 Settings Window to foreground
void Show();
void Hide();
bool IsVisible();

// Repaint live state when device status or theme changes
void Invalidate();

} // namespace Settings
