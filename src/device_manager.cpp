#include "device_manager.h"
extern "C" {
#include <hidsdi.h>
}
#include <setupapi.h>
#include <devpkey.h>
#include <strsafe.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace Device {

static const WCHAR* REG_SUBKEY = L"Software\\rapoo-tray";
static const WCHAR* REG_BAT_SUBKEY = L"Software\\rapoo-tray\\BatteryCache";
static const WCHAR* REG_NAME_SUBKEY = L"Software\\rapoo-tray\\CustomNames";

// Windows PnP Battery Percent Property Key: {104EA319-6EE2-4701-BD47-8DDBF425BBE5}, pid 2
static const DEVPROPKEY PKEY_Win_BatteryPercent = {
    { 0x104EA319, 0x6EE2, 0x4701, { 0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5 } }, 2
};

// ContainerId Property Key: {8C7ED206-3F8A-4827-B3AB-AE9E1FAEFC6C}, pid 2
static const DEVPROPKEY PKEY_Win_ContainerId = {
    { 0x8C7ED206, 0x3F8A, 0x4827, { 0xB3, 0xAB, 0xAE, 0x9E, 0x1F, 0xAE, 0xFC, 0x6C } }, 2
};

// FriendlyName Property Key: {A45C254E-DF1C-4EFD-8020-67D146A850E0}, pid 14
static const DEVPROPKEY PKEY_Win_FriendlyName = {
    { 0xA45C254E, 0xDF1C, 0x4EFD, { 0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0 } }, 14
};

// DeviceDesc Property Key: {A45C254E-DF1C-4EFD-8020-67D146A850E0}, pid 2
static const DEVPROPKEY PKEY_Win_DeviceDesc = {
    { 0xA45C254E, 0xDF1C, 0x4EFD, { 0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0 } }, 2
};

// BusReportedDeviceDesc Property Key: {540B947E-8B40-45BC-A8A2-6A0B894CBDA2}, pid 4
static const DEVPROPKEY PKEY_Win_BusReportedDesc = {
    { 0x540B947E, 0x8B40, 0x45BC, { 0xA8, 0xA2, 0x6A, 0x0B, 0x89, 0x4C, 0xBD, 0xA2 } }, 4
};

static CRITICAL_SECTION g_csState;
static std::vector<State> g_devices;
static std::wstring g_pinnedDeviceId;
static std::vector<std::wstring> g_deviceOrder;
static StateCallback g_callback = nullptr;
static HWND g_hNotifyWnd = NULL;

static HANDLE g_hWorkerThread = NULL;
static HANDLE g_hStopEvent = NULL;
static HANDLE g_hDevChangeEvent = NULL;
static HANDLE g_hForceRefreshEvent = NULL;

// ----------------------------------------------------------------------------
// Registry Helpers
// ----------------------------------------------------------------------------

static std::wstring LoadRegistryString(const WCHAR* valName) {
    HKEY hKey;
    WCHAR buf[2048] = {0};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(buf);
        if (RegQueryValueExW(hKey, valName, NULL, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && type == REG_SZ) {
            RegCloseKey(hKey);
            return std::wstring(buf);
        }
        RegCloseKey(hKey);
    }
    return L"";
}

static void SaveRegistryString(const WCHAR* valName, const WCHAR* val) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (val && val[0]) {
            RegSetValueExW(hKey, valName, 0, REG_SZ, (const BYTE*)val, (DWORD)((wcslen(val) + 1) * sizeof(WCHAR)));
        } else {
            RegDeleteValueW(hKey, valName);
        }
        RegCloseKey(hKey);
    }
}

static std::wstring LoadCustomName(const WCHAR* devId) {
    if (!devId || !devId[0]) return L"";
    HKEY hKey;
    WCHAR buf[128] = {0};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_NAME_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(buf);
        if (RegQueryValueExW(hKey, devId, NULL, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && type == REG_SZ) {
            RegCloseKey(hKey);
            return std::wstring(buf);
        }
        RegCloseKey(hKey);
    }
    return L"";
}

static void SaveCustomName(const WCHAR* devId, const WCHAR* customName) {
    if (!devId || !devId[0]) return;
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_NAME_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (customName && customName[0]) {
            RegSetValueExW(hKey, devId, 0, REG_SZ, (const BYTE*)customName, (DWORD)((wcslen(customName) + 1) * sizeof(WCHAR)));
        } else {
            RegDeleteValueW(hKey, devId);
        }
        RegCloseKey(hKey);
    }
}

static std::vector<std::wstring> ParseDeviceOrderString(const std::wstring& raw) {
    std::vector<std::wstring> res;
    size_t start = 0;
    while (start < raw.size()) {
        size_t pos = raw.find(L'|', start);
        std::wstring token = (pos == std::wstring::npos) ? raw.substr(start) : raw.substr(start, pos - start);
        if (!token.empty() && std::find(res.begin(), res.end(), token) == res.end()) {
            res.push_back(token);
        }
        if (pos == std::wstring::npos) break;
        start = pos + 1;
    }
    return res;
}

static void SaveDeviceOrderLocked() {
    std::wstring joined;
    for (size_t i = 0; i < g_deviceOrder.size(); ++i) {
        if (i > 0) joined += L"|";
        joined += g_deviceOrder[i];
    }
    SaveRegistryString(L"DeviceOrder", joined.c_str());
}

static int LoadCachedBattery(const WCHAR* devId, int defVal = 100) {
    if (!devId || !devId[0]) return defVal;
    HKEY hKey;
    DWORD val = (DWORD)defVal;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_BAT_SUBKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, size = sizeof(DWORD);
        if (RegQueryValueExW(hKey, devId, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (val >= 1 && val <= 100) return (int)val;
        } else {
            RegCloseKey(hKey);
        }
    }
    return defVal;
}

