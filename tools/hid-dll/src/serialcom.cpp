#include <windows.h>
#include "serialcom.h"
#ifdef HID_MODE
    #define WINVER 0x0501
    #define _WIN32_WINNT 0x0501 
    
    #include <setupapi.h>
    #include <cfgmgr32.h>
    
    // INITGUID harus sebelum include GUID headers
    #define INITGUID
    #include <initguid.h>      // Pindah ke sini
    #include <devguid.h>
    #include <hidclass.h>
    
    extern "C"
    {
        #include <hidsdi.h>
    }
#endif
#include <sstream>
#include <iomanip>
#include <mutex>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <iostream>
#include "json.hpp"   //  ← letakkan file json.hpp di folder include/

using json = nlohmann::json;

// Minimal, XP-safe heap helpers
char* dup_str_ansi(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* p = (char*)HeapAlloc(GetProcessHeap(), 0, n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}
// ---- LOG optional (bisa no-op jika tidak ada logger) ----
void logMsg(const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    SAFE_SNPRINTF(buf, sizeof(buf), fmt, ap); // best-effort
    va_end(ap);
    OutputDebugStringA(buf);
    OutputDebugStringA("\r\n");
    // Output ke file (sederhana)
    FILE* logFile = fopen("C:/Temp/cbs32.log", "a");
    if (logFile != NULL) {
        fprintf(logFile, "%s\r\n", buf);
        fclose(logFile);
    }
}

void logMsgHex(const char* tag, const uint8_t* p, int n) {
    char line[512]; int pos = 0;
    pos += SAFE_SNPRINTF(line+pos, sizeof(line)-pos, "%s:", tag);
    for (int i=0;i<n && pos<(int)sizeof(line)-4;i++)
        pos += SAFE_SNPRINTF(line+pos, sizeof(line)-pos, " %02X", p[i]);
    OutputDebugStringA(line); OutputDebugStringA("\r\n");
    FILE* logFile = fopen("C:/Temp/cbs32hex.log", "a");
    if (logFile != NULL) {
        fprintf(logFile, "%s\r\n", line);
        fclose(logFile);
    }
}

// ---- checksum ASCII 2 char (sum % 256) ----
void make_checksum_ascii(const uint8_t* data, int len, uint8_t out[3]) {
    unsigned int s = 0;
    for (int i=0;i<len;i++) s += data[i];
    uint8_t v = (uint8_t)(s & 0xFF);
    const char* H="0123456789ABCDEF";
    out[0] = (uint8_t)H[v>>4];
    out[1] = (uint8_t)H[v&0x0F];
    out[2] = 0;
}


std::string toHex2(const std::string& data) {
    std::ostringstream os;
    os << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < data.length(); i++) {
        unsigned char c = data[i];
        os << std::setw(2) << (int)c << " ";
    }
    return os.str();
}

// ============================
//  Hitung checksum (sum 8-bit semua byte payload)
// ============================
uint8_t calcChecksum(const std::string& data) {
    uint16_t sum = 0;
    for (unsigned char c : data)
        sum += c;
    return static_cast<uint8_t>(sum & 0xFF);
}

int buildFrame(int id, unsigned char* out, int maxlen) {
    if (maxlen < 10) return 0;
    const unsigned char STX = 0x05;
    const unsigned char ETX = 0x04;
    
    char idStr[3];
    sprintf(idStr, "%02d", id);
    
    int checksum = STX;
    checksum += 'D';
    checksum += 'C';
    checksum += 'R';
    checksum += 'Z';
    checksum += idStr[0];
    checksum += idStr[1];
    checksum &= 0xFF;
    
    int pos = 0;
    out[pos++] = STX;
    out[pos++] = 'D';
    out[pos++] = 'C';
    out[pos++] = 'R';
    out[pos++] = 'Z';
    out[pos++] = (unsigned char)idStr[0];
    out[pos++] = (unsigned char)idStr[1];
    
    char chk[3];
    sprintf(chk, "%02X", checksum);
    out[pos++] = (unsigned char)chk[0];
    out[pos++] = (unsigned char)chk[1];
    
    out[pos++] = ETX;
    return pos;
}

