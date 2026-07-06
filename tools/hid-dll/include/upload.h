#pragma once
#include "serialcom.h"
#include "uploadcallback.h"
extern "C" {
    __declspec(dllexport) const char* __stdcall handleUploadWithCallback(
    const char* jsonPayload,
    UploadCallbacks* callbacks);
}