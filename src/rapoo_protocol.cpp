#include "rapoo_protocol.h"
#include <cstring>
#include <algorithm>

namespace Rapoo {

DWORD BuildUnlockPacket(BYTE* outBuf, DWORD bufSize) {
    if (!outBuf || bufSize < 33) return 0;
    std::memset(outBuf, 0, bufSize);
    outBuf[0] = REPORT_ID_CONTROL; // 0x06
    outBuf[1] = 0xA5;
    outBuf[2] = 0xA3;
    outBuf[3] = 0x00;
    outBuf[4] = 0x00;
    outBuf[5] = 0x00;
    outBuf[6] = 0x00;
    outBuf[7] = 0x00;
    outBuf[8] = 0x00;
    return 33;
}

DWORD BuildRequestStatusPacket(BYTE* outBuf, DWORD bufSize) {
    if (!outBuf || bufSize < 33) return 0;
    std::memset(outBuf, 0, bufSize);
    outBuf[0] = REPORT_ID_CONTROL; // 0x06
    outBuf[1] = 0xA7;              // read_work_mode (triggers immediate 0x07 status report on VT_nrf54L)
    outBuf[2] = 0x00;
    return 33;
}

DWORD BuildWriteCommand(BYTE bank, BYTE addr, const BYTE* pData, BYTE dataLen, BYTE* outBuf, DWORD bufSize) {
    if (!outBuf || bufSize < 33) return 0;
    std::memset(outBuf, 0, bufSize);
    outBuf[0] = REPORT_ID_CONTROL; // 0x06
    outBuf[1] = 0xA5;
    outBuf[2] = 0xA5;
    outBuf[3] = (BYTE)(dataLen & 0xFF);
    outBuf[4] = addr;
    outBuf[5] = bank;
    outBuf[6] = 0x00;
    outBuf[7] = 0x00;
    if (pData && dataLen > 0) {
        BYTE copyLen = (dataLen > 25) ? 25 : dataLen;
        std::memcpy(&outBuf[8], pData, copyLen);
    }
    return 33;
}

DWORD BuildReadCommand(BYTE bank, BYTE addr, BYTE readLen, BYTE* outBuf, DWORD bufSize) {
    if (!outBuf || bufSize < 33) return 0;
    std::memset(outBuf, 0, bufSize);
    outBuf[0] = REPORT_ID_CONTROL; // 0x06
    outBuf[1] = 0xA5;
    outBuf[2] = 0xA4;
    outBuf[3] = readLen;
    outBuf[4] = addr;
    outBuf[5] = bank;
    outBuf[6] = 0x00;
    outBuf[7] = 0x00;
    outBuf[8] = 0x00;
    return 33;
}

bool ParseStatusReport(const BYTE* buf, DWORD bytesRead, DeviceStatus& outStatus, int cachedBattery) {
    if (!buf || bytesRead < 9) return false;
    if (buf[0] != REPORT_ID_STATUS) return false;

    // Ignore pairing/disconnect control events (0xB1, 0xB2, 0xB3)
    if (buf[1] == 0xB1 || buf[1] == 0xB2 || buf[1] == 0xB3) return false;

    // Cybercafe protocol devices prefix 0xB0 (userParams) before the 18-byte payload
    const BYTE* p = (buf[1] == 0xB0) ? (buf + 2) : (buf + 1);
    DWORD pLen = bytesRead - (DWORD)(p - buf);
    if (pLen < 8) return false;

    // In official Rapoo SDK (Nf):
    // p[0] low nibble = conProtocol (0 = active, 2 = disconnected/mode switch)
    // p[0] high nibble = sensor ID (1..15, e.g. 0x10, 0x20, 0x30, 0x40 on PAW3955U/VT3 PRO V2)
    BYTE conProtocol = p[0] & 0x0F;
    BYTE sensorId = (p[0] >> 4) & 0x0F;
    if (sensorId == 0) return false;

    outStatus.isWired = false;
    outStatus.rfConnected = (conProtocol != 2);

    outStatus.dpiLevel = (int)p[1] + 1;
    outStatus.dpiX = (int)p[2] | ((int)p[3] << 8);
    outStatus.dpiY = (int)p[4] | ((int)p[5] << 8);

    BYTE statusByte = p[6]; // batStatus: 1 = discharging, 2/3 = charging
    BYTE rawBat = p[7];     // batPercent: 0..100

    // Charging flag detection based on Rapoo batStatus (p[6]): 2 or 3 means charging
    bool charging = (statusByte == 0x02 || statusByte == 0x03 || (statusByte & 0x02) != 0);
    outStatus.isCharging = charging;

    if (pLen >= 11 && p[10] != 0) {
        outStatus.pollingHz = CodeToPollingHz(p[10]);
    } else {
        outStatus.pollingHz = 0;
    }

    if (pLen >= 18 && p[17] != 0) {
        outStatus.rfConnected = (((p[17] >> 7) & 1) != 0) && (conProtocol != 2);
    }

    // Battery percentage filtering:
    // In Rapoo hardware, rawBat is an integer 0..100.
    // When the mouse is first plugged into USB/charging, rawBat may momentarily report 0 or 1
    // (transient handshake / ADC settle time). We protect the battery gauge by retaining
    // the previous valid reading until a real measurement (>= 2) arrives.
    if (rawBat >= 2 && rawBat <= 100) {
        outStatus.battery = rawBat;
    } else if (rawBat <= 1) {
        if (cachedBattery >= 2 && cachedBattery <= 100) {
            outStatus.battery = cachedBattery;
        } else {
            outStatus.battery = (rawBat == 0) ? 100 : rawBat;
        }
    } else {
        outStatus.battery = 100;
    }

    return true;
}

bool ParseFeatureRead(const BYTE* buf, DWORD bytesRead, BYTE expectedLen, BYTE* outData) {
    if (!buf || bytesRead < 5) return false;

    // Case 1: Windows HID driver stripped Report ID -> buf[0] is ACK (0x01)
    if (buf[0] == 0x01 && bytesRead >= (DWORD)(4 + expectedLen)) {
        if (outData) {
            std::memcpy(outData, &buf[4], expectedLen);
        }
        return true;
    }

    // Case 2: buf[0] is Report ID (0x08) and buf[1] is ACK (0x01)
    if (buf[0] == REPORT_ID_FEATURE && buf[1] == 0x01 && bytesRead >= (DWORD)(5 + expectedLen)) {
        if (outData) {
            std::memcpy(outData, &buf[5], expectedLen);
        }
        return true;
    }

    return false;
}

int CodeToPollingHz(BYTE code) {
    switch (code) {
        case 0x08: return 125;
        case 0x04: return 250;
        case 0x02: return 500;
        case 0x01: return 1000;
        case 0x84: return 2000;
        case 0x82: return 4000;
        case 0x81: return 8000;
        default:   return 1000;
    }
}

BYTE PollingHzToCode(int hz) {
    switch (hz) {
        case 125:  return 0x08;
        case 250:  return 0x04;
        case 500:  return 0x02;
        case 1000: return 0x01;
        case 2000: return 0x84;
        case 4000: return 0x82;
        case 8000: return 0x81;
        default:   return 0x01;
    }
}

} // namespace Rapoo