int buildFrameRA(int id, unsigned char* out, int maxlen) {
    if (maxlen < 10) return 0;
    const unsigned char STX = 0x05;
    const unsigned char ETX = 0x04;
    
    char idStr[3];
    sprintf(idStr, "%02d", id);
    
    int checksum = STX;
    checksum += idStr[0];
    checksum += idStr[1];
    checksum += 'R';
    checksum += 'A';    
    checksum &= 0xFF;
    
    int pos = 0;
    out[pos++] = STX;
    out[pos++] = (unsigned char)idStr[0];
    out[pos++] = (unsigned char)idStr[1];
    out[pos++] = 'R';
    out[pos++] = 'A';

    char chk[3];
    sprintf(chk, "%02X", checksum);
    out[pos++] = (unsigned char)chk[0];
    out[pos++] = (unsigned char)chk[1];
    
    out[pos++] = ETX;
    return pos;
}

// ---------------- Serial mock ----------------
static bool compareChecksum(const unsigned char* rxbuf, int stx, int etx) {
    if (etx - stx < 4) return false;

    const unsigned char* chkPtr = &rxbuf[etx - 2];

    unsigned char hi = chkPtr[0];
    unsigned char lo = chkPtr[1];
    uint8_t hiVal = (hi <= '9') ? hi - '0' : (hi & 0xDF) - 'A' + 10;
    uint8_t loVal = (lo <= '9') ? lo - '0' : (lo & 0xDF) - 'A' + 10;
    uint8_t recvVal = (hiVal << 4) | loVal;

    uint8_t sum = 0;
    for (int i = stx; i < etx - 2; ++i)
        sum += rxbuf[i];
    sum &= 0xFF;

    //logMsgHex("CS RCV", &recvVal, 1);
    //logMsgHex("CS CALC", &sum, 1);
    return (sum == recvVal);
}

#ifdef HID_MODE
// ========== HID VERSION ==========
char* getResponse(bool jsonFmt) 
{
    static char lastResponse[512];
    static BYTE rxbuf[128];

    if (hHidDevice == INVALID_HANDLE_VALUE) {
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}" :
            "HID device error");
        return lastResponse;
    }

    PHIDP_PREPARSED_DATA ppData = nullptr;
    HidD_GetPreparsedData(hHidDevice, &ppData);

    HIDP_CAPS caps;
    HidP_GetCaps(ppData, &caps);
    HidD_FreePreparsedData(ppData);

    std::vector<BYTE> rx(caps.InputReportByteLength);
    DWORD readed = 0;
    memset(rxbuf, 0, sizeof(rxbuf));

    // ==== Overlapped setup ====
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent) {
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"CreateEvent failed\"}" :
            "CreateEvent failed");
        return lastResponse;
    }

    const DWORD TIMEOUT_MS = 500; // ubah sesuai kebutuhan
    BOOL ok = ReadFile(hHidDevice, rx.data(), (DWORD)rx.size(), &readed, &ov);

    if (!ok) {
        DWORD err = GetLastError();

        if (err == ERROR_IO_PENDING) {
            // Tunggu dengan timeout
            DWORD waitRes = WaitForSingleObject(ov.hEvent, TIMEOUT_MS);

            if (waitRes == WAIT_TIMEOUT) {
                CancelIo(hHidDevice);
                CloseHandle(ov.hEvent);
                sprintf_s(lastResponse, jsonFmt ?
                    "{\"status\":\"err\",\"msg\":\"timeout %lu ms\"}" :
                    "timeout %lu ms", TIMEOUT_MS);
                return lastResponse;
            } 
            else if (waitRes == WAIT_OBJECT_0) {
                if (!GetOverlappedResult(hHidDevice, &ov, &readed, FALSE)) {
                    DWORD err2 = GetLastError();
                    CloseHandle(ov.hEvent);
                    sprintf_s(lastResponse, jsonFmt ?
                        "{\"status\":\"err\",\"msg\":\"GetOverlappedResult %lu\"}" :
                        "GetOverlappedResult %lu", err2);
                    return lastResponse;
                }
                // sukses, lanjut ke bawah
            } 
            else {
                // WAIT_FAILED
                DWORD err3 = GetLastError();
                CloseHandle(ov.hEvent);
                sprintf_s(lastResponse, jsonFmt ?
                    "{\"status\":\"err\",\"msg\":\"WaitForSingleObject failed %lu\"}" :
                    "WaitForSingleObject failed %lu", err3);
                return lastResponse;
            }
        } else {
            // ReadFile gagal langsung (bukan pending)
            CloseHandle(ov.hEvent);
            sprintf_s(lastResponse, jsonFmt ?
                "{\"status\":\"err\",\"msg\":\"ReadFile failed %lu\"}" :
                "ReadFile failed %lu", err);
            return lastResponse;
        }
    }

    CloseHandle(ov.hEvent);

    // === Parsing data ===
    if (readed <= 1) {
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"no data\"}" :
            "no data");
        return lastResponse;
    }

    memcpy(rxbuf, rx.data() + 1, readed - 1);
    readed -= 1;

    const unsigned char STX = 0x06;
    const unsigned char ETX = 0x03;
    int stx = -1, etx = -1;

    for (DWORD i = 0; i < readed; i++) {
        if (rxbuf[i] == STX && stx < 0) stx = i;
        if (rxbuf[i] == ETX && i > stx) { etx = i; break; }
    }

    if (stx < 0 || etx < 0 || etx - stx < 2) {
        char hex[256] = {0}, *ph = hex;
        for (DWORD i = 0; i < readed && (ph - hex) < (ptrdiff_t)(sizeof(hex)-4); i++)
            ph += _snprintf(ph, sizeof(hex)-(ph-hex), "%02X ", rxbuf[i]);

        _snprintf(lastResponse, sizeof(lastResponse),
                  "{\"status\":\"raw\",\"len\":%lu,\"data\":\"%s\"}", readed, hex);
        return lastResponse;
    }

    if (!compareChecksum(rxbuf, stx, etx)) {
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"ceksum error\"}" :
            "ceksum error");
        return lastResponse;
    }

    // === Ekstrak payload ===
    char payload[256] = {0};
    int p = 0;
    for (int i = stx + 1; i < (etx - 2) && p < (int)sizeof(payload) - 1; i++) {
        unsigned char c = rxbuf[i];
        if (c >= 0x20 && c <= 0x7E)
            payload[p++] = (char)c;
        else if (c == 0x00)
            payload[p++] = '.';
        else
            p += _snprintf(payload + p, sizeof(payload) - p, "[%02X]", c);
    }

    payload[p] = 0;

    if (jsonFmt)
        _snprintf(lastResponse, sizeof(lastResponse),
                  "{\"status\":\"ok\",\"data\":\"%s\"}", payload);
    else
        strncpy(lastResponse, payload, sizeof(lastResponse) - 1);

    return lastResponse;
}