static void SaveCachedBattery(const WCHAR* devId, int battery) {
    if (!devId || !devId[0] || battery < 1 || battery > 100) return;
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_BAT_SUBKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = (DWORD)battery;
        RegSetValueExW(hKey, devId, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static std::wstring GuidToString(const GUID& g) {
    WCHAR buf[64] = {0};
    StringCchPrintfW(
        buf, ARRAYSIZE(buf),
        L"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        g.Data1, g.Data2, g.Data3,
        g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
        g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]
    );
    return buf;
}

// ----------------------------------------------------------------------------
// Rapoo Model Table & Multi-Device Endpoint Discovery
// ----------------------------------------------------------------------------

struct RapooModelEntry {
    const WCHAR* pidKeyword;
    const WCHAR* modelName;
};

static const RapooModelEntry VERIFIED_MODELS[] = {
    // VT0 系列
    { L"144a", L"雷柏 VT0 MAX 大师版" }, { L"464a", L"雷柏 VT0 MAX 大师版" },
    { L"14a1", L"雷柏 VT0 Air MAX" },    { L"46a1", L"雷柏 VT0 Air MAX" },
    { L"1420", L"雷柏 VT0 MAX" },        { L"4620", L"雷柏 VT0 MAX" },
    { L"1410", L"雷柏 VT0" },            { L"4610", L"雷柏 VT0" },

    // VT1 系列
    { L"1b0a", L"雷柏 VT1S MAX V2" },    { L"4b0a", L"雷柏 VT1S MAX V2" },
    { L"146a", L"雷柏 VT1S V2" },        { L"466a", L"雷柏 VT1S V2" },
    { L"1444", L"雷柏 VT1 MAX" },        { L"4644", L"雷柏 VT1 MAX" },
    { L"1a11", L"雷柏 VT1 SE" },         { L"4a11", L"雷柏 VT1 SE" },
    { L"1415", L"雷柏 VT1" },            { L"4615", L"雷柏 VT1" },

    // VT2 系列
    { L"1475", L"雷柏 VT2 MAX 大师版 V2" },  { L"4675", L"雷柏 VT2 MAX 大师版 V2" },
    { L"1b12", L"雷柏 VT2 Air MAX V2" },     { L"4b12", L"雷柏 VT2 Air MAX V2" },
    { L"1b09", L"雷柏 VT2 MAX V2" },         { L"4b09", L"雷柏 VT2 MAX V2" },
    { L"1b21", L"雷柏 VT2 RT 大师版 V2" },   { L"4b21", L"雷柏 VT2 RT 大师版 V2" },
    { L"1b20", L"雷柏 VT2 RT V2" },          { L"4b20", L"雷柏 VT2 RT V2" },
    { L"1472", L"雷柏 VT2 ULTRA V2" },       { L"4672", L"雷柏 VT2 ULTRA V2" },
    { L"1468", L"雷柏 VT2 V2" },             { L"4668", L"雷柏 VT2 V2" },
    { L"1423", L"雷柏 VT2 MAX" },            { L"4623", L"雷柏 VT2 MAX" },
    { L"1422", L"雷柏 VT2" },                { L"4622", L"雷柏 VT2" },
    { L"14a4", L"雷柏 VT2K MAX" },           { L"46a4", L"雷柏 VT2K MAX" },
    { L"1476", L"雷柏 VT2S MAX 大师版 V2" }, { L"4676", L"雷柏 VT2S MAX 大师版 V2" },
    { L"1b13", L"雷柏 VT2S Air MAX V2" },    { L"4b13", L"雷柏 VT2S Air MAX V2" },
    { L"1b04", L"雷柏 VT2S MAX V2" },        { L"4b04", L"雷柏 VT2S MAX V2" },
    { L"145d", L"雷柏 VT2S V2" },            { L"465d", L"雷柏 VT2S V2" },

    // VT3 系列
    { L"146d", L"雷柏 VT3 MAX 大师版 V2" },  { L"466d", L"雷柏 VT3 MAX 大师版 V2" },
    { L"1b0c", L"雷柏 VT3 Air MAX V2" },     { L"4b0c", L"雷柏 VT3 Air MAX V2" },
    { L"1b06", L"雷柏 VT3 MAX V2" },         { L"4b06", L"雷柏 VT3 MAX V2" },
    { L"14a5", L"雷柏 VT3 PRO V2" },         { L"46a5", L"雷柏 VT3 PRO V2" },
    { L"1462", L"雷柏 VT3 V2" },             { L"4662", L"雷柏 VT3 V2" },
    { L"1417", L"雷柏 VT3 MAX" },            { L"4617", L"雷柏 VT3 MAX" },
    { L"1412", L"雷柏 VT3" },                { L"4612", L"雷柏 VT3" },
    { L"146e", L"雷柏 VT3S MAX 大师版 V2" }, { L"466e", L"雷柏 VT3S MAX 大师版 V2" },
    { L"144b", L"雷柏 VT3S MAX 大师版" },    { L"464b", L"雷柏 VT3S MAX 大师版" },
    { L"1b0d", L"雷柏 VT3S Air MAX V2" },    { L"4b0d", L"雷柏 VT3S Air MAX V2" },
    { L"14a2", L"雷柏 VT3S Air MAX" },       { L"46a2", L"雷柏 VT3S Air MAX" },
    { L"1b07", L"雷柏 VT3S MAX V2" },        { L"4b07", L"雷柏 VT3S MAX V2" },
    { L"1421", L"雷柏 VT3S MAX" },           { L"4621", L"雷柏 VT3S MAX" },
    { L"14a6", L"雷柏 VT3S PRO V2" },        { L"46a6", L"雷柏 VT3S PRO V2" },
    { L"1464", L"雷柏 VT3S V2" },            { L"4664", L"雷柏 VT3S V2" },
    { L"1406", L"雷柏 VT3S" },               { L"4606", L"雷柏 VT3S" },
    { L"1411", L"雷柏 VT3S" },               { L"4611", L"雷柏 VT3S" },

    // VT4 / VT6 系列
    { L"1477", L"雷柏 VT4 MAX 大师版 V2" },  { L"4677", L"雷柏 VT4 MAX 大师版 V2" },
    { L"1b14", L"雷柏 VT4 Air MAX V2" },     { L"4b14", L"雷柏 VT4 Air MAX V2" },
    { L"1b03", L"雷柏 VT4 MAX V2" },         { L"4b03", L"雷柏 VT4 MAX V2" },
    { L"145b", L"雷柏 VT4 V2" },             { L"465b", L"雷柏 VT4 V2" },
    { L"1452", L"雷柏 VT6 MAX" },            { L"4652", L"雷柏 VT6 MAX" },
    { L"1451", L"雷柏 VT6" },                { L"4651", L"雷柏 VT6" },

    // VT7 / VT8 系列
    { L"1470", L"雷柏 VT7 MAX 大师版 V2" },  { L"4670", L"雷柏 VT7 MAX 大师版 V2" },
    { L"1b0e", L"雷柏 VT7 Air MAX V2" },     { L"4b0e", L"雷柏 VT7 Air MAX V2" },
    { L"1b05", L"雷柏 VT7 MAX V2" },         { L"4b05", L"雷柏 VT7 MAX V2" },
    { L"14a7", L"雷柏 VT7 PRO V2" },         { L"46a7", L"雷柏 VT7 PRO V2" },
    { L"1a00", L"雷柏 VT7 SE" },             { L"4a00", L"雷柏 VT7 SE" },
    { L"1460", L"雷柏 VT7 V2" },             { L"4660", L"雷柏 VT7 V2" },
    { L"1416", L"雷柏 VT7 MAX" },            { L"4616", L"雷柏 VT7 MAX" },
    { L"1413", L"雷柏 VT7" },                { L"4613", L"雷柏 VT7" },
    { L"1471", L"雷柏 VT7S MAX 大师版 V2" }, { L"4671", L"雷柏 VT7S MAX 大师版 V2" },
    { L"1b0f", L"雷柏 VT7S Air MAX V2" },    { L"4b0f", L"雷柏 VT7S Air MAX V2" },
    { L"1b02", L"雷柏 VT7S MAX V2" },        { L"4b02", L"雷柏 VT7S MAX V2" },
    { L"14a8", L"雷柏 VT7S PRO V2" },        { L"46a8", L"雷柏 VT7S PRO V2" },
    { L"1459", L"雷柏 VT7S V2" },            { L"4659", L"雷柏 VT7S V2" },
    { L"1478", L"雷柏 VT8 MAX 大师版 V2" },  { L"4678", L"雷柏 VT8 MAX 大师版 V2" },
    { L"1b15", L"雷柏 VT8 Air MAX V2" },     { L"4b15", L"雷柏 VT8 Air MAX V2" },
    { L"1b0b", L"雷柏 VT8 MAX V2" },         { L"4b0b", L"雷柏 VT8 MAX V2" },
    { L"146c", L"雷柏 VT8 V2" },             { L"466c", L"雷柏 VT8 V2" },

    // VT9 系列
    { L"1473", L"雷柏 VT9 MAX 大师版 V2" },  { L"4673", L"雷柏 VT9 MAX 大师版 V2" },
    { L"1b10", L"雷柏 VT9 Air MAX V2" },     { L"4b10", L"雷柏 VT9 Air MAX V2" },
    { L"1b08", L"雷柏 VT9 MAX V2" },         { L"4b08", L"雷柏 VT9 MAX V2" },
    { L"14a9", L"雷柏 VT9 PRO V2" },         { L"46a9", L"雷柏 VT9 PRO V2" },
    { L"1466", L"雷柏 VT9 V2" },             { L"4666", L"雷柏 VT9 V2" },
    { L"1418", L"雷柏 VT9 MAX" },            { L"4618", L"雷柏 VT9 MAX" },
    { L"1414", L"雷柏 VT9" },                { L"4614", L"雷柏 VT9" },
    { L"1474", L"雷柏 VT9S MAX 大师版 V2" }, { L"4674", L"雷柏 VT9S MAX 大师版 V2" },
    { L"1b11", L"雷柏 VT9S Air MAX V2" },    { L"4b11", L"雷柏 VT9S Air MAX V2" },
    { L"1b01", L"雷柏 VT9S MAX V2" },        { L"4b01", L"雷柏 VT9S MAX V2" },
    { L"14aa", L"雷柏 VT9S PRO V2" },        { L"46aa", L"雷柏 VT9S PRO V2" },
    { L"1453", L"雷柏 VT9S V2" },            { L"4653", L"雷柏 VT9S V2" },

    // 底座 / 接收器 / 键盘
    { L"4827", L"雷柏 P5 PLUS 幻彩充底座" },
    { L"4825", L"雷柏 P6 PLUS 充底座" },
    { L"1924", L"雷柏 V700DIY-98" },         { L"4824", L"雷柏 V700DIY-98" },
    { L"1905", L"雷柏 V700DIY-98" },         { L"4805", L"雷柏 V700DIY-98" },
    { L"1906", L"雷柏 V700DIY-87" },         { L"4806", L"雷柏 V700DIY-87" },
    { L"1907", L"雷柏 V700DIY-75" },         { L"4807", L"雷柏 V700DIY-75" },
};

static void ResolveRapooModelName(const WCHAR* targetPid, const WCHAR* usbProductStr, WCHAR* outModel, DWORD maxModelLen) {
    if (!outModel || maxModelLen == 0) return;
    for (const auto& entry : VERIFIED_MODELS) {
        if (targetPid && wcsstr(targetPid, entry.pidKeyword)) {
            StringCchCopyW(outModel, maxModelLen, entry.modelName);
            return;
        }
    }
    if (usbProductStr && usbProductStr[0] && _wcsicmp(usbProductStr, L"Rapoo Gaming Device") != 0) {
        StringCchCopyW(outModel, maxModelLen, usbProductStr);
        return;
    }
    StringCchCopyW(outModel, maxModelLen, L"雷柏无线鼠标");
}

struct RapooEndpointGroup {
    std::wstring id;
    std::wstring pid;
    std::wstring modelName;
    std::wstring pathStatus;
    std::wstring pathControl;
    std::wstring pathFeature;
    USHORT statusInLen = 0;
    USHORT controlOutLen = 0;
    bool isWired = false;

    bool hasTelemetry() const {
        return (statusInLen >= 19 && controlOutLen >= 33);
    }
};

struct ActiveRapooSession {
    RapooEndpointGroup info;
    HANDLE hStatus = INVALID_HANDLE_VALUE;
    HANDLE hControl = INVALID_HANDLE_VALUE;
    HANDLE hFeature = INVALID_HANDLE_VALUE;
    HANDLE hReadEvent = NULL;
    OVERLAPPED ov = {0};
    BYTE readBuf[65] = {0};
    bool readPending = false;
    bool gotFirstPacket = false;
    State state;
};

static bool PingRapooSession(ActiveRapooSession& sess, int& outHz) {
    if (!sess.info.hasTelemetry() || sess.hControl == INVALID_HANDLE_VALUE || sess.hFeature == INVALID_HANDLE_VALUE) {
        return false;
    }

    BYTE outBuf[33] = {0};
    DWORD pktSize = Rapoo::BuildReadCommand(Rapoo::BANK_SYSTEM, Rapoo::ADDR_POLLING_HZ, 1, outBuf, sizeof(outBuf));

    DWORD written = 0;
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    BOOL wOk = WriteFile(sess.hControl, outBuf, pktSize, &written, &ov);
    if (!wOk && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(ov.hEvent, 250) == WAIT_OBJECT_0) {
            GetOverlappedResult(sess.hControl, &ov, &written, FALSE);
            wOk = (written == pktSize);
        } else {
            CancelIo(sess.hControl);
            wOk = FALSE;
        }
    }
    CloseHandle(ov.hEvent);
    if (!wOk) return false;

    // On VT_nrf54L dongles, Feature(0x08) returns 0x02 (busy_ack_value) while waiting for RF response
    BYTE readVal = 0;
    for (int attempt = 0; attempt < 5; ++attempt) {
        Sleep(18);
        BYTE featBuf[33] = {0};
        featBuf[0] = Rapoo::REPORT_ID_FEATURE;
        BOOL fOk = HidD_GetFeature(sess.hFeature, featBuf, sizeof(featBuf));
        if (!fOk) continue;

        if (Rapoo::ParseFeatureRead(featBuf, sizeof(featBuf), 1, &readVal)) {
            outHz = Rapoo::CodeToPollingHz(readVal);
            return true;
        }
        bool isBusy = (featBuf[0] == 0x02) || (featBuf[0] == Rapoo::REPORT_ID_FEATURE && featBuf[1] == 0x02);
        if (!isBusy) break;
    }

    return false;
}

