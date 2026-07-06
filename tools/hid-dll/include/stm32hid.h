#pragma once

#include <stdbool.h>
#include "cbs32.h"

#ifdef __cplusplus
extern "C" {
#endif

CBS_API bool CBS_CALL openStm32Converter();
CBS_API void CBS_CALL closeStm32Converter();
CBS_API const char* CBS_CALL stm32SendJson(const char* jsonText);
CBS_API const char* CBS_CALL stm32Ping();
CBS_API const char* CBS_CALL stm32GetSensor(int node, const char* sensor, int channel);
CBS_API const char* CBS_CALL stm32GetAll(int node);
CBS_API const char* CBS_CALL stm32ScanAll();

#ifdef __cplusplus
}
#endif