char* getResponse2(bool jsonFmt)
{
    static char lastResponse[512];
    static BYTE rxbuf[128];

    if (hHidDevice == INVALID_HANDLE_VALUE) {
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}" :
            "HID device error");
        return lastResponse;
    }

    PHIDP_PREPARSED_DATA ppData = nullptr;
    HidD_GetPreparsedData(hHidDevice, &ppData);

    HIDP_CAPS caps;
    HidP_GetCaps(ppData, &caps);
    HidD_FreePreparsedData(ppData);
    std::vector<BYTE> rx(caps.InputReportByteLength);
    // HID tidak punya PurgeComm, langsung read
    DWORD readed = 0;
    memset(rxbuf, 0, sizeof(rxbuf));
    Sleep(10);
    BOOL ok = ReadFile(hHidDevice, rx.data(), (DWORD)rx.size(), &readed, NULL);
    if (!ok) {
        logMsgHex("Error RX RAW", rx.data(), rx.size());   // <-- biar kelihatan frame mentah di DebugView
        DWORD err = GetLastError();
        sprintf_s(lastResponse, jsonFmt ? "{\"status\":\"err\",\"msg\":\"read failed %lu\"}" :"read failed %lu", err);
        return lastResponse;
    }
   
    memcpy(rxbuf, rx.data()+1, readed-1);
    readed -= 1;
    // === Parsing STX/ETX ===
    const unsigned char STX = 0x06;
    const unsigned char ETX = 0x03;
    int stx = -1, etx = -1;

    // HID report bisa punya Report ID di byte[0], skip jika perlu
    // Cek apakah ada Report ID (jika descriptor pakai)
    int offset = 0;
    // Jika rxbuf[0] bukan STX dan bukan data valid, kemungkinan Report ID
    // Uncomment baris ini jika pakai Report ID:
    // if (rxbuf[0] == 0x00) offset = 1;

    for (DWORD i = offset; i < readed; i++) {
        if (rxbuf[i] == STX && stx < 0) stx = i;
        if (rxbuf[i] == ETX && i > (DWORD)stx) { etx = i; break; }
    }

    // Kalau tidak ada STX/ETX, tampilkan raw data saja
    if (stx < 0 || etx < 0 || etx - stx < 2) {
        char hex[256] = {0}, *ph = hex;
        for (DWORD i = offset; i < readed && (ph - hex) < (ptrdiff_t)(sizeof(hex)-4); i++)
            ph += _snprintf(ph, sizeof(hex)-(ph-hex), "%02X ", rxbuf[i]);

        _snprintf(lastResponse, sizeof(lastResponse),
                  "{\"status\":\"raw\",\"len\":%lu,\"data\":\"%s\"}", readed, hex);
        return lastResponse;
    }

    if(!compareChecksum(rxbuf, stx, etx)){
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"ceksum error\"}" :
            "ceksum error");
        return lastResponse;
    }
    
    // === Ekstrak payload di antara STX dan ETX ===
    char payload[256] = {0};
    int p = 0;
    for (int i = stx + 1; i < (etx-2) && p < (int)sizeof(payload)-1; i++) {
        unsigned char c = rxbuf[i];
        if (c >= 0x20 && c <= 0x7E)
            payload[p++] = (char)c;
        else if (c == 0x00)
            payload[p++] = '.';
        else
            p += _snprintf(payload + p, sizeof(payload) - p, "[%02X]", c);
    }

    payload[p] = 0; // terminator

    // === Kembalikan JSON / plain ===
    if (jsonFmt)
        _snprintf(lastResponse, sizeof(lastResponse),
                  "{\"status\":\"ok\",\"data\":\"%s\"}", payload);
    else
        strncpy(lastResponse, payload, sizeof(lastResponse)-1);

    return lastResponse;
}

