#define WIN32_LEAN_AND_MEAN
#include "cbs32.h"
#include <windows.h>
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
#include "serialcom.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sstream>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
//#pragma comment(lib, "hid.lib")
//#pragma comment(lib, "setupapi.lib")


HANDLE hHidDevice = INVALID_HANDLE_VALUE;
HANDLE hSerial = INVALID_HANDLE_VALUE;// Handle HID device (deklarasi global)
unsigned char deviceBuffer[48][128];

int  deviceLen[48];

#ifdef HID_MODE
const char* CBS_CALL getVersion() {
    static char lastResponse[256];

    if (hHidDevice == INVALID_HANDLE_VALUE) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"no HID device\"}");
        return dup_str_ansi(lastResponse);
    }

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
    // ========== Rakit frame VER ========== 
    // Format contoh: [STX] V E R [CK1][CK2] [ETX]
    const unsigned char STX = 0x05;
    const unsigned char ETX = 0x04;

    //unsigned char tx[8]; 
    int pos = 0;
    tx[pos++] = (hasReportID) ? 1 : 0;
    tx[pos++] = STX;
    tx[pos++] = 'V';
    tx[pos++] = 'E';
    tx[pos++] = 'R';

    unsigned char ck[3] = {0};
    make_checksum_ascii(tx.data(), pos, ck);
    tx[pos++] = ck[0];   // CK ascii hi
    tx[pos++] = ck[1];   // CK ascii lo
    tx[pos++] = ETX;

    logMsgHex("TX RAW", tx.data(), tx.size());
    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hHidDevice, tx.data(), (DWORD)tx.size(), &bytesWritten, NULL);
    if (!ok) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"write fail %lu\"}", GetLastError());
        return dup_str_ansi(lastResponse);
    }

    const char* ack = getResponse(false); // ambil payload plain
    if (!ack) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"no response\"}");
        return dup_str_ansi(lastResponse);
    }

    wsprintfA(lastResponse, "{\"status\":\"ok\",\"versi\":\"%s\"}", ack);
    return dup_str_ansi(lastResponse);
}

const char* CBS_CALL readMonitoring(int id){
    static char lastResponse[4096];
    if (hHidDevice == INVALID_HANDLE_VALUE) {
        snprintf(lastResponse, sizeof(lastResponse),"{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}");
        return dup_str_ansi(lastResponse);
    }
    if (id < 1 || id > 48) {
        snprintf(lastResponse, sizeof(lastResponse),"{\"status\":\"err\",\"msg\":\"bad id\"}");
        return dup_str_ansi(lastResponse);
    }

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

    unsigned char frame[32];
    
    int len = buildFrameRA(id, frame, sizeof(frame));
    tx[0] = (hasReportID) ? 1 : 0;
    memcpy(tx.data()+1, frame, len);
    //logMsgHex("TX RAW", tx.data(), tx.size());
    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hHidDevice, tx.data(), (DWORD)tx.size(), &bytesWritten, NULL);
    if (!ok) {
        snprintf(lastResponse, sizeof(lastResponse),"{\"status\":\"err\",\"msg\":\"write fail %lu\"}", GetLastError());
        return dup_str_ansi(lastResponse);
    }
    
    const char* ack = getResponse(false);
    //const char* ack = "01RM03011500000000000B00411000000000010101100000000000000022";
    size_t ack_len = strlen(ack);
    if (ack_len >= 54) {
        snprintf(lastResponse,
            sizeof(lastResponse),
            "{"
                "\"status\":\"ok\","
                "\"data\":{"
                "\"deviceID\":\"%.2s\","
                "\"cmd\":\"%.2s\","
                "\"MTemp\":\"%.4s\","
                "\"ATemp\":\"%.4s\","
                "\"MWater\":\"%.4s\","
                "\"AWater\":\"%.4s\","
                "\"Status1\":\"%c\","
                "\"Status2\":\"%c\","
                "\"Output\":{"
                    "\"Out1\":\"%c\",\"Out2\":\"%c\",\"Out3\":\"%c\",\"Out4\":\"%c\","
                    "\"Out5\":\"%c\",\"Out6\":\"%c\",\"Out7\":\"%c\",\"Out8\":\"%c\""
                "},"
                "\"Input\":{"
                    "\"In1\":\"%c\",\"In2\":\"%c\",\"In3\":\"%c\",\"In4\":\"%c\",\"In5\":\"%c\""
                "},"
                "\"prodgId\":\"%.3s\","
                "\"stepCounter\":\"%.2s\","
                "\"stepId\":\"%.2s\","
                "\"param\":[\"%.4s\",\"%.4s\",\"%.4s\"]"
            "}}",
            ack,              // deviceID (0–1)
            ack + 2,          // cmd (2–3)
            ack + 4,          // MTemp (4–7)
            ack + 8,          // ATemp (8–11)
            ack + 12,         // MWater (12–15)
            ack + 16,         // AWater (16–19)
            ack[20], ack[21], ack[22], ack[23], ack[24],   // Out1–Out5
            ack[25], ack[26], ack[27], ack[28], ack[29],   // Out6–Out10
            ack[30], ack[31], ack[32], ack[33], ack[34],   // In1–In5
            ack + 35,          // prodgId (35–37)
            ack + 38,          // stepCounter (38–39)
            ack + 40,          // stepId (40–41)
            ack + 42,          // param[0] (42–45)
            ack + 46,          // param[1] (46–49)
            ack + 50           // param[2] (50–53)
        );    
    }
    else{
        snprintf(lastResponse, sizeof(lastResponse), "{\"status\":\"err\",\"data\":\"no data\"}");
    }
    return dup_str_ansi(lastResponse);
}

