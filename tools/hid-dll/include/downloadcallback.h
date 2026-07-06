#pragma once
#include <cstdint>
// Definisi tipe callback untuk berbagai event
typedef void(__stdcall* OnCommandSentCallback)(const char* command, const char* hexData);
typedef void(__stdcall* OnResponseReceivedCallback)(const char* command, const char* response);
typedef void(__stdcall* OnProgressCallback)(int current, int total, const char* status);
typedef void(__stdcall* OnErrorCallback)(const char* errorMsg);
typedef void(__stdcall* OnRetryCallback)(const char* command, int retryCount, int maxRetries);

// Struct untuk menampung semua callback
struct DownloadCallbacks {
    OnCommandSentCallback onCommandSent;
    OnResponseReceivedCallback onResponseReceived;
    OnProgressCallback onProgress;
    OnErrorCallback onError;
    OnRetryCallback onRetry;
}; 