static void SendRapooUnlock(HANDLE hControl) {
    if (hControl == INVALID_HANDLE_VALUE) return;
    BYTE buf[33] = {0};
    DWORD written = 0;
    OVERLAPPED uOv = {0};
    uOv.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    // 1. Send 0xA5 0xA3 read-only unlock handshake
    DWORD uSize = Rapoo::BuildUnlockPacket(buf, sizeof(buf));
    WriteFile(hControl, buf, uSize, &written, &uOv);
    WaitForSingleObject(uOv.hEvent, 150);

    Sleep(20);

    // 2. Send 0xA7 read_work_mode to trigger immediate 0x07 status input report on VT_nrf54L dongles
    ResetEvent(uOv.hEvent);
    DWORD sSize = Rapoo::BuildRequestStatusPacket(buf, sizeof(buf));
    WriteFile(hControl, buf, sSize, &written, &uOv);
    WaitForSingleObject(uOv.hEvent, 150);

    CloseHandle(uOv.hEvent);
}

// Discover ALL connected Rapoo devices simultaneously (no upper limit)
static std::vector<RapooEndpointGroup> DiscoverAllRapooGroups() {
    std::map<std::wstring, RapooEndpointGroup> groups;

    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return {};

    SP_DEVICE_INTERFACE_DATA devData = {0};
    devData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &hidGuid, i, &devData); ++i) {
        DWORD reqSize = 0;
        SetupDiGetDeviceInterfaceDetailW(hDevInfo, &devData, NULL, 0, &reqSize, NULL);
        if (reqSize == 0) continue;

        PSP_DEVICE_INTERFACE_DETAIL_DATA_W pDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(reqSize);
        if (!pDetail) continue;
        pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA devInfo = {0};
        devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetailW(hDevInfo, &devData, pDetail, reqSize, NULL, &devInfo)) {
            WCHAR lowerPath[MAX_PATH] = {0};
            StringCchCopyW(lowerPath, MAX_PATH, pDetail->DevicePath);
            _wcslwr_s(lowerPath, MAX_PATH);

            if (wcsstr(lowerPath, L"vid_24ae")) {
                WCHAR pidStr[16] = L"pid_0000";
                const WCHAR* pPid = wcsstr(lowerPath, L"pid_");
                if (pPid) {
                    StringCchCopyNW(pidStr, ARRAYSIZE(pidStr), pPid, 8);
                }

                // Group interfaces belonging to the same physical device via ContainerId
                GUID containerGuid = {0};
                DEVPROPTYPE propType = 0;
                std::wstring groupKey = pidStr;
                if (SetupDiGetDevicePropertyW(hDevInfo, &devInfo, &PKEY_Win_ContainerId, &propType, (PBYTE)&containerGuid, sizeof(GUID), NULL, 0)) {
                    groupKey = std::wstring(pidStr) + L"_" + GuidToString(containerGuid);
                }

                auto& grp = groups[groupKey];
                if (grp.id.empty()) {
                    grp.id = L"RAPOO_" + groupKey;
                    grp.pid = pidStr;
                    grp.isWired = (wcsstr(pidStr, L"pid_46") != nullptr ||
                                   wcsstr(pidStr, L"pid_4a") != nullptr ||
                                   wcsstr(pidStr, L"pid_4b") != nullptr);
                }

                HANDLE hProbe = CreateFileW(pDetail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hProbe == INVALID_HANDLE_VALUE) {
                    hProbe = CreateFileW(pDetail->DevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                }
                if (hProbe == INVALID_HANDLE_VALUE) {
                    hProbe = CreateFileW(pDetail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                }

                if (hProbe != INVALID_HANDLE_VALUE) {
                    WCHAR prodName[64] = {0};
                    HidD_GetProductString(hProbe, prodName, sizeof(prodName));
                    if (grp.modelName.empty()) {
                        WCHAR resolved[64] = {0};
                        ResolveRapooModelName(pidStr, prodName, resolved, 64);
                        grp.modelName = resolved;
                    }

                    PHIDP_PREPARSED_DATA pData = NULL;
                    if (HidD_GetPreparsedData(hProbe, &pData)) {
                        HIDP_CAPS caps;
                        if (HidP_GetCaps(pData, &caps) == HIDP_STATUS_SUCCESS && caps.UsagePage == 0xFF00) {
                            if (caps.Usage == 0x000E || caps.OutputReportByteLength == 33) {
                                if (caps.OutputReportByteLength >= grp.controlOutLen) {
                                    grp.pathControl = pDetail->DevicePath;
                                    grp.controlOutLen = caps.OutputReportByteLength;
                                }
                            } else if (caps.Usage == 0x000F || (caps.FeatureReportByteLength == 33 && caps.Usage != 0x0010)) {
                                grp.pathFeature = pDetail->DevicePath;
                            } else if (caps.Usage == 0x0002 || (caps.InputReportByteLength >= 19 && wcsstr(lowerPath, L"col09")) || (caps.InputReportByteLength == 19 && caps.Usage != 0x000E)) {
                                if (caps.InputReportByteLength >= grp.statusInLen) {
                                    grp.pathStatus = pDetail->DevicePath;
                                    grp.statusInLen = caps.InputReportByteLength;
                                }
                            }
                        }
                        HidD_FreePreparsedData(pData);
                    }
                    CloseHandle(hProbe);
                }
            }
        }
        free(pDetail);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    std::vector<RapooEndpointGroup> result;
    for (auto& kv : groups) {
        if (!kv.second.pathStatus.empty() && !kv.second.pathControl.empty()) {
            result.push_back(std::move(kv.second));
        }
    }
    return result;
}

// ----------------------------------------------------------------------------
// Multi-Brand External Device Discovery (Bluetooth LE, Logitech HID++,
// Razer, ATK, Standard HID Battery System, and PnP Battery Peripherals)
// ----------------------------------------------------------------------------

static void InferBrandAndCategory(const std::wstring& name, const std::wstring& hwid, State& st) {
    std::wstring lowerName = name;
    for (auto& c : lowerName) c = towlower(c);
    std::wstring lowerHw = hwid;
    for (auto& c : lowerHw) c = towlower(c);

    // Brand detection
    if (lowerHw.find(L"vid_046d") != std::wstring::npos || lowerName.find(L"logi") != std::wstring::npos || lowerName.find(L"g pro") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"罗技 Logitech");
    } else if (lowerHw.find(L"vid_36f9") != std::wstring::npos || lowerName.find(L"xiberia") != std::wstring::npos || lowerName.find(L"西伯利亚") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"西伯利亚 Xiberia");
    } else if (lowerHw.find(L"vid_1532") != std::wstring::npos || lowerName.find(L"razer") != std::wstring::npos || lowerName.find(L"viper") != std::wstring::npos || lowerName.find(L"deathadder") != std::wstring::npos || lowerName.find(L"orochi") != std::wstring::npos || lowerName.find(L"basilisk") != std::wstring::npos || lowerName.find(L"naga") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"雷蛇 Razer");
    } else if (lowerHw.find(L"vid_373b") != std::wstring::npos || lowerName.find(L"atk") != std::wstring::npos || lowerName.find(L"vxe") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"ATK / VXE");
    } else if (lowerHw.find(L"vid_045e") != std::wstring::npos || lowerName.find(L"xbox") != std::wstring::npos || lowerName.find(L"surface") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"微软 Microsoft");
    } else if (lowerHw.find(L"vid_054c") != std::wstring::npos || lowerName.find(L"dualsense") != std::wstring::npos || lowerName.find(L"dualshock") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"索尼 Sony");
    } else if (lowerHw.find(L"vid_05ac") != std::wstring::npos || lowerName.find(L"airpods") != std::wstring::npos || lowerName.find(L"magic") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"苹果 Apple");
    } else if (lowerHw.find(L"vid_3434") != std::wstring::npos || lowerName.find(L"keychron") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"Keychron");
    } else if (lowerName.find(L"pulsar") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"Pulsar");
    } else if (lowerName.find(L"lamzu") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"Lamzu");
    } else if (lowerHw.find(L"bth") != std::wstring::npos) {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"蓝牙无线外设");
    } else {
        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"通用无线外设");
    }

    // Category detection
    if (lowerName.find(L"mouse") != std::wstring::npos || lowerName.find(L"鼠标") != std::wstring::npos ||
        lowerName.find(L"viper") != std::wstring::npos || lowerName.find(L"superlight") != std::wstring::npos ||
        lowerName.find(L"mx master") != std::wstring::npos || lowerName.find(L"mx anywhere") != std::wstring::npos ||
        lowerName.find(L"orochi") != std::wstring::npos || lowerName.find(L"deathadder") != std::wstring::npos ||
        lowerName.find(L"basilisk") != std::wstring::npos || lowerName.find(L"naga") != std::wstring::npos) {
        StringCchCopyW(st.category, ARRAYSIZE(st.category), L"mouse");
    } else if (lowerName.find(L"keyboard") != std::wstring::npos || lowerName.find(L"键盘") != std::wstring::npos ||
               lowerName.find(L"keychron") != std::wstring::npos) {
        StringCchCopyW(st.category, ARRAYSIZE(st.category), L"keyboard");
    } else if (lowerName.find(L"headset") != std::wstring::npos || lowerName.find(L"headphone") != std::wstring::npos ||
               lowerName.find(L"airpods") != std::wstring::npos || lowerName.find(L"buds") != std::wstring::npos ||
               lowerName.find(L"xiberia") != std::wstring::npos || lowerName.find(L"耳机") != std::wstring::npos) {
        StringCchCopyW(st.category, ARRAYSIZE(st.category), L"headset");
    } else if (lowerName.find(L"controller") != std::wstring::npos || lowerName.find(L"gamepad") != std::wstring::npos ||
               lowerName.find(L"xbox") != std::wstring::npos || lowerName.find(L"dualsense") != std::wstring::npos ||
               lowerName.find(L"手柄") != std::wstring::npos) {
        StringCchCopyW(st.category, ARRAYSIZE(st.category), L"gamepad");
    } else {
        StringCchCopyW(st.category, ARRAYSIZE(st.category), L"device");
    }

    // Connection type
    if (lowerHw.find(L"bth") != std::wstring::npos || lowerHw.find(L"bluetooth") != std::wstring::npos) {
        StringCchCopyW(st.connType, ARRAYSIZE(st.connType), L"蓝牙 BLE");
        st.isWired = false;
    } else if (lowerHw.find(L"vid_1532") != std::wstring::npos) {
        StringCchCopyW(st.connType, ARRAYSIZE(st.connType), L"2.4G HyperSpeed");
        st.isWired = false;
    } else {
        StringCchCopyW(st.connType, ARRAYSIZE(st.connType), L"无线连接");
        st.isWired = false;
    }
}

