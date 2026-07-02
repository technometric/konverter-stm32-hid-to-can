#pragma once

#include <Arduino.h>

constexpr uint8_t CUSTOM_HID_REPORT_SIZE = 2;

void CustomHID_begin();
void CustomHID_task();
bool CustomHID_rxActive();
bool CustomHID_txActive();