#else
char* getResponse(bool jsonFmt)
{
    static char lastResponse[512];
    static unsigned char rxbuf[512];

    if (hSerial == INVALID_HANDLE_VALUE) {
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}" :
            "serial error");
        return lastResponse;
    }

    PurgeComm(hSerial, PURGE_RXCLEAR);

    DWORD readed = 0;
    BOOL okR = ReadFile(hSerial, rxbuf, sizeof(rxbuf), &readed, NULL);
    if (!okR) {
        DWORD err = GetLastError();
        _snprintf(lastResponse, sizeof(lastResponse),
                  "{\"status\":\"err\",\"msg\":\"ReadFile fail (%lu)\"}", err);
        return lastResponse;
    }
    if (readed == 0) {
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"no data\"}" :
            "no data");
        return lastResponse;
    }
    
    logMsgHex("RX RAW", rxbuf, readed);   // <-- biar kelihatan frame mentah di DebugView

    // === Parsing STX/ETX ===
    const unsigned char STX = 0x06;
    const unsigned char ETX = 0x03;
    int stx = -1, etx = -1;

    for (DWORD i = 0; i < readed; i++) {
        if (rxbuf[i] == STX && stx < 0) stx = i;
        if (rxbuf[i] == ETX && i > (DWORD)stx) { etx = i; break; }
    }

    // Kalau tidak ada STX/ETX, tampilkan raw data saja
    if (stx < 0 || etx < 0 || etx - stx < 2) {
        char hex[256] = {0}, *ph = hex;
        for (DWORD i = 0; i < readed && (ph - hex) < (ptrdiff_t)(sizeof(hex)-4); i++)
            ph += _snprintf(ph, sizeof(hex)-(ph-hex), "%02X ", rxbuf[i]);

        _snprintf(lastResponse, sizeof(lastResponse),
                  "{\"status\":\"raw\",\"len\":%lu,\"data\":\"%s\"}", readed, hex);
        return lastResponse;
    }

    if(!compareChecksum(rxbuf, stx, etx)){
        strcpy(lastResponse, jsonFmt ?
            "{\"status\":\"err\",\"msg\":\"ceksum error\"}" :
            "ceksum error");
        return lastResponse;
    }
    // === Ekstrak payload di antara STX dan ETX ===
    char payload[256] = {0};
    int p = 0;
    for (int i = stx + 1; i < (etx-2) && p < (int)sizeof(payload)-1; i++) {
        unsigned char c = rxbuf[i];
        if (c >= 0x20 && c <= 0x7E)
            payload[p++] = (char)c;
        else if (c == 0x00)
            payload[p++] = '.';
        else
            p += _snprintf(payload + p, sizeof(payload) - p, "[%02X]", c);
    }

    payload[p] = 0; // terminator

    // === Kembalikan JSON / plain ===
    if (jsonFmt)
        _snprintf(lastResponse, sizeof(lastResponse),
                  "{\"status\":\"ok\",\"data\":\"%s\"}", payload);
    else
        strncpy(lastResponse, payload, sizeof(lastResponse)-1);

    return lastResponse;
}
#endif