// Cache last known Xiberia headset state by container key so transient RF/HID timeouts never drop the device
static std::map<std::wstring, State> g_xiberiaLastState;

// Query Xiberia / Actions (VID_36F9) wireless headset status & battery on UsagePage 0xFF90 (Report ID 0x55)
static bool QueryXiberiaHeadsetStatus(HANDLE hDev, bool& outOnline, int& outBat, bool& outCharging) {
    HidD_SetNumInputBuffers(hDev, 64);

    auto WriteReport64 = [&](const BYTE* pkt) -> bool {
        OVERLAPPED ov = {0};
        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        DWORD written = 0;
        BOOL ok = WriteFile(hDev, pkt, 64, &written, &ov);
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ov.hEvent, 150) == WAIT_OBJECT_0) {
                GetOverlappedResult(hDev, &ov, &written, FALSE);
                ok = (written > 0);
            } else {
                CancelIo(hDev);
                ok = FALSE;
            }
        }
        CloseHandle(ov.hEvent);
        return ok != FALSE;
    };

    auto ReadReport64 = [&](BYTE* outBuf, DWORD timeoutMs) -> bool {
        memset(outBuf, 0, 65);
        OVERLAPPED ov = {0};
        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(hDev, outBuf, 64, &bytesRead, &ov);
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ov.hEvent, timeoutMs) == WAIT_OBJECT_0) {
                GetOverlappedResult(hDev, &ov, &bytesRead, FALSE);
                ok = (bytesRead > 0);
            } else {
                CancelIo(hDev);
                ok = FALSE;
            }
        }
        CloseHandle(ov.hEvent);
        return ok != FALSE && bytesRead > 0;
    };

    auto ParseXiberiaPacket = [&](const BYTE* resp) -> bool {
        if (resp[0] == 0x55 && resp[1] == 0x10 && resp[2] == 0x01) {
            outOnline = (resp[4] != 0);
            int rawBat = resp[7] & 0x7F;
            outCharging = ((resp[7] & 0x80) != 0);
            outBat = (rawBat >= 1 && rawBat <= 100) ? rawBat : 0;
            return true;
        }
        if (resp[0] == 0x55 && resp[1] == 0x10 && resp[2] == 0x09) {
            outOnline = true;
            int rawBat = resp[4] & 0x7F;
            outCharging = ((resp[4] & 0x80) != 0);
            outBat = (rawBat >= 1 && rawBat <= 100) ? rawBat : 0;
            return true;
        }
        return false;
    };

    // Drain any stale queued input packet first
    for (int d = 0; d < 2; ++d) {
        BYTE drainBuf[65] = {0};
        if (!ReadReport64(drainBuf, 5)) break;
    }

    BYTE queryPkt[64] = {0};
    queryPkt[0] = 0x55;
    queryPkt[1] = 0x10;
    queryPkt[2] = 0x01;

    // 1. Try direct status & battery query first (avoids re-handshaking an already initialized dongle)
    if (WriteReport64(queryPkt)) {
        for (int tries = 0; tries < 3; ++tries) {
            BYTE resp[65] = {0};
            if (!ReadReport64(resp, 120)) break;
            if (ParseXiberiaPacket(resp)) return true;
        }
    }

    // 2. If no response, send Actions ActHID_IniDev wakeup frame [0x55, 0x57, 0x00, ...] and retry query
    BYTE initPkt[64] = {0};
    initPkt[0] = 0x55;
    initPkt[1] = 0x57;
    if (!WriteReport64(initPkt)) {
        return false;
    }
    Sleep(25);
    BYTE drainBuf[65] = {0};
    ReadReport64(drainBuf, 10);

    if (!WriteReport64(queryPkt)) {
        return false;
    }
    for (int tries = 0; tries < 4; ++tries) {
        BYTE resp[65] = {0};
        if (!ReadReport64(resp, 180)) break;
        if (ParseXiberiaPacket(resp)) return true;
    }
    return false;
}

