#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RAPOO_CORE_EXPORTS
#define RAPOO_API __declspec(dllexport)
#else
#define RAPOO_API __declspec(dllimport)
#endif

#pragma pack(push, 1)
typedef struct {
    wchar_t id[128];
    wchar_t modelName[64];
    wchar_t defaultModelName[64];
    wchar_t brandName[32];
    wchar_t category[16];
    wchar_t connType[16];
    bool isConnected;
    bool isWired;
    bool isCharging;
    bool hasBattery;
    int battery;
    bool hasDpi;
    int dpiLevel;
    int dpiX;
    int dpiY;
    bool hasPolling;
    int pollingHz;
    bool isPinned;
} DeviceStateExport;
#pragma pack(pop)

typedef void (*CoreStateChangeCallback)();
typedef void (*CoreBarClickCallback)();
typedef void (*CoreTrayRightClickCallback)(int x, int y);

RAPOO_API bool RapooCore_Init();
RAPOO_API void RapooCore_Cleanup();

RAPOO_API void RapooCore_SetCallbacks(
    CoreStateChangeCallback onStateChange,
    CoreBarClickCallback onBarClick,
    CoreTrayRightClickCallback onTrayRightClick
);

RAPOO_API int RapooCore_GetDeviceCount();
RAPOO_API bool RapooCore_GetDevice(int index, DeviceStateExport* outState);
RAPOO_API bool RapooCore_GetPrimaryDevice(DeviceStateExport* outState);

RAPOO_API void RapooCore_Refresh();
RAPOO_API void RapooCore_SetPinned(const wchar_t* deviceId);
RAPOO_API void RapooCore_SetCustomName(const wchar_t* deviceId, const wchar_t* customName);

// Taskbar Bar Controls
RAPOO_API bool RapooCore_GetBarRect(RECT* outRect);
RAPOO_API void RapooCore_UpdateBar();
RAPOO_API void RapooCore_SetBarEnabled(bool enabled);
RAPOO_API bool RapooCore_IsBarEnabled();
RAPOO_API void RapooCore_SetBarPosition(int pos);
RAPOO_API int RapooCore_GetBarPosition();
RAPOO_API void RapooCore_SetBarWidth(int width);
RAPOO_API int RapooCore_GetBarWidth();
RAPOO_API void RapooCore_SetBarSpacing(int spacing);
RAPOO_API int RapooCore_GetBarSpacing();
RAPOO_API void RapooCore_SetBarIconSize(int size);
RAPOO_API int RapooCore_GetBarIconSize();
RAPOO_API void RapooCore_SetBarIconOffsetY(int offset);
RAPOO_API int RapooCore_GetBarIconOffsetY();
RAPOO_API void RapooCore_SetBarFontSize(int size);
RAPOO_API int RapooCore_GetBarFontSize();
RAPOO_API void RapooCore_SetBarIconColorMode(int mode);
RAPOO_API int RapooCore_GetBarIconColorMode();
RAPOO_API void RapooCore_SetBarColorMode(int mode);
RAPOO_API int RapooCore_GetBarColorMode();

RAPOO_API void RapooCore_UpdateTrayTooltip(const wchar_t* tip);

#ifdef __cplusplus
}
#endif
