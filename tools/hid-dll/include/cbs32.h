#pragma once
#include <stdbool.h>
#include "mode.h"
#ifdef __cplusplus
extern "C" {
#endif
 
#ifdef _WIN32
  #define CBS_API __declspec(dllexport)
  #define CBS_CALL __stdcall
#else
  #define CBS_API
  #define CBS_CALL
#endif

#ifndef HID_MODE
CBS_API const char* CBS_CALL getTest(int id);
CBS_API void CBS_CALL freeCString(const char* p);
CBS_API const char* CBS_CALL getMonitoring(int id);
CBS_API int CBS_CALL handleAksi(int id, const char* aksi);
#endif
CBS_API const char* CBS_CALL getMonitoring(int id);
CBS_API const char* CBS_CALL readMonitoring(int id);
CBS_API const char* CBS_CALL getVersion();
CBS_API const char* CBS_CALL handleAksi(int id, const char* aksi);
CBS_API bool CBS_CALL openStm32Converter();
CBS_API void CBS_CALL closeStm32Converter();
CBS_API const char* CBS_CALL stm32SendJson(const char* jsonText);
CBS_API const char* CBS_CALL stm32Ping();
CBS_API const char* CBS_CALL stm32GetSensor(int node, const char* sensor, int channel);
CBS_API const char* CBS_CALL stm32GetAll(int node);
CBS_API const char* CBS_CALL stm32ScanAll();

#ifdef __cplusplus
} // extern "C"
#endif