const char* CBS_CALL handleAksi(int id, const char* aksi) {
    static char lastResponse[64];
    if (hHidDevice == INVALID_HANDLE_VALUE) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}");
        return dup_str_ansi(lastResponse);
    }
    if (id < 1 || id > 48) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"bad id\"}");
        return dup_str_ansi(lastResponse);
    }
    
    // Validasi aksi
    if (strcmp(aksi, "WT") != 0 && strcmp(aksi, "WR") != 0 && 
        strcmp(aksi, "WQ") != 0 && strcmp(aksi, "WM") != 0) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"bad aksi\"}");
        return dup_str_ansi(lastResponse);
    }
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

    char payload[32];
    int pos = 0;
    
    payload[pos++] = 0x05;  // STX
    
    // ID dalam format 2 digit hex
    sprintf(&payload[pos], "%02X", id);
    pos += 2;
    
    // Aksi
    payload[pos++] = aksi[0];
    payload[pos++] = aksi[1];
    
    // Hitung checksum
    uint8_t cs = 0;
    for (int i = 0; i < pos; i++) {
        cs += (uint8_t)payload[i];
    }
    
    // Tambahkan checksum
    sprintf(&payload[pos], "%02X", cs);
    pos += 2;
    
    payload[pos++] = 0x04;  // ETX

    tx[0] = (hasReportID) ? 1 : 0;
    memcpy(tx.data()+1, payload, pos);
    //logMsgHex("TX RAW", tx.data(), tx.size());
    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hHidDevice, tx.data(), (DWORD)tx.size(), &bytesWritten, NULL);
    if (!ok) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"write fail %lu\"}", GetLastError());
        return dup_str_ansi(lastResponse);
    }
    
    const char* ack = getResponse2(false);
    
    if (strcmp(ack, "no response") == 0 || strcmp(ack, "serial error") == 0) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"no response\"}");
        return dup_str_ansi(lastResponse);
    }
    
    if (!ack) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"no response\"}");
        return dup_str_ansi(lastResponse);
    }

    wsprintfA(lastResponse, "{\"status\":\"ok\",\"versi\":\"%s\"}", ack);
    return dup_str_ansi(lastResponse);
}

int CBS_CALL handleAksi2(int id, const char* aksi) {
    if (id < 1 || id > 48) return -1;
    if (!aksi) return -1;
    
    // Validasi aksi
    if (strcmp(aksi, "WT") != 0 && strcmp(aksi, "WR") != 0 && 
        strcmp(aksi, "WQ") != 0 && strcmp(aksi, "WM") != 0) {
        return -1;
    }

    if (hHidDevice == INVALID_HANDLE_VALUE) {
        return -2;
    }

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

    char payload[32];
    int pos = 0;
    
    payload[pos++] = 0x05;  // STX
    
    // ID dalam format 2 digit hex
    sprintf(&payload[pos], "%02X", id);
    pos += 2;
    
    // Aksi
    payload[pos++] = aksi[0];
    payload[pos++] = aksi[1];
    
    // Hitung checksum
    uint8_t cs = 0;
    for (int i = 0; i < pos; i++) {
        cs += (uint8_t)payload[i];
    }
    
    // Tambahkan checksum
    sprintf(&payload[pos], "%02X", cs);
    pos += 2;
    
    payload[pos++] = 0x04;  // ETX

    tx[0] = (hasReportID) ? 1 : 0;
    memcpy(tx.data()+1, payload, pos);
    //logMsgHex("TX RAW", tx.data(), tx.size());
    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hHidDevice, tx.data(), (DWORD)tx.size(), &bytesWritten, NULL);
    if (!ok) {
        //wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"write fail %lu\"}", GetLastError());
        return -GetLastError(); //dup_str_ansi(lastResponse);
    }
    
    const char* ack = getResponse2(false);
    
    if (strcmp(ack, "no response") == 0 || strcmp(ack, "serial error") == 0) {
        return 1;
    }
    
    logMsgHex("ACK", (const uint8_t*)ack, strlen(ack));
    Sleep(50);
    return 0;
}

