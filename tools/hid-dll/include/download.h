#pragma once
#include "downloadcallback.h"
#include "serialcom.h"

extern "C" {
    __declspec(dllexport) int __stdcall handleDownloadWithCallback(
        const char* jsonPayload,
        DownloadCallbacks* cb, bool test = false);
}
