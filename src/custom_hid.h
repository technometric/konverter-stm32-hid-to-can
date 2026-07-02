#pragma once

#include <Arduino.h>

constexpr uint8_t CUSTOM_HID_REPORT_SIZE = 64;
constexpr uint8_t CUSTOM_HID_JSON_PAYLOAD_SIZE = 59;

void CustomHID_begin();
void CustomHID_task();
bool CustomHID_rxActive();
bool CustomHID_txActive();
