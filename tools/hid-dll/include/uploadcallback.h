#pragma once
#include <cstdint>

// Callback definitions untuk upload event
typedef void(__stdcall* OnCommandSentCallback)(const char* command, const char* hexData);
typedef void(__stdcall* OnResponseReceivedCallback)(const char* command, const char* response);
typedef void(__stdcall* OnProgressCallback)(int current, int total, const char* status);
typedef void(__stdcall* OnErrorCallback)(const char* errorMsg);
typedef void(__stdcall* OnRetryCallback)(const char* command, int retryCount, int maxRetries);

struct UploadCallbacks {
    OnCommandSentCallback onCommandSent;
    OnResponseReceivedCallback onResponseReceived;
    OnProgressCallback onProgress;
    OnErrorCallback onError;
    OnRetryCallback onRetry;
};

