#pragma once
#include <windows.h>
#include <string>
#include <cstdint>
#include "mode.h"
#define ERR_OK              0
#define ERR_PORT_NOT_FOUND  1
#define ERR_ALREADY_OPEN    2
#define ERR_OPEN_FAILED     3
#define ERR_SET_STATE_FAIL  4
#define ERR_UNKNOWN         9
// Global variables (deklarasi extern)
extern HANDLE hHidDevice;
extern HANDLE hSerial;


#ifndef SAFE_SNPRINTF
  #define SAFE_SNPRINTF _snprintf
#endif
// buffer untuk 48 device (slot terbaru)
extern unsigned char deviceBuffer[48][128];
extern int  deviceLen[48];

// Serial communication functions
char* dup_str_ansi(const char* s);
char* getResponse(bool jsonFmt = false);
char* getResponse2(bool jsonFmt = false);
void logMsg(const char* fmt, ...);
void logMsgHex(const char* tag, const uint8_t* p, int n);
uint8_t calcChecksum(const std::string& payload);
std::string toHex2(const std::string& data);
int buildFrame(int id, unsigned char* out, int maxlen);
int buildFrameRA(int id, unsigned char* out, int maxlen);
void make_checksum_ascii(const uint8_t* data, int len, uint8_t out[3]);
// DLL Exports untuk serial functions
extern "C" {
    __declspec(dllexport) int __stdcall OpenSerial(const char* portName="COM1", unsigned long baudRate=115200);
    __declspec(dllexport) void __stdcall CloseSerial();
    __declspec(dllexport) const char* __stdcall sendDataDummy(const char* jsonStr);    
}
