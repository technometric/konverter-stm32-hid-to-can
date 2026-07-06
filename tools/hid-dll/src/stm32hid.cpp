#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <hidclass.h>

extern "C" {
#include <hidsdi.h>
}

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "serialcom.h"
#include "stm32hid.h"

namespace {

const WORD STM32_VID = 0x0483;
const WORD STM32_PID = 0x5750;
const BYTE REPORT_TYPE_JSON = 0x01;
const DWORD REPORT_SIZE = 64;
const DWORD PAYLOAD_SIZE = 59;
const DWORD DEFAULT_TIMEOUT_MS = 5000;

HANDLE gStm32Hid = INVALID_HANDLE_VALUE;
BYTE gSeq = 1;

const char* jsonError(const char* msg)
{
    char buf[192];
    _snprintf(buf, sizeof(buf) - 1, "{\"ok\":false,\"err\":\"%s\"}", msg);
    buf[sizeof(buf) - 1] = '\0';
    return dup_str_ansi(buf);
}

bool getCaps(HANDLE handle, HIDP_CAPS* caps)
{
    if (handle == INVALID_HANDLE_VALUE || caps == NULL) return false;

    PHIDP_PREPARSED_DATA ppData = NULL;
    if (!HidD_GetPreparsedData(handle, &ppData)) return false;

    NTSTATUS status = HidP_GetCaps(ppData, caps);
    HidD_FreePreparsedData(ppData);
    return status == HIDP_STATUS_SUCCESS;
}

bool openMatchingHid(WORD vid, WORD pid, HANDLE* outHandle)
{
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO info = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA ifaceData;
    ifaceData.cbSize = sizeof(ifaceData);

    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(info, NULL, &hidGuid, index, &ifaceData); ++index) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(info, &ifaceData, NULL, 0, &requiredSize, NULL);
        if (requiredSize == 0) continue;

        std::vector<BYTE> detailBytes(requiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA detail =
            reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(&detailBytes[0]);
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(info, &ifaceData, detail, requiredSize, NULL, NULL)) {
            continue;
        }

        HANDLE h = CreateFile(
            detail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (h == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attr;
        attr.Size = sizeof(attr);
        if (HidD_GetAttributes(h, &attr) && attr.VendorID == vid && attr.ProductID == pid) {
            *outHandle = h;
            SetupDiDestroyDeviceInfoList(info);
            return true;
        }

        CloseHandle(h);
    }

    SetupDiDestroyDeviceInfoList(info);
    return false;
}

bool ensureOpen()
{
    if (gStm32Hid != INVALID_HANDLE_VALUE) return true;
    return openMatchingHid(STM32_VID, STM32_PID, &gStm32Hid);
}

BYTE nextSeqForJson(const char* jsonText)
{
    const char* key = strstr(jsonText, "\"seq\"");
    const char* colon = key ? strchr(key, ':') : NULL;
    if (colon) {
        int value = atoi(colon + 1);
        if (value > 0 && value < 256) {
            gSeq = static_cast<BYTE>(value + 1);
            if (gSeq == 0) gSeq = 1;
            return static_cast<BYTE>(value);
        }
    }

    BYTE seq = gSeq++;
    if (gSeq == 0) gSeq = 1;
    return seq;
}

bool writeReport(const BYTE* report)
{
    HIDP_CAPS caps;
    DWORD outLen = REPORT_SIZE + 1;
    if (getCaps(gStm32Hid, &caps) && caps.OutputReportByteLength > 0) {
        outLen = std::max<DWORD>(caps.OutputReportByteLength, REPORT_SIZE + 1);
    }

    std::vector<BYTE> tx(outLen, 0);
    memcpy(&tx[1], report, REPORT_SIZE);

    DWORD written = 0;
    return WriteFile(gStm32Hid, &tx[0], outLen, &written, NULL) && written == outLen;
}

bool readOneReport(std::vector<BYTE>& report, DWORD timeoutMs)
{
    HIDP_CAPS caps;
    DWORD inLen = REPORT_SIZE + 1;
    if (getCaps(gStm32Hid, &caps) && caps.InputReportByteLength > 0) {
        inLen = std::max<DWORD>(caps.InputReportByteLength, REPORT_SIZE + 1);
    }

    std::vector<BYTE> rx(inLen, 0);
    DWORD start = GetTickCount();

    while ((GetTickCount() - start) < timeoutMs) {
        DWORD readed = 0;
        BOOL ok = ReadFile(gStm32Hid, &rx[0], inLen, &readed, NULL);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_INVALID_HANDLE || err == ERROR_DEVICE_NOT_CONNECTED) return false;
            Sleep(5);
            continue;
        }

        if (readed >= REPORT_SIZE + 1 && rx[0] == 0 && rx[1] == REPORT_TYPE_JSON) {
            report.assign(rx.begin() + 1, rx.begin() + 1 + REPORT_SIZE);
            return true;
        }

        if (readed >= REPORT_SIZE && rx[0] == REPORT_TYPE_JSON) {
            report.assign(rx.begin(), rx.begin() + REPORT_SIZE);
            return true;
        }
    }

    return false;
}