const char* CBS_CALL getMonitoring(int id) {
    static char lastResponse[1024];

    if (hHidDevice == INVALID_HANDLE_VALUE) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}");
        return dup_str_ansi(lastResponse);
    }
    if (id < 1 || id > 48) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"bad id\"}");
        return dup_str_ansi(lastResponse);
    }

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

    unsigned char frame[32];
    
    int len = buildFrame(id, frame, sizeof(frame));
    tx[0] = (hasReportID) ? 1 : 0;
    memcpy(tx.data()+1, frame, len);
    //logMsgHex("TX RAW", tx.data(), tx.size());
    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hHidDevice, tx.data(), (DWORD)tx.size(), &bytesWritten, NULL);
    if (!ok) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"write fail %lu\"}", GetLastError());
        return dup_str_ansi(lastResponse);
    }
    
    const char* ack = getResponse(false);
    wsprintfA(lastResponse, "{\"id\":%d,\"data\":\"%s\"}", id, ack);

    return dup_str_ansi(lastResponse);
}
#else
const char* CBS_CALL getMonitoring(int id) {
    static char lastResponse[1024];

    if (hSerial == INVALID_HANDLE_VALUE) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}");
        return dup_str_ansi(lastResponse);
    }
    if (id < 1 || id > 48) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"bad id\"}");
        return dup_str_ansi(lastResponse);
    }

    unsigned char frame[32];
    int len = buildFrame(id, frame, sizeof(frame));
    //logMsgHex("TX", frame, len);
    
    DWORD written = 0;
    BOOL okW = WriteFile(hSerial, frame, (DWORD)len, &written, NULL);
    if (!okW) {
        DWORD e = GetLastError();
        wsprintfA(lastResponse, "{\"status\":\"err\",\"id\":%d,\"err\":%lu}", id, e);
        return dup_str_ansi(lastResponse);
    }

    const char* ack = getResponse(false);
    wsprintfA(lastResponse, "{\"id\":%d,\"data\":\"%s\"}", id, ack);

    return dup_str_ansi(lastResponse);
}

const char* CBS_CALL readMonitoring(int id){
    static char lastResponse[1024];
    if (hSerial == INVALID_HANDLE_VALUE) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"INVALID_HANDLE_VALUE\"}");
        return dup_str_ansi(lastResponse);
    }
    if (id < 1 || id > 48) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"bad id\"}");
        return dup_str_ansi(lastResponse);
    }

    unsigned char frame[32];
    int len = buildFrame(id, frame, sizeof(frame));
    //logMsgHex("TX", frame, len);
    
    DWORD written = 0;
    BOOL okW = WriteFile(hSerial, frame, (DWORD)len, &written, NULL);
    if (!okW) {
        DWORD e = GetLastError();
        wsprintfA(lastResponse, "{\"status\":\"err\",\"id\":%d,\"err\":%lu}", id, e);
        return dup_str_ansi(lastResponse);
    }

    const char* ack = getResponse(false);
    //const char* ack = "01RM03011500000000000B00411000000000010101100000000000000022";
    size_t ack_len = strlen(ack);
    if (ack_len >= 54) {
        wsprintfA(lastResponse,
            "{"
                "\"status\":\"ok\","
                "\"data\":{"
                "\"deviceID\":\"%.2s\","
                "\"cmd\":\"%.2s\","
                "\"MTemp\":\"%.4s\","
                "\"ATemp\":\"%.4s\","
                "\"MWater\":\"%.4s\","
                "\"AWater\":\"%.4s\","
                "\"Status1\":\"%c\","
                "\"Status2\":\"%c\","
                "\"Output\":{"
                    "\"Out1\":\"%c\",\"Out2\":\"%c\",\"Out3\":\"%c\",\"Out4\":\"%c\","
                    "\"Out5\":\"%c\",\"Out6\":\"%c\",\"Out7\":\"%c\",\"Out8\":\"%c\""
                "},"
                "\"Input\":{"
                    "\"In1\":\"%c\",\"In2\":\"%c\",\"In3\":\"%c\",\"In4\":\"%c\",\"In5\":\"%c\""
                "},"
                "\"prodgId\":\"%.3s\","
                "\"stepCounter\":\"%.2s\","
                "\"stepId\":\"%.2s\","
                "\"param\":[\"%.4s\",\"%.4s\",\"%.4s\"]"
            "}}",
            ack,              // deviceID (0–1)
            ack + 2,          // cmd (2–3)
            ack + 4,          // MTemp (4–7)
            ack + 8,          // ATemp (8–11)
            ack + 12,         // MWater (12–15)
            ack + 16,         // AWater (16–19)
            ack[20], ack[21], ack[22], ack[23], ack[24],   // Out1–Out5
            ack[25], ack[26], ack[27], ack[28], ack[29],   // Out6–Out10
            ack[30], ack[31], ack[32], ack[33], ack[34],   // In1–In5
            ack + 35,          // prodgId (35–37)
            ack + 38,          // stepCounter (38–39)
            ack + 40,          // stepId (40–41)
            ack + 42,          // param[0] (42–45)
            ack + 46,          // param[1] (46–49)
            ack + 50           // param[2] (50–53)
        );    
    }
    else{
        wsprintfA(lastResponse, "{\"status\":\"err\",\"data\":\"no data\"}");
    }
    return dup_str_ansi(lastResponse);
}