#ifdef HID_MODE
static int _openHidDevice(WORD vid, WORD pid) {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&hidGuid, NULL, NULL, 
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        std::cout << "SetupDiGetClassDevs gagal\n";
        return ERR_OPEN_FAILED;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    bool found = false;

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &hidGuid, 
         i, &deviceInterfaceData); i++) {
        
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, 
            NULL, 0, &requiredSize, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = 
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, 
            detailData, requiredSize, NULL, NULL)) {
            
            /*HANDLE hDevice = CreateFile(detailData->DevicePath, 
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);*/
            
            HANDLE hDevice = CreateFile(detailData->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING,
            0,                                      // ← blocking
            NULL);

            if (hDevice != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES attrib;
                attrib.Size = sizeof(HIDD_ATTRIBUTES);
                
                if (HidD_GetAttributes(hDevice, &attrib)) {
                    if (attrib.VendorID == vid && attrib.ProductID == pid) {
                        hHidDevice = hDevice;                        
                        found = true;
                        SetupDiDestroyDeviceInfoList(deviceInfoSet);
                        std::wcout << L"Device ditemukan: " << detailData->DevicePath << std::endl;
                        free(detailData);
                        return ERR_OK;
                    }
                }
                CloseHandle(hDevice);
            }
        }
        free(detailData);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    if (!found) {
        std::cout << "Device tidak ditemukan.\n";
        return ERR_PORT_NOT_FOUND;
    }
    return ERR_OK;
}

const char* __stdcall sendDataDummy(const char* jsonStr) {
    static std::string last;

    PHIDP_PREPARSED_DATA ppData = nullptr;
    HidD_GetPreparsedData(hHidDevice, &ppData);

    HIDP_CAPS caps;
    HidP_GetCaps(ppData, &caps);
    // cek apakah device punya report ID ≠ 0
    HIDP_BUTTON_CAPS buttonCaps[1];
    USHORT numCaps = 1;
    NTSTATUS status = HidP_GetButtonCaps(HidP_Output, buttonCaps, &numCaps, ppData);

    BOOL hasReportID = FALSE;
    if (status == HIDP_STATUS_SUCCESS && numCaps > 0) {
        hasReportID = (buttonCaps[0].ReportID != 0);
    }

    HidD_FreePreparsedData(ppData);
    std::vector<BYTE> tx(caps.OutputReportByteLength,0x00);

    try {
        // === Parse JSON input ===
        json root = json::parse(jsonStr);
        std::string id = root["id"];
        std::string MTemp = root["MTemp"];
        std::string ATemp = root["ATemp"];
        std::string MWater = root["MWater"];
        std::string AWater = root["AWater"];
        std::string io = root["io"];
        std::string progId = root["progId"];
        std::string stepNumber = root["stepNumber"];
        std::string stepId = root["stepId"];
        // === Build payload string ===
        std::ostringstream oss;
        oss << id
            << "RA"
            << MTemp
            << ATemp
            << MWater
            << AWater
            << io
            << progId
            << stepNumber
            << stepId;

        auto values = root["value"];
        for (size_t i = 0; i < values.size(); ++i) {
            char buf[8];
            sprintf_s(buf, "%04d", (int)values[i]);
            oss << buf;
        }

        std::string payload = oss.str();

        // === Tambahkan STX / ETX ===
        std::string frame;
        frame.push_back(0x05);   // STX
        frame += payload;
        frame.push_back(0x04);   // ETX

        // === Kirim ke UART ===
        if (hHidDevice != INVALID_HANDLE_VALUE) {
            last = "{\"status\":\"ok\",\"msg\":\"Data sent\"}";
        }
        else {
            last = "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}";
            return last.c_str();
        }

        tx[0] = (hasReportID) ? 1 : 0;
        memcpy(tx.data()+1, frame.c_str(), frame.length());
        
        logMsgHex("TX RAW", tx.data(), tx.size());
        DWORD bytesWritten = 0;
        BOOL ok = WriteFile(hHidDevice, tx.data(), (DWORD)tx.size(), &bytesWritten, NULL);
        if (!ok) {
            char buf[128];
            wsprintfA(buf, "{\"status\":\"err\",\"msg\":\"write fail %lu\"}", GetLastError());
            last = buf;
            return dup_str_ansi(last.c_str());
        }
    }
    catch (std::exception& e) {
        last = std::string("{\"status\":\"err\",\"msg\":\"") + e.what() + "\"}";
    }
    return last.c_str();
}
// ========== Fungsi Helper: Close HID Device ==========
static void _closeHidDevice(void) {
    if (hHidDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hHidDevice);
        hHidDevice = INVALID_HANDLE_VALUE;
    }
}
#else
static int _OpenSerial(const char* portName, unsigned long baudRate)
{
    if (!portName || !*portName) return ERR_PORT_NOT_FOUND;

    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
    }

    char device[64];
    if (strncmp(portName, "\\\\.\\", 4) == 0)
        lstrcpynA(device, portName, sizeof(device));
    else if (_strnicmp(portName, "COM", 3) == 0)
        wsprintfA(device, "\\\\.\\%s", portName);
    else
        lstrcpynA(device, portName, sizeof(device));

    HANDLE h = CreateFileA(
        device,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
            return ERR_PORT_NOT_FOUND; // COM tidak ada
        else if (err == ERROR_ACCESS_DENIED)
            return ERR_ALREADY_OPEN;   // COM sedang dipakai aplikasi lain
        else
            return ERR_OPEN_FAILED;
    }

    // ========== Set parameter port ==========
    DCB dcb; ZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    GetCommState(h, &dcb);
    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;      // <-- penting
    dcb.fParity  = FALSE;
    dcb.fOutxCtsFlow = 0;
    dcb.fOutxDsrFlow = 0;
    dcb.fDtrControl  = DTR_CONTROL_ENABLE;
    dcb.fRtsControl  = RTS_CONTROL_ENABLE;
    SetCommState(h, &dcb);
    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return ERR_SET_STATE_FAIL;
    }
    // ========== Timeout & buffer ==========
    COMMTIMEOUTS to = {0};
    to.ReadIntervalTimeout        = 50;
    to.ReadTotalTimeoutConstant   = 100;
    to.ReadTotalTimeoutMultiplier = 10;
    to.WriteTotalTimeoutConstant  = 100;
    SetCommTimeouts(h, &to);

    SetupComm(h, 512, 512);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    EscapeCommFunction(h, SETDTR);
    EscapeCommFunction(h, SETRTS);

    hSerial = h;
    return ERR_OK;
}

