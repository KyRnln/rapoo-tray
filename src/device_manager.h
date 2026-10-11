#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include "rapoo_protocol.h"

namespace Device {

struct State {
    WCHAR id[128] = {0};              // Persistent unique hardware/endpoint ID
    WCHAR modelName[64] = {0};        // Effective display name (custom name if set, otherwise default)
    WCHAR defaultModelName[64] = {0}; // Hardware / model default display name
    WCHAR brandName[32] = {0};        // Brand label (e.g. "雷柏 Rapoo", "Logitech", "Bluetooth")
    WCHAR category[16] = {0};         // "mouse", "keyboard", "headset", "gamepad", "device"
    WCHAR connType[16] = {0};         // "2.4G 无线", "USB 有线", "蓝牙 BLE", "无线连接"
    bool isConnected = false;
    bool isWired = false;
    bool isCharging = false;
    bool hasBattery = true;
    int battery = 100;
    bool hasDpi = false;
    int dpiLevel = 1;
    int dpiX = 800;
    int dpiY = 800;
    bool hasPolling = false;
    int pollingHz = 1000;
    bool isPinned = false;            // True if this device is at the top slot (primary tray device)
};

typedef void (*StateCallback)(const State& primaryState, DWORD changeMask);

constexpr DWORD CHANGE_CONNECTED = 0x01;
constexpr DWORD CHANGE_BATTERY   = 0x02;
constexpr DWORD CHANGE_DPI       = 0x04;
constexpr DWORD CHANGE_POLLING   = 0x08;
constexpr DWORD CHANGE_LIST      = 0x20;

bool Start(HWND hNotifyWnd, StateCallback callback);
void Stop();

void NotifyDeviceChange();

bool RefreshPollingRate();
void ForceRefresh();

// Returns the primary device state (the top battery-capable device in sorted order)
State GetCurrentState();

// Returns all discovered devices in user-sorted order
std::vector<State> GetAllDevices();

// Get or set the pinned/top device ID for backwards compatibility
std::wstring GetPinnedDeviceId();
void SetPinnedDeviceId(const WCHAR* deviceId);

// Set custom device sort order (first battery-capable device becomes primary tray battery display)
void SetDeviceOrder(const std::vector<std::wstring>& orderedIds);

// Set or clear a custom display name for a device (empty string restores default hardware name)
void SetDeviceCustomName(const WCHAR* deviceId, const WCHAR* customName);

} // namespace Device