const char* CBS_CALL getTest(int id) {
    // Demo: return tiny JSON; in real impl, read from serial/CAN/etc
    // Ensure ANSI and heap-allocated
    char buf[256];
    wsprintfA(buf, "{\"ok\":true,\"id\":%d,\"msg\":\"cbs32 monitoring\"}", id);
    return dup_str_ansi(buf);
}

const char* CBS_CALL getVersion() {
    static char lastResponse[256];

    if (hSerial == INVALID_HANDLE_VALUE) {
        //strcpy(lastResponse, "{\"status\":\"err\",\"msg\":\"no serial\"}");
        //return lastResponse;
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"no serial\"}");
        return dup_str_ansi(lastResponse);
    }

    // ========== Rakit frame VER ========== 
    // Format contoh: [STX] V E R [CK1][CK2] [ETX]
    const unsigned char STX = 0x05;
    const unsigned char ETX = 0x04;

    unsigned char tx[8]; int pos = 0;
    tx[pos++] = STX;
    tx[pos++] = 'V';
    tx[pos++] = 'E';
    tx[pos++] = 'R';

    unsigned char ck[3]={0};
    make_checksum_ascii(tx, pos, ck);
    tx[pos++] = ck[0];   // CK ascii hi
    tx[pos++] = ck[1];   // CK ascii lo
    tx[pos++] = ETX;

    DWORD written = 0;
    BOOL okW = WriteFile(hSerial, tx, (DWORD)pos, &written, NULL);
    if (!okW || written != (DWORD)pos) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"write fail %lu\"}", GetLastError());
        return dup_str_ansi(lastResponse);
    }
    //logMsgHex("TX VER", tx, pos);

    const char* ack = getResponse(false); // ambil payload plain
    if (!ack) {
        wsprintfA(lastResponse, "{\"status\":\"err\",\"msg\":\"no response\"}");
        return dup_str_ansi(lastResponse);
    }

    wsprintfA(lastResponse, "{\"status\":\"ok\",\"versi\":\"%s\"}", ack);
    return dup_str_ansi(lastResponse);
}

int CBS_CALL handleAksi(int id, const char* aksi) {
    if (id < 1 || id > 48) return -1;
    if (!aksi) return -1;
    
    // Validasi aksi
    if (strcmp(aksi, "WT") != 0 && strcmp(aksi, "WR") != 0 && 
        strcmp(aksi, "WQ") != 0 && strcmp(aksi, "WM") != 0) {
        return -1;
    }

    char payload[32];
    int pos = 0;
    
    payload[pos++] = 0x05;  // STX
    
    // ID dalam format 2 digit hex
    sprintf(&payload[pos], "%02X", id);
    pos += 2;
    
    // Aksi
    payload[pos++] = aksi[0];
    payload[pos++] = aksi[1];
    
    // Hitung checksum
    uint8_t cs = 0;
    for (int i = 0; i < pos; i++) {
        cs += (uint8_t)payload[i];
    }
    
    // Tambahkan checksum
    sprintf(&payload[pos], "%02X", cs);
    pos += 2;
    
    payload[pos++] = 0x04;  // ETX

    DWORD written;
    WriteFile(hSerial, payload, pos, &written, NULL);
    logMsgHex("TX AKSI", (const uint8_t*)payload, pos);
    
    const char* ack = getResponse(false);
    
    if (strcmp(ack, "no response") == 0 || strcmp(ack, "serial error") == 0) {
        return 1;
    }
    
    logMsgHex("ACK", (const uint8_t*)ack, strlen(ack));
    Sleep(50);
    return 0;
}

void CBS_CALL freeCString(const char* p) {
    if (p) HeapFree(GetProcessHeap(), 0, (LPVOID)p);
}

#endif
// DllMain: XP-safe
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    (void)hinst; (void)reserved;
    if (reason == DLL_PROCESS_DETACH) {
        CloseSerial();
    }
    return TRUE;
}