const char* __stdcall sendDataDummy(const char* jsonStr) {
    static std::string last;

    try {
        // === Parse JSON input ===
        json root = json::parse(jsonStr);

        std::string id = root["id"];
        std::string MTemp = root["MTemp"];
        std::string ATemp = root["ATemp"];
        std::string MWater = root["MWater"];
        std::string AWater = root["AWater"];
        std::string io = root["io"];
        std::string progId = root["progId"];
        std::string stepNumber = root["stepNumber"];
        std::string stepId = root["stepId"];

        // === Build payload string ===
        std::ostringstream oss;
        oss << id
            << "RA"
            << MTemp
            << ATemp
            << MWater
            << AWater
            << io
            << progId
            << stepNumber
            << stepId;

        auto values = root["value"];
        for (size_t i = 0; i < values.size(); ++i) {
            char buf[8];
            sprintf_s(buf, "%04d", (int)values[i]);
            oss << buf;
        }

        std::string payload = oss.str();

        // === Tambahkan STX / ETX ===
        std::string frame;
        frame.push_back(0x05);   // STX
        frame += payload;
        frame.push_back(0x04);   // ETX

        // === Kirim ke UART ===
        if (hSerial != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hSerial, frame.data(), (DWORD)frame.size(), &written, NULL);
            last = getResponse();
        }
        else {
            last = "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}";
            return last.c_str();
        }
    }
    catch (std::exception& e) {
        last = std::string("{\"status\":\"err\",\"msg\":\"") + e.what() + "\"}";
    }
    return last.c_str();
}

static void _CloseSerial(void) {
    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
    }
}
#endif

int __stdcall OpenSerial(const char* portName, unsigned long baudRate){
    #ifdef HID_MODE
        return _openHidDevice(0x303A, 0x4002);
    #else
        return _OpenSerial(portName, baudRate);
    #endif
}

void __stdcall CloseSerial(void) {
    #ifdef HID_MODE
        _closeHidDevice();
    #else
        _CloseSerial();
    #endif
}