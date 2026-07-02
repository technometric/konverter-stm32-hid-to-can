#include <Arduino.h>
#include "custom_hid.h"

constexpr uint8_t LED_RX_PIN = LED_BUILTIN;
constexpr uint8_t LED_TX_PIN = PB2;

void setup() {
  pinMode(LED_RX_PIN, OUTPUT);
  pinMode(LED_TX_PIN, OUTPUT);
  CustomHID_begin();
}

void loop() {
  CustomHID_task();
  digitalWrite(LED_RX_PIN, CustomHID_rxActive() ? HIGH : LOW);
  digitalWrite(LED_TX_PIN, CustomHID_txActive() ? HIGH : LOW);
}