std::string sendAndReadJson(const char* jsonText)
{
    if (!jsonText || jsonText[0] == '\0') return "{\"ok\":false,\"err\":\"empty_json\"}";
    if (!ensureOpen()) return "{\"ok\":false,\"err\":\"stm32_hid_not_found\"}";

    const BYTE seq = nextSeqForJson(jsonText);

    std::string raw(jsonText);
    const size_t total = std::max<size_t>(1, (raw.size() + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE);
    if (total > 255) return "{\"ok\":false,\"err\":\"json_too_long\"}";

    for (size_t i = 0; i < total; ++i) {
        BYTE report[REPORT_SIZE];
        memset(report, 0, sizeof(report));

        const size_t offset = i * PAYLOAD_SIZE;
        const size_t len = std::min<size_t>(PAYLOAD_SIZE, raw.size() - offset);

        report[0] = REPORT_TYPE_JSON;
        report[1] = seq;
        report[2] = static_cast<BYTE>(i);
        report[3] = static_cast<BYTE>(total);
        report[4] = static_cast<BYTE>(len);
        if (len > 0) memcpy(report + 5, raw.data() + offset, len);

        if (!writeReport(report)) return "{\"ok\":false,\"err\":\"hid_write_failed\"}";
        Sleep(2);
    }

    std::map<int, std::string> chunks;
    int expectedTotal = -1;
    DWORD start = GetTickCount();

    while ((GetTickCount() - start) < DEFAULT_TIMEOUT_MS) {
        std::vector<BYTE> report;
        DWORD elapsed = GetTickCount() - start;
        DWORD remain = elapsed >= DEFAULT_TIMEOUT_MS ? 1 : DEFAULT_TIMEOUT_MS - elapsed;
        if (!readOneReport(report, std::min<DWORD>(remain, 250))) continue;
        if (report.size() < REPORT_SIZE || report[0] != REPORT_TYPE_JSON || report[1] != seq) continue;

        int index = report[2];
        int totalRx = report[3];
        int len = report[4];
        if (totalRx <= 0 || len < 0 || len > static_cast<int>(PAYLOAD_SIZE)) continue;

        expectedTotal = totalRx;
        chunks[index] = std::string(reinterpret_cast<const char*>(&report[5]), len);

        if (expectedTotal > 0 && static_cast<int>(chunks.size()) >= expectedTotal) {
            std::string out;
            for (int i = 0; i < expectedTotal; ++i) out += chunks[i];
            return out;
        }
    }

    return "{\"ok\":false,\"err\":\"hid_timeout\"}";
}

std::string withSeq(const char* jsonBody)
{
    char buf[768];
    _snprintf(buf, sizeof(buf) - 1, "{\"seq\":%u,%s", static_cast<unsigned>(gSeq), jsonBody);
    buf[sizeof(buf) - 1] = '\0';
    return std::string(buf);
}

} // namespace

extern "C" {

CBS_API bool CBS_CALL openStm32Converter()
{
    return ensureOpen();
}

CBS_API void CBS_CALL closeStm32Converter()
{
    if (gStm32Hid != INVALID_HANDLE_VALUE) {
        CloseHandle(gStm32Hid);
        gStm32Hid = INVALID_HANDLE_VALUE;
    }
}

CBS_API const char* CBS_CALL stm32SendJson(const char* jsonText)
{
    std::string result = sendAndReadJson(jsonText);
    return dup_str_ansi(result.c_str());
}

CBS_API const char* CBS_CALL stm32Ping()
{
    std::string cmd = withSeq("\"cmd\":\"ping\"}");
    std::string result = sendAndReadJson(cmd.c_str());
    return dup_str_ansi(result.c_str());
}

CBS_API const char* CBS_CALL stm32GetSensor(int node, const char* sensor, int channel)
{
    if (node < 1 || node > 48) return jsonError("node_out_of_range");
    if (!sensor || sensor[0] == '\0') sensor = "flow";

    char cmd[192];
    _snprintf(cmd, sizeof(cmd) - 1,
              "{\"seq\":%u,\"cmd\":\"get\",\"node\":%d,\"sensor\":\"%s\",\"ch\":%d}",
              static_cast<unsigned>(gSeq), node, sensor, channel);
    cmd[sizeof(cmd) - 1] = '\0';

    std::string result = sendAndReadJson(cmd);
    return dup_str_ansi(result.c_str());
}

CBS_API const char* CBS_CALL stm32GetAll(int node)
{
    if (node < 1 || node > 48) return jsonError("node_out_of_range");

    char cmd[96];
    _snprintf(cmd, sizeof(cmd) - 1,
              "{\"seq\":%u,\"cmd\":\"get_all\",\"node\":%d}",
              static_cast<unsigned>(gSeq), node);
    cmd[sizeof(cmd) - 1] = '\0';

    std::string result = sendAndReadJson(cmd);
    return dup_str_ansi(result.c_str());
}

CBS_API const char* CBS_CALL stm32ScanAll()
{
    std::string cmd = withSeq("\"cmd\":\"scan_all\"}");
    std::string result = sendAndReadJson(cmd.c_str());
    return dup_str_ansi(result.c_str());
}

}
