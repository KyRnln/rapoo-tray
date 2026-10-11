#pragma once
#include <windows.h>
#include "device_manager.h"

namespace Flyout {

bool Init(HINSTANCE hInstance);
void Cleanup();

// Show or toggle the Twinkle Tray style WinUI 3 Flyout panel
void Show(HWND hOwnerWnd);
void Hide();
void Toggle(HWND hOwnerWnd);
bool IsVisible();

// Notify flyout to repaint when device state or theme changes
void Invalidate();

} // namespace Flyout