// Query Logitech HID++ 2.0 receiver/device battery (Feature 0x1004 Unified Battery / 0x1000 Battery Status)
static bool QueryLogitechHidppBattery(HANDLE hDev, BYTE devIdx, int& outBat, bool& outCharging) {
    // Short HID++ 2.0 packet (0x10, 7 bytes) or Long (0x11, 20 bytes)
    // Step 1: Query IRoot (0x0000) function 0 to get feature index of 0x1004 (Unified Battery) or 0x1000 (Battery Unified Level)
    const USHORT features[2] = { 0x1004, 0x1000 };
    for (int f = 0; f < 2; ++f) {
        BYTE req[20] = {0};
        req[0] = 0x11; // Long HID++ report
        req[1] = devIdx;
        req[2] = 0x00; // IRoot
        req[3] = 0x0A; // Function 0 (GetFeature), SW ID = 0x0A
        req[4] = (BYTE)(features[f] >> 8);
        req[5] = (BYTE)(features[f] & 0xFF);

        OVERLAPPED wOv = {0};
        wOv.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        DWORD written = 0;
        BOOL wOk = WriteFile(hDev, req, 20, &written, &wOv);
        if (!wOk && GetLastError() == ERROR_IO_PENDING) {
            wOk = (WaitForSingleObject(wOv.hEvent, 120) == WAIT_OBJECT_0);
        }
        CloseHandle(wOv.hEvent);
        if (!wOk) continue;

        BYTE featIdx = 0;
        for (int tries = 0; tries < 4; ++tries) {
            BYTE resp[21] = {0};
            OVERLAPPED rOv = {0};
            rOv.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            DWORD bytesRead = 0;
            BOOL rOk = ReadFile(hDev, resp, 20, &bytesRead, &rOv);
            if (!rOk && GetLastError() == ERROR_IO_PENDING) {
                if (WaitForSingleObject(rOv.hEvent, 120) == WAIT_OBJECT_0) {
                    GetOverlappedResult(hDev, &rOv, &bytesRead, FALSE);
                    rOk = TRUE;
                } else {
                    CancelIo(hDev);
                }
            }
            CloseHandle(rOv.hEvent);
            if (!rOk) break;
            if ((resp[0] == 0x10 || resp[0] == 0x11) && resp[1] == devIdx && resp[2] == 0x00 && (resp[3] & 0x0F) == 0x0A) {
                featIdx = resp[4];
                break;
            }
        }

        if (featIdx == 0) continue;

        // Step 2: Call function 1 (GetStatus) for 0x1004 or function 0 (GetBatteryLevelStatus) for 0x1000
        BYTE batReq[20] = {0};
        batReq[0] = 0x11;
        batReq[1] = devIdx;
        batReq[2] = featIdx;
        batReq[3] = (features[f] == 0x1004) ? 0x1A : 0x0A;

        OVERLAPPED bwOv = {0};
        bwOv.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        wOk = WriteFile(hDev, batReq, 20, &written, &bwOv);
        if (!wOk && GetLastError() == ERROR_IO_PENDING) {
            wOk = (WaitForSingleObject(bwOv.hEvent, 120) == WAIT_OBJECT_0);
        }
        CloseHandle(bwOv.hEvent);
        if (!wOk) continue;

        for (int tries = 0; tries < 4; ++tries) {
            BYTE resp[21] = {0};
            OVERLAPPED rOv = {0};
            rOv.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            DWORD bytesRead = 0;
            BOOL rOk = ReadFile(hDev, resp, 20, &bytesRead, &rOv);
            if (!rOk && GetLastError() == ERROR_IO_PENDING) {
                if (WaitForSingleObject(rOv.hEvent, 120) == WAIT_OBJECT_0) {
                    GetOverlappedResult(hDev, &rOv, &bytesRead, FALSE);
                    rOk = TRUE;
                } else {
                    CancelIo(hDev);
                }
            }
            CloseHandle(rOv.hEvent);
            if (!rOk) break;
            if ((resp[0] == 0x10 || resp[0] == 0x11) && resp[1] == devIdx && resp[2] == featIdx) {
                int pct = (int)resp[4];
                if (pct >= 1 && pct <= 100) {
                    outBat = pct;
                    if (features[f] == 0x1004) {
                        outCharging = (resp[6] == 1); // charging status in 0x1004
                    } else {
                        outCharging = (resp[6] >= 1 && resp[6] <= 3); // charging status in 0x1000
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

// Cache last known Razer device state by container key so RF sleep/timeouts never drop the device
static std::map<std::wstring, BYTE> g_razerTxId;
static std::map<std::wstring, State> g_razerLastState;

// Query Razer 91-byte Feature Report (Razer Synapse / OpenRazer protocol)
static bool QueryRazerReport(HANDLE hDev, BYTE txId, BYTE cmdClass, BYTE cmdId, BYTE dataSize, BYTE arg0, BYTE* outResp, BYTE& outStatus) {
    outStatus = 0;
    for (int attempt = 0; attempt < 2; ++attempt) {
        BYTE req[91] = {0};
        req[0] = 0x00; // Windows HID Report ID
        req[1] = 0x00; // Status
        req[2] = txId; // Transaction ID
        req[6] = dataSize;
        req[7] = cmdClass;
        req[8] = cmdId;
        req[9] = arg0;
        BYTE crc = 0;
        for (int i = 3; i <= 88; ++i) crc ^= req[i];
        req[89] = crc;

        if (!HidD_SetFeature(hDev, req, 91)) return false;

        for (int poll = 0; poll < 4; ++poll) {
            Sleep(15);
            BYTE resp[91] = {0};
            if (!HidD_GetFeature(hDev, resp, 91)) break;
            if (resp[7] != cmdClass || resp[8] != cmdId) continue;
            outStatus = resp[1];
            if (resp[1] == 0x02) { // Success
                if (outResp) memcpy(outResp, resp, 91);
                return true;
            }
            if (resp[1] == 0x01) continue; // Busy, keep polling
            break;
        }
        if (outStatus == 0x04 || outStatus == 0x05) break;
        Sleep(10);
    }
    return false;
}

static bool QueryRazerDeviceStatus(HANDLE hDev, const std::wstring& key, bool& outOnline, int& outBat, bool& outCharging) {
    BYTE txId = g_razerTxId[key];
    if (txId == 0) txId = 0x1F;
    txId = ((txId + 1) & 0x7F);
    if (txId == 0) txId = 1;
    g_razerTxId[key] = txId;

    outOnline = false;
    outBat = 0;
    outCharging = false;

    // 1. Query battery status (cmdClass = 0x07, cmdId = 0x80, dataSize = 0x02)
    BYTE resp[91] = {0};
    BYTE status = 0;
    if (QueryRazerReport(hDev, txId, 0x07, 0x80, 0x02, 0x00, resp, status)) {
        int raw = (int)resp[10];
        if (raw > 0) {
            outOnline = true;
            outBat = (raw * 100 + 127) / 255;
            if (outBat > 100) outBat = 100;
        }
    }

    // 2. Query charging status (cmdClass = 0x07, cmdId = 0x84, dataSize = 0x02) if online
    if (outOnline) {
        BYTE chgResp[91] = {0};
        BYTE chgStatus = 0;
        if (QueryRazerReport(hDev, txId, 0x07, 0x84, 0x02, 0x00, chgResp, chgStatus)) {
            outCharging = (chgResp[10] != 0);
        }
    }

    return outOnline;
}

static std::vector<State> DiscoverExternalBatteryDevices() {
    std::map<std::wstring, State> extMap;

    // 1. Scan Windows PnP device nodes for any peripheral reporting PKEY_Win_BatteryPercent
    HDEVINFO hAllDevs = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hAllDevs != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA devInfo = {0};
        devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(hAllDevs, i, &devInfo); ++i) {
            DEVPROPTYPE propType = 0;
            BYTE batPct = 0;
            DWORD reqSize = 0;
            if (!SetupDiGetDevicePropertyW(hAllDevs, &devInfo, &PKEY_Win_BatteryPercent, &propType, &batPct, sizeof(batPct), &reqSize, 0)) {
                continue;
            }
            if (batPct == 0 || batPct > 100) continue;

            WCHAR instId[256] = {0};
            SetupDiGetDeviceInstanceIdW(hAllDevs, &devInfo, instId, ARRAYSIZE(instId), NULL);
            WCHAR lowerInst[256] = {0};
            StringCchCopyW(lowerInst, ARRAYSIZE(lowerInst), instId);
            _wcslwr_s(lowerInst, ARRAYSIZE(lowerInst));

            // Skip Rapoo (handled with full DPI/Polling telemetry in Rapoo session manager)
            // and skip internal laptop ACPI batteries (ACPI\PNP0C0A)
            if (wcsstr(lowerInst, L"vid_24ae") || wcsstr(lowerInst, L"pnp0c0a") || wcsstr(lowerInst, L"acpi\\")) {
                continue;
            }

            GUID containerGuid = {0};
            std::wstring key = instId;
            if (SetupDiGetDevicePropertyW(hAllDevs, &devInfo, &PKEY_Win_ContainerId, &propType, (PBYTE)&containerGuid, sizeof(GUID), NULL, 0)) {
                key = L"EXT_" + GuidToString(containerGuid);
            }

            if (extMap.find(key) != extMap.end()) continue;

            WCHAR nameBuf[128] = {0};
            if (!SetupDiGetDevicePropertyW(hAllDevs, &devInfo, &PKEY_Win_FriendlyName, &propType, (PBYTE)nameBuf, sizeof(nameBuf), NULL, 0) || !nameBuf[0]) {
                if (!SetupDiGetDevicePropertyW(hAllDevs, &devInfo, &PKEY_Win_BusReportedDesc, &propType, (PBYTE)nameBuf, sizeof(nameBuf), NULL, 0) || !nameBuf[0]) {
                    SetupDiGetDevicePropertyW(hAllDevs, &devInfo, &PKEY_Win_DeviceDesc, &propType, (PBYTE)nameBuf, sizeof(nameBuf), NULL, 0);
                }
            }
            if (!nameBuf[0]) {
                StringCchCopyW(nameBuf, ARRAYSIZE(nameBuf), L"无线外设");
            }

            State st = {};
            StringCchCopyW(st.id, ARRAYSIZE(st.id), key.c_str());
            StringCchCopyW(st.modelName, ARRAYSIZE(st.modelName), nameBuf);
            InferBrandAndCategory(nameBuf, lowerInst, st);
            st.isConnected = true;
            st.isCharging = false;
            st.battery = (int)batPct;
            st.hasDpi = false;
            st.hasPolling = false;
            SaveCachedBattery(st.id, st.battery);

            extMap[key] = st;
        }
        SetupDiDestroyDeviceInfoList(hAllDevs);
    }

    // 2. Scan HID interfaces for Xiberia Headsets (VID_36F9), Logitech HID++ 2.0 (VID_046D) & Standard HID Battery System
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO hHidInfo = SetupDiGetClassDevsW(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hHidInfo != INVALID_HANDLE_VALUE) {
        SP_DEVICE_INTERFACE_DATA ifData = {0};
        ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hHidInfo, NULL, &hidGuid, i, &ifData); ++i) {
            DWORD reqSize = 0;
            SetupDiGetDeviceInterfaceDetailW(hHidInfo, &ifData, NULL, 0, &reqSize, NULL);
            if (reqSize == 0) continue;

            PSP_DEVICE_INTERFACE_DETAIL_DATA_W pDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(reqSize);
            if (!pDetail) continue;
            pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            SP_DEVINFO_DATA devInfo = {0};
            devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

            if (SetupDiGetDeviceInterfaceDetailW(hHidInfo, &ifData, pDetail, reqSize, NULL, &devInfo)) {
                WCHAR lowerPath[MAX_PATH] = {0};
                StringCchCopyW(lowerPath, MAX_PATH, pDetail->DevicePath);
                _wcslwr_s(lowerPath, MAX_PATH);

                // Check Xiberia / Actions wireless headset interface (VID_36F9, UsagePage 0xFF90, Report ID 0x55)
                if (wcsstr(lowerPath, L"vid_36f9")) {
                    GUID containerGuid = {0};
                    DEVPROPTYPE propType = 0;
                    std::wstring key = L"XIBERIA_DEFAULT";
                    if (SetupDiGetDevicePropertyW(hHidInfo, &devInfo, &PKEY_Win_ContainerId, &propType, (PBYTE)&containerGuid, sizeof(GUID), NULL, 0)) {
                        key = L"EXT_" + GuidToString(containerGuid);
                    }

                    if (extMap.find(key) == extMap.end()) {
                        HANDLE hDev = CreateFileW(pDetail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
                        bool canReadWrite = (hDev != INVALID_HANDLE_VALUE);
                        if (!canReadWrite) {
                            hDev = CreateFileW(pDetail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        }
                        if (hDev != INVALID_HANDLE_VALUE) {
                            PHIDP_PREPARSED_DATA pData = NULL;
                            if (HidD_GetPreparsedData(hDev, &pData)) {
                                HIDP_CAPS caps;
                                if (HidP_GetCaps(pData, &caps) == HIDP_STATUS_SUCCESS &&
                                    caps.UsagePage == 0xFF90 &&
                                    caps.InputReportByteLength == 64) {
                                    WCHAR prodName[64] = {0};
                                    if (!HidD_GetProductString(hDev, prodName, sizeof(prodName)) || !prodName[0]) {
                                        StringCchCopyW(prodName, ARRAYSIZE(prodName), L"XIBERIA 无线耳机");
                                    }
                                    size_t len = wcslen(prodName);
                                    while (len > 0 && (prodName[len - 1] == L' ' || prodName[len - 1] == L'\t')) {
                                        prodName[--len] = L'\0';
                                    }

                                    bool online = false;
                                    int bat = 0;
                                    bool charging = false;
                                    if (canReadWrite && QueryXiberiaHeadsetStatus(hDev, online, bat, charging)) {
                                        State st = {};
                                        StringCchCopyW(st.id, ARRAYSIZE(st.id), key.c_str());
                                        StringCchCopyW(st.modelName, ARRAYSIZE(st.modelName), prodName);
                                        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"西伯利亚 Xiberia");
                                        StringCchCopyW(st.category, ARRAYSIZE(st.category), L"headset");
                                        StringCchCopyW(st.connType, ARRAYSIZE(st.connType), L"2.4G 无线");
                                        st.isConnected = online;
                                        st.isWired = false;
                                        st.isCharging = online && charging;
                                        st.hasBattery = true;
                                        st.battery = (online && bat > 0) ? bat : LoadCachedBattery(st.id, (bat > 0) ? bat : 100);
                                        st.hasDpi = false;
                                        st.hasPolling = false;
                                        if (online && bat > 0) {
                                            SaveCachedBattery(st.id, st.battery);
                                        }
                                        g_xiberiaLastState[key] = st;
                                        extMap[key] = st;
                                    } else {
                                        // USB receiver is physically present, preserve last known state so it never vanishes on transient timeout
                                        auto itCache = g_xiberiaLastState.find(key);
                                        if (itCache != g_xiberiaLastState.end()) {
                                            extMap[key] = itCache->second;
                                        } else {
                                            State st = {};
                                            StringCchCopyW(st.id, ARRAYSIZE(st.id), key.c_str());
                                            StringCchCopyW(st.modelName, ARRAYSIZE(st.modelName), prodName);
                                            StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"西伯利亚 Xiberia");
                                            StringCchCopyW(st.category, ARRAYSIZE(st.category), L"headset");
                                            StringCchCopyW(st.connType, ARRAYSIZE(st.connType), L"2.4G 无线");
                                            st.isConnected = false;
                                            st.isWired = false;
                                            st.isCharging = false;
                                            st.hasBattery = true;
                                            st.battery = LoadCachedBattery(st.id, 100);
                                            st.hasDpi = false;
                                            st.hasPolling = false;
                                            g_xiberiaLastState[key] = st;
                                            extMap[key] = st;
                                        }
                                    }
                                }
                                HidD_FreePreparsedData(pData);
                            }
                            CloseHandle(hDev);
                        }
                    }
                }

                // Check Logitech HID++ 2.0 long report interface (VID_046D)
                if (wcsstr(lowerPath, L"vid_046d")) {
                    GUID containerGuid = {0};
                    DEVPROPTYPE propType = 0;
                    std::wstring key = L"LOGI_DEFAULT";
                    if (SetupDiGetDevicePropertyW(hHidInfo, &devInfo, &PKEY_Win_ContainerId, &propType, (PBYTE)&containerGuid, sizeof(GUID), NULL, 0)) {
                        key = L"EXT_" + GuidToString(containerGuid);
                    }

                    if (extMap.find(key) == extMap.end()) {
                        HANDLE hDev = CreateFileW(pDetail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
                        if (hDev != INVALID_HANDLE_VALUE) {
                            PHIDP_PREPARSED_DATA pData = NULL;
                            if (HidD_GetPreparsedData(hDev, &pData)) {
                                HIDP_CAPS caps;
                                if (HidP_GetCaps(pData, &caps) == HIDP_STATUS_SUCCESS &&
                                    (caps.UsagePage == 0xFF00 || caps.UsagePage == 0xFF43) &&
                                    caps.OutputReportByteLength == 20) {
                                    int bat = 0;
                                    bool charging = false;
                                    // Try device index 1 (Lightspeed receiver slot 1) or 0xFF (direct wired/wireless)
                                    if (QueryLogitechHidppBattery(hDev, 0x01, bat, charging) ||
                                        QueryLogitechHidppBattery(hDev, 0xFF, bat, charging)) {
                                        WCHAR prodName[64] = {0};
                                        if (!HidD_GetProductString(hDev, prodName, sizeof(prodName)) || !prodName[0]) {
                                            StringCchCopyW(prodName, ARRAYSIZE(prodName), L"Logitech G 无线外设");
                                        }
                                        State st = {};
                                        StringCchCopyW(st.id, ARRAYSIZE(st.id), key.c_str());
                                        StringCchCopyW(st.modelName, ARRAYSIZE(st.modelName), prodName);
                                        StringCchCopyW(st.brandName, ARRAYSIZE(st.brandName), L"罗技 Logitech");
                                        InferBrandAndCategory(prodName, lowerPath, st);
                                        StringCchCopyW(st.connType, ARRAYSIZE(st.connType), L"2.4G Lightspeed");
                                        st.isConnected = true;
                                        st.isCharging = charging;
                                        st.battery = bat;
                                        SaveCachedBattery(st.id, st.battery);
                                        extMap[key] = st;
                                    }
                                }
                                HidD_FreePreparsedData(pData);
                            }
                            CloseHandle(hDev);
                        }
                    }
                }

                // Check Razer 91-byte Feature Report interface (VID_1532)
                if (wcsstr(lowerPath, L"vid_1532")) {
                    GUID containerGuid = {0};
                    DEVPROPTYPE propType = 0;
                    std::wstring key = L"RAZER_DEFAULT";
                    if (SetupDiGetDevicePropertyW(hHidInfo, &devInfo, &PKEY_Win_ContainerId, &propType, (PBYTE)&containerGuid, sizeof(GUID), NULL, 0)) {
                        key = L"EXT_" + GuidToString(containerGuid);
                    }

                    if (extMap.find(key) == extMap.end()) {
                        HANDLE hDev = CreateFileW(pDetail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hDev != INVALID_HANDLE_VALUE) {
                            PHIDP_PREPARSED_DATA pData = NULL;
                            if (HidD_GetPreparsedData(hDev, &pData)) {
                                HIDP_CAPS caps;
                                if (HidP_GetCaps(pData, &caps) == HIDP_STATUS_SUCCESS &&
                                    caps.FeatureReportByteLength == 91) {
                                    WCHAR rawProd[64] = {0};
                                    HidD_GetProductString(hDev, rawProd, sizeof(rawProd));

                                    WCHAR prodName[64] = {0};
                                    if (_wcsnicmp(rawProd, L"Razer", 5) != 0) {
                                        StringCchPrintfW(prodName, ARRAYSIZE(prodName), L"Razer %s", rawProd[0] ? rawProd : L"无线鼠标");
                                    } else {
                                        StringCchCopyW(prodName, ARRAYSIZE(prodName), rawProd);
                                    }
                                    size_t len = wcslen(prodName);
                                    while (len > 0 && (prodName[len - 1] == L' ' || prodName[len - 1] == L'\t')) {
                                        prodName[--len] = L'\0';
                                    }

                                    bool online = false;
                                    int bat = 0;
                                    bool charging = false;
                                    QueryRazerDeviceStatus(hDev, key, online, bat, charging);

                                    State st = {};
                                    StringCchCopyW(st.id, ARRAYSIZE(st.id), key.c_str());
                                    StringCchCopyW(st.modelName, ARRAYSIZE(st.modelName), prodName);
                                    InferBrandAndCategory(prodName, lowerPath, st);
                                    st.isConnected = online;
                                    st.isWired = false;
                                    st.isCharging = online && charging;
                                    st.hasBattery = true;
                                    st.battery = (online && bat > 0) ? bat : LoadCachedBattery(st.id, 97);
                                    if (online && bat > 0) {
                                        SaveCachedBattery(st.id, st.battery);
                                    }
                                    st.hasDpi = false;
                                    st.hasPolling = false;

                                    g_razerLastState[key] = st;
                                    extMap[key] = st;
                                }
                                HidD_FreePreparsedData(pData);
                            }
                            CloseHandle(hDev);
                        }
                    }
                }
            }
            free(pDetail);
        }
        SetupDiDestroyDeviceInfoList(hHidInfo);
    }

    std::vector<State> out;
    for (auto& kv : extMap) {
        out.push_back(std::move(kv.second));
    }
    return out;
}

// ----------------------------------------------------------------------------
// State Synchronization & Primary Tray Device Selection
// ----------------------------------------------------------------------------

static void ApplyNamesAndOrderLocked() {
    for (auto& st : g_devices) {
        if (!st.defaultModelName[0]) {
            StringCchCopyW(st.defaultModelName, ARRAYSIZE(st.defaultModelName), st.modelName);
        }
        std::wstring custom = LoadCustomName(st.id);
        if (!custom.empty()) {
            StringCchCopyW(st.modelName, ARRAYSIZE(st.modelName), custom.c_str());
        } else {
            StringCchCopyW(st.modelName, ARRAYSIZE(st.modelName), st.defaultModelName);
        }
    }

    if (!g_deviceOrder.empty()) {
        std::stable_sort(g_devices.begin(), g_devices.end(), [](const State& a, const State& b) {
            auto itA = std::find(g_deviceOrder.begin(), g_deviceOrder.end(), a.id);
            auto itB = std::find(g_deviceOrder.begin(), g_deviceOrder.end(), b.id);
            size_t rankA = (itA != g_deviceOrder.end()) ? (size_t)std::distance(g_deviceOrder.begin(), itA) : 10000;
            size_t rankB = (itB != g_deviceOrder.end()) ? (size_t)std::distance(g_deviceOrder.begin(), itB) : 10000;
            return rankA < rankB;
        });
    }

    // Ensure any newly discovered devices are appended to g_deviceOrder so ordering is stable
    bool orderAppended = false;
    for (const auto& st : g_devices) {
        if (st.id[0] && std::find(g_deviceOrder.begin(), g_deviceOrder.end(), st.id) == g_deviceOrder.end()) {
            g_deviceOrder.push_back(st.id);
            orderAppended = true;
        }
    }
    if (orderAppended) {
        SaveDeviceOrderLocked();
    }

    for (size_t i = 0; i < g_devices.size(); ++i) {
        g_devices[i].isPinned = (i == 0);
    }
}

static State SelectPrimaryLocked() {
    if (g_devices.empty()) {
        State emptySt = {};
        StringCchCopyW(emptySt.modelName, ARRAYSIZE(emptySt.modelName), L"未检测到外设");
        StringCchCopyW(emptySt.defaultModelName, ARRAYSIZE(emptySt.defaultModelName), L"未检测到外设");
        StringCchCopyW(emptySt.brandName, ARRAYSIZE(emptySt.brandName), L"多设备监控");
        StringCchCopyW(emptySt.category, ARRAYSIZE(emptySt.category), L"mouse");
        StringCchCopyW(emptySt.connType, ARRAYSIZE(emptySt.connType), L"离线");
        emptySt.isConnected = false;
        return emptySt;
    }

    // The device moved to the top of the list displays its battery in the status bar
    for (const auto& d : g_devices) {
        if (d.hasBattery) {
            return d;
        }
    }

    return g_devices[0];
}

static void RebuildDeviceList(const std::vector<ActiveRapooSession*>& rapooSessions,
                              const std::vector<State>& externalDevs,
                              DWORD changeMask) {
    State primaryCopy = {};
    EnterCriticalSection(&g_csState);
    g_devices.clear();

    // Default initial discovery order before user sorting: Rapoo battery devices -> External battery devices -> Docks
    for (const auto* s : rapooSessions) {
        if (!s->state.hasBattery) continue;
        g_devices.push_back(s->state);
    }

    for (const auto& st : externalDevs) {
        g_devices.push_back(st);
    }

    for (const auto* s : rapooSessions) {
        if (s->state.hasBattery) continue;
        g_devices.push_back(s->state);
    }

    ApplyNamesAndOrderLocked();
    primaryCopy = SelectPrimaryLocked();
    LeaveCriticalSection(&g_csState);

    if (g_callback && changeMask != 0) {
        g_callback(primaryCopy, changeMask);
    }
}

// ----------------------------------------------------------------------------
// Background Multi-Device Worker Thread
// ----------------------------------------------------------------------------

static void CloseRapooSession(ActiveRapooSession* s) {
    if (!s) return;
    if (s->hStatus != INVALID_HANDLE_VALUE) {
        if (s->readPending) CancelIo(s->hStatus);
        CloseHandle(s->hStatus);
    }
    if (s->hControl != INVALID_HANDLE_VALUE) CloseHandle(s->hControl);
    if (s->hFeature != INVALID_HANDLE_VALUE) CloseHandle(s->hFeature);
    if (s->hReadEvent) CloseHandle(s->hReadEvent);
    delete s;
}

static DWORD WINAPI HidWorkerThread(LPVOID) {
    std::vector<ActiveRapooSession*> sessions;
    std::vector<State> extDevices;
    DWORD lastExtScanTick = 0;

    while (WaitForSingleObject(g_hStopEvent, 0) == WAIT_TIMEOUT) {
        // 1. Discover all Rapoo endpoint groups and reconcile active sessions
        auto groups = DiscoverAllRapooGroups();
        bool listChanged = false;

        // Remove sessions whose hardware was unplugged
        for (auto it = sessions.begin(); it != sessions.end(); ) {
            bool stillPresent = false;
            for (const auto& g : groups) {
                if (g.id == (*it)->info.id) {
                    stillPresent = true;
                    break;
                }
            }
            if (!stillPresent) {
                CloseRapooSession(*it);
                it = sessions.erase(it);
                listChanged = true;
            } else {
                ++it;
            }
        }

        // Open new sessions for newly arrived Rapoo devices
        for (const auto& g : groups) {
            bool exists = false;
            for (const auto* s : sessions) {
                if (s->info.id == g.id) {
                    exists = true;
                    break;
                }
            }
            if (exists) continue;

            ActiveRapooSession* sess = new ActiveRapooSession();
            sess->info = g;

            StringCchCopyW(sess->state.id, ARRAYSIZE(sess->state.id), g.id.c_str());
            StringCchCopyW(sess->state.modelName, ARRAYSIZE(sess->state.modelName), g.modelName.c_str());
            StringCchCopyW(sess->state.brandName, ARRAYSIZE(sess->state.brandName), L"雷柏 Rapoo");

            if (g.hasTelemetry()) {
                HANDLE hStatus = CreateFileW(g.pathStatus.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
                if (hStatus == INVALID_HANDLE_VALUE) {
                    delete sess;
                    continue;
                }
                HidD_SetNumInputBuffers(hStatus, 64);
                sess->hStatus = hStatus;
                sess->hReadEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
                sess->ov.hEvent = sess->hReadEvent;

                if (!g.pathControl.empty()) {
                    sess->hControl = CreateFileW(g.pathControl.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
                    if (sess->hControl == INVALID_HANDLE_VALUE) {
                        sess->hControl = CreateFileW(g.pathControl.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
                    }
                }
                if (!g.pathFeature.empty()) {
                    sess->hFeature = CreateFileW(g.pathFeature.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (sess->hFeature == INVALID_HANDLE_VALUE) {
                        sess->hFeature = CreateFileW(g.pathFeature.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    }
                    if (sess->hFeature == INVALID_HANDLE_VALUE) {
                        sess->hFeature = CreateFileW(g.pathFeature.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    }
                }

                StringCchCopyW(sess->state.category, ARRAYSIZE(sess->state.category), L"mouse");
                StringCchCopyW(sess->state.connType, ARRAYSIZE(sess->state.connType), g.isWired ? L"USB 有线" : L"2.4G 无线");
                sess->state.isConnected = true;
                sess->state.isWired = g.isWired;
                sess->state.isCharging = g.isWired;
                sess->state.hasBattery = true;
                sess->state.battery = LoadCachedBattery(g.id.c_str(), 100);
                sess->state.hasDpi = true;
                sess->state.dpiLevel = 1;
                sess->state.dpiX = 800;
                sess->state.dpiY = 800;
                sess->state.hasPolling = true;
                sess->state.pollingHz = 1000;

                // Start overlapped ReadFile on hStatus BEFORE sending unlock/status request
                // so the immediate 0x07 status response is captured without race condition
                ResetEvent(sess->hReadEvent);
                DWORD initBytesRead = 0;
                BOOL rOk = ReadFile(sess->hStatus, sess->readBuf, 19, &initBytesRead, &sess->ov);
                if (rOk) {
                    SetEvent(sess->hReadEvent);
                    sess->readPending = true;
                } else if (GetLastError() == ERROR_IO_PENDING) {
                    sess->readPending = true;
                }

                // Read-only unlock handshake & immediate status report request (0xA7)
                SendRapooUnlock(sess->hControl);

                // Harvest immediate status report if available within 150ms
                if (sess->readPending && WaitForSingleObject(sess->hReadEvent, 150) == WAIT_OBJECT_0) {
                    sess->readPending = false;
                    DWORD bytesRead = 0;
                    if (GetOverlappedResult(sess->hStatus, &sess->ov, &bytesRead, FALSE)) {
                        Rapoo::DeviceStatus devStatus;
                        if (Rapoo::ParseStatusReport(sess->readBuf, bytesRead, devStatus, sess->state.battery)) {
                            sess->gotFirstPacket = true;
                            sess->state.dpiLevel = devStatus.dpiLevel;
                            sess->state.dpiX = devStatus.dpiX;
                            sess->state.dpiY = devStatus.dpiY;
                            sess->state.battery = devStatus.battery;
                            sess->state.isCharging = devStatus.isCharging || sess->state.isWired;
                            if (devStatus.pollingHz > 0) {
                                sess->state.pollingHz = devStatus.pollingHz;
                            }
                            SaveCachedBattery(sess->state.id, devStatus.battery);
                        }
                    }
                }

                int hz = sess->state.pollingHz;
                if (PingRapooSession(*sess, hz)) {
                    sess->state.pollingHz = hz;
                }
            } else {
                // Non-telemetry Rapoo USB device (e.g. Charging Dock / RGB Dock PID_4827)
                StringCchCopyW(sess->state.category, ARRAYSIZE(sess->state.category), L"dock");
                StringCchCopyW(sess->state.connType, ARRAYSIZE(sess->state.connType), L"USB 底座");
                sess->state.isConnected = true;
                sess->state.isWired = true;
                sess->state.isCharging = false;
                sess->state.hasBattery = false;
                sess->state.battery = 0;
                sess->state.hasDpi = false;
                sess->state.hasPolling = false;
            }

            sessions.push_back(sess);
            listChanged = true;
        }

        // 2. Scan external multi-brand / Bluetooth devices periodically or on PnP change
        DWORD nowTick = GetTickCount();
        DWORD extInterval = sessions.empty() ? 2500 : 10000;
        if (listChanged || lastExtScanTick == 0 || (nowTick - lastExtScanTick) >= extInterval) {
            auto newExt = DiscoverExternalBatteryDevices();
            bool extDiff = (newExt.size() != extDevices.size());
            if (!extDiff) {
                for (size_t i = 0; i < newExt.size(); ++i) {
                    if (wcscmp(newExt[i].id, extDevices[i].id) != 0 ||
                        newExt[i].isConnected != extDevices[i].isConnected ||
                        newExt[i].isCharging != extDevices[i].isCharging ||
                        newExt[i].battery != extDevices[i].battery ||
                        wcscmp(newExt[i].modelName, extDevices[i].modelName) != 0) {
                        extDiff = true;
                        break;
                    }
                }
            }
            if (extDiff || listChanged) {
                extDevices = std::move(newExt);
                listChanged = true;
            }
            lastExtScanTick = nowTick;
        }

        if (listChanged) {
            RebuildDeviceList(sessions, extDevices, CHANGE_CONNECTED | CHANGE_BATTERY | CHANGE_LIST);
        }

        // 3. Issue async reads on all active Rapoo sessions with telemetry and wait on events
        std::vector<HANDLE> waitHandles;
        waitHandles.push_back(g_hStopEvent);
        waitHandles.push_back(g_hDevChangeEvent);
        waitHandles.push_back(g_hForceRefreshEvent);

        for (auto* s : sessions) {
            if (!s->info.hasTelemetry()) continue;
            if (!s->readPending && s->hStatus != INVALID_HANDLE_VALUE) {
                ResetEvent(s->hReadEvent);
                DWORD bytesRead = 0;
                BOOL ok = ReadFile(s->hStatus, s->readBuf, 19, &bytesRead, &s->ov);
                if (ok) {
                    SetEvent(s->hReadEvent);
                    s->readPending = true;
                } else if (GetLastError() == ERROR_IO_PENDING) {
                    s->readPending = true;
                }
            }
            if (s->readPending && waitHandles.size() < MAXIMUM_WAIT_OBJECTS) {
                waitHandles.push_back(s->hReadEvent);
            }
        }

        DWORD waitTimeout = sessions.empty() ? 2500 : 3000;
        DWORD waitRes = WaitForMultipleObjects((DWORD)waitHandles.size(), waitHandles.data(), FALSE, waitTimeout);

        if (waitRes == WAIT_OBJECT_0) {
            // Stop requested
            break;
        } else if (waitRes == WAIT_OBJECT_0 + 1) {
            // PnP Device Arrival / Removal debounce
            Sleep(500);
            while (WaitForSingleObject(g_hDevChangeEvent, 0) == WAIT_OBJECT_0) {
                ResetEvent(g_hDevChangeEvent);
                Sleep(100);
            }
            lastExtScanTick = 0;
            continue;
        } else if (waitRes == WAIT_OBJECT_0 + 2) {
            // Force refresh requested
            for (auto* s : sessions) {
                if (!s->info.hasTelemetry()) continue;
                int hz = s->state.pollingHz;
                if (PingRapooSession(*s, hz)) {
                    s->state.pollingHz = hz;
                }
                SendRapooUnlock(s->hControl);
            }
            extDevices = DiscoverExternalBatteryDevices();
            lastExtScanTick = GetTickCount();
            RebuildDeviceList(sessions, extDevices, CHANGE_BATTERY | CHANGE_POLLING | CHANGE_LIST);
            continue;
        } else if (waitRes == WAIT_TIMEOUT) {
            // Ping wireless Rapoo dongles to check sleep/online status
            DWORD mask = 0;
            for (auto* s : sessions) {
                if (s->info.hasTelemetry() && s->gotFirstPacket && !s->state.isWired) {
                    int hz = 0;
                    bool alive = PingRapooSession(*s, hz);
                    if (!alive && s->state.isConnected) {
                        s->state.isConnected = false;
                        s->state.isCharging = false;
                        mask |= CHANGE_CONNECTED | CHANGE_BATTERY;
                    } else if (alive && !s->state.isConnected) {
                        s->state.isConnected = true;
                        s->state.pollingHz = hz;
                        mask |= CHANGE_CONNECTED | CHANGE_POLLING;
                    }
                }
            }
            if (mask != 0) {
                RebuildDeviceList(sessions, extDevices, mask);
            }
            continue;
        }

        // Check which Rapoo session(s) completed a status packet read
        DWORD totalMask = 0;
        bool sessionBroken = false;
        for (auto* s : sessions) {
            if (!s->readPending) continue;
            if (WaitForSingleObject(s->hReadEvent, 0) == WAIT_OBJECT_0) {
                s->readPending = false;
                DWORD bytesRead = 0;
                if (!GetOverlappedResult(s->hStatus, &s->ov, &bytesRead, FALSE)) {
                    sessionBroken = true;
                    continue;
                }

                Rapoo::DeviceStatus devStatus;
                if (Rapoo::ParseStatusReport(s->readBuf, bytesRead, devStatus, s->state.battery)) {
                    s->gotFirstPacket = true;
                    if (!s->state.isConnected) {
                        s->state.isConnected = true;
                        totalMask |= CHANGE_CONNECTED;
                    }
                    bool effWired = s->info.isWired || devStatus.isWired;
                    if (effWired != s->state.isWired) {
                        s->state.isWired = effWired;
                        StringCchCopyW(s->state.connType, ARRAYSIZE(s->state.connType), effWired ? L"USB 有线" : L"2.4G 无线");
                        totalMask |= CHANGE_CONNECTED;
                    }
                    if (devStatus.dpiLevel != s->state.dpiLevel || devStatus.dpiX != s->state.dpiX) {
                        s->state.dpiLevel = devStatus.dpiLevel;
                        s->state.dpiX = devStatus.dpiX;
                        s->state.dpiY = devStatus.dpiY;
                        totalMask |= CHANGE_DPI;
                    }
                    if (devStatus.pollingHz > 0 && devStatus.pollingHz != s->state.pollingHz) {
                        s->state.pollingHz = devStatus.pollingHz;
                        totalMask |= CHANGE_POLLING;
                    }
                    bool effCharging = devStatus.isCharging || effWired;
                    if (devStatus.battery != s->state.battery || effCharging != s->state.isCharging) {
                        s->state.battery = devStatus.battery;
                        s->state.isCharging = effCharging;
                        SaveCachedBattery(s->state.id, devStatus.battery);
                        totalMask |= CHANGE_BATTERY;
                    }
                }
            }
        }

        if (sessionBroken) {
            // Clean up broken session so next loop iteration re-enumerates
            for (auto it = sessions.begin(); it != sessions.end(); ) {
                DWORD dummy = 0;
                if (!(*it)->readPending && !GetOverlappedResult((*it)->hStatus, &(*it)->ov, &dummy, FALSE) && GetLastError() == ERROR_DEVICE_NOT_CONNECTED) {
                    CloseRapooSession(*it);
                    it = sessions.erase(it);
                } else {
                    ++it;
                }
            }
            totalMask |= CHANGE_CONNECTED | CHANGE_LIST;
        }

        if (totalMask != 0) {
            RebuildDeviceList(sessions, extDevices, totalMask);
        }
    }

    for (auto* s : sessions) {
        CloseRapooSession(s);
    }
    sessions.clear();
    return 0;
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

bool Start(HWND hNotifyWnd, StateCallback callback) {
    g_hNotifyWnd = hNotifyWnd;
    g_callback = callback;

    InitializeCriticalSection(&g_csState);
    g_pinnedDeviceId = LoadRegistryString(L"PinnedDeviceId");
    g_deviceOrder = ParseDeviceOrderString(LoadRegistryString(L"DeviceOrder"));
    if (g_deviceOrder.empty() && !g_pinnedDeviceId.empty()) {
        g_deviceOrder.push_back(g_pinnedDeviceId);
    }

    g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_hDevChangeEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_hForceRefreshEvent = CreateEventW(NULL, FALSE, FALSE, NULL);

    g_hWorkerThread = CreateThread(NULL, 0, HidWorkerThread, NULL, 0, NULL);
    return (g_hWorkerThread != NULL);
}

void Stop() {
    if (g_hStopEvent) SetEvent(g_hStopEvent);
    if (g_hWorkerThread) {
        WaitForSingleObject(g_hWorkerThread, 2000);
        CloseHandle(g_hWorkerThread);
        g_hWorkerThread = NULL;
    }
    if (g_hStopEvent) { CloseHandle(g_hStopEvent); g_hStopEvent = NULL; }
    if (g_hDevChangeEvent) { CloseHandle(g_hDevChangeEvent); g_hDevChangeEvent = NULL; }
    if (g_hForceRefreshEvent) { CloseHandle(g_hForceRefreshEvent); g_hForceRefreshEvent = NULL; }
    DeleteCriticalSection(&g_csState);
}

void NotifyDeviceChange() {
    if (g_hDevChangeEvent) {
        SetEvent(g_hDevChangeEvent);
    }
}

bool RefreshPollingRate() {
    if (g_hForceRefreshEvent) {
        SetEvent(g_hForceRefreshEvent);
        return true;
    }
    return false;
}

void ForceRefresh() {
    if (g_hForceRefreshEvent) {
        SetEvent(g_hForceRefreshEvent);
    }
}

State GetCurrentState() {
    EnterCriticalSection(&g_csState);
    State st = SelectPrimaryLocked();
    LeaveCriticalSection(&g_csState);
    return st;
}

std::vector<State> GetAllDevices() {
    EnterCriticalSection(&g_csState);
    std::vector<State> copy = g_devices;
    LeaveCriticalSection(&g_csState);
    return copy;
}

std::wstring GetPinnedDeviceId() {
    EnterCriticalSection(&g_csState);
    std::wstring id = (!g_devices.empty()) ? g_devices[0].id : g_pinnedDeviceId;
    LeaveCriticalSection(&g_csState);
    return id;
}

void SetPinnedDeviceId(const WCHAR* deviceId) {
    if (!deviceId || !deviceId[0]) return;
    std::vector<std::wstring> newOrder;
    newOrder.push_back(deviceId);
    SetDeviceOrder(newOrder);
}

void SetDeviceOrder(const std::vector<std::wstring>& orderedIds) {
    State primaryCopy = {};
    EnterCriticalSection(&g_csState);

    std::vector<std::wstring> merged;
    for (const auto& id : orderedIds) {
        if (!id.empty() && std::find(merged.begin(), merged.end(), id) == merged.end()) {
            merged.push_back(id);
        }
    }
    for (const auto& existing : g_deviceOrder) {
        if (!existing.empty() && std::find(merged.begin(), merged.end(), existing) == merged.end()) {
            merged.push_back(existing);
        }
    }
    g_deviceOrder = std::move(merged);
    SaveDeviceOrderLocked();

    ApplyNamesAndOrderLocked();
    primaryCopy = SelectPrimaryLocked();
    LeaveCriticalSection(&g_csState);

    if (g_callback) {
        g_callback(primaryCopy, CHANGE_CONNECTED | CHANGE_BATTERY | CHANGE_LIST);
    }
}

void SetDeviceCustomName(const WCHAR* deviceId, const WCHAR* customName) {
    if (!deviceId || !deviceId[0]) return;
    State primaryCopy = {};
    EnterCriticalSection(&g_csState);

    SaveCustomName(deviceId, customName);
    ApplyNamesAndOrderLocked();
    primaryCopy = SelectPrimaryLocked();
    LeaveCriticalSection(&g_csState);

    if (g_callback) {
        g_callback(primaryCopy, CHANGE_LIST);
    }
}

} // namespace Device
