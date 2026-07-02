#include "custom_hid.h"

#include <string.h>
#include "usbd_core.h"
#include "usbd_customhid.h"
#include "usbd_desc.h"

extern "C" USBD_HandleTypeDef hUSBD_Device_CustomHID;

namespace {

constexpr uint8_t OUT_START = 0x10;
constexpr uint8_t OUT_DATA = 0x11;
constexpr uint8_t OUT_END = 0x12;
constexpr uint8_t IN_START = 0x20;
constexpr uint8_t IN_DATA = 0x21;
constexpr uint8_t IN_END = 0x22;
constexpr size_t JSON_BUFFER_SIZE = 640;
constexpr uint16_t LED_PULSE_MS = 80;

volatile bool rxReportAvailable = false;
volatile uint8_t rxReport0 = 0;
volatile uint8_t rxReport1 = 0;

char rxJson[JSON_BUFFER_SIZE] = {};
size_t rxJsonLen = 0;
uint8_t activeSeq = 0;
bool rxStreaming = false;

char txJson[JSON_BUFFER_SIZE] = {};
size_t txJsonLen = 0;
size_t txJsonOffset = 0;
uint8_t txSeq = 0;
uint8_t txPhase = 0;
bool txPending = false;

uint32_t rxPulseUntil = 0;
uint32_t txPulseUntil = 0;
uint32_t nextTxAllowedMs = 0;

int findJsonInt(const char *json, const char *key, int fallback)
{
  const char *pos = strstr(json, key);
  if (pos == nullptr) {
    return fallback;
  }
  pos = strchr(pos, ':');
  if (pos == nullptr) {
    return fallback;
  }
  return atoi(pos + 1);
}

bool hasText(const char *json, const char *text)
{
  return strstr(json, text) != nullptr;
}

const char *sensorUnit(const char *sensor)
{
  if (strcmp(sensor, "flow") == 0) return "L/min";
  if (strcmp(sensor, "steam") == 0) return "kg/h";
  if (strcmp(sensor, "ph") == 0) return "pH";
  if (strcmp(sensor, "kwh") == 0) return "kWh";
  if (strcmp(sensor, "turbidity") == 0) return "NTU";
  if (strcmp(sensor, "pt100") == 0) return "C";
  return "";
}

float dummyValue(uint8_t node, const char *sensor, uint8_t channel)
{
  if (strcmp(sensor, "flow") == 0) return 10.0f + node * 0.25f;
  if (strcmp(sensor, "steam") == 0) return 40.0f + node * 0.5f;
  if (strcmp(sensor, "ph") == 0) return 6.80f + (node % 12) * 0.03f;
  if (strcmp(sensor, "kwh") == 0) return 1200.0f + node * 17.35f;
  if (strcmp(sensor, "turbidity") == 0) return 3.5f + node * 0.12f;
  if (strcmp(sensor, "pt100") == 0) return 30.0f + node * 0.10f + channel * 0.35f;
  return 0.0f;
}

int32_t dummyValueScaled(uint8_t node, const char *sensor, uint8_t channel, uint16_t scale)
{
  return static_cast<int32_t>(dummyValue(node, sensor, channel) * scale + 0.5f);
}

void appendScaled(char *buffer, size_t bufferSize, size_t &offset, int32_t scaled, uint16_t scale, uint8_t decimals)
{
  const int32_t whole = scaled / scale;
  const int32_t fraction = scaled % scale;
  offset += snprintf(buffer + offset, bufferSize - offset, "%ld.", static_cast<long>(whole));
  int32_t divisor = scale / 10;
  while (divisor > 1) {
    if (fraction < divisor) {
      offset += snprintf(buffer + offset, bufferSize - offset, "0");
    }
    divisor /= 10;
  }
  offset += snprintf(buffer + offset, bufferSize - offset, "%ld", static_cast<long>(fraction));
}

void scaledToText(char *buffer, size_t bufferSize, int32_t scaled, uint16_t scale, uint8_t decimals)
{
  size_t offset = 0;
  appendScaled(buffer, bufferSize, offset, scaled, scale, decimals);
}

void extractSensor(const char *json, char *sensor, size_t sensorSize)
{
  const char *key = strstr(json, "\"sensor\"");
  const char *colon = key ? strchr(key, ':') : nullptr;
  const char *start = colon ? strchr(colon, '"') : nullptr;
  if (start == nullptr) {
    strncpy(sensor, "flow", sensorSize);
    sensor[sensorSize - 1] = '\0';
    return;
  }
  start++;
  const char *end = strchr(start, '"');
  if (end == nullptr) {
    strncpy(sensor, "flow", sensorSize);
    sensor[sensorSize - 1] = '\0';
    return;
  }
  const size_t len = min(sensorSize - 1, static_cast<size_t>(end - start));
  memcpy(sensor, start, len);
  sensor[len] = '\0';
}

void queueJson(uint8_t seq, const char *json)
{
  strncpy(txJson, json, sizeof(txJson) - 1);
  txJson[sizeof(txJson) - 1] = '\0';
  txJsonLen = strlen(txJson);
  txJsonOffset = 0;
  txSeq = seq;
  txPhase = 0;
  txPending = true;
}

void processJsonCommand(const char *json)
{
  const uint8_t seq = static_cast<uint8_t>(findJsonInt(json, "\"seq\"", activeSeq));
  const int node = findJsonInt(json, "\"node\"", 1);
  if (node < 1 || node > 48) {
    char out[120];
    snprintf(out, sizeof(out), "{\"seq\":%u,\"ok\":false,\"err\":\"node_out_of_range\"}", seq);
    queueJson(seq, out);
    return;
  }

  if (hasText(json, "\"cmd\":\"ping\"")) {
    char out[120];
    snprintf(out, sizeof(out), "{\"seq\":%u,\"ok\":true,\"device\":\"nucleo_f446re_hid_can_dummy\",\"nodes\":48}", seq);
    queueJson(seq, out);
    return;
  }

  if (hasText(json, "\"cmd\":\"scan_all\"")) {
    char out[320];
    char nodes[170] = {};
    size_t off = 0;
    for (uint8_t i = 1; i <= 48; i++) {
      off += snprintf(nodes + off, sizeof(nodes) - off, "%s%u", i == 1 ? "" : ",", i);
    }
    snprintf(out, sizeof(out), "{\"seq\":%u,\"ok\":true,\"nodes\":[%s]}", seq, nodes);
    queueJson(seq, out);
    return;
  }

  if (hasText(json, "\"cmd\":\"get_all\"")) {
    char out[480];
    char pt100[120] = {};
    size_t off = 0;
    for (uint8_t ch = 1; ch <= 8; ch++) {
      off += snprintf(pt100 + off, sizeof(pt100) - off, "%s", ch == 1 ? "" : ",");
      appendScaled(pt100, sizeof(pt100), off, dummyValueScaled(node, "pt100", ch, 100), 100, 2);
    }

    char flow[16], steam[16], ph[16], kwh[20], turbidity[16];
    scaledToText(flow, sizeof(flow), dummyValueScaled(node, "flow", 0, 100), 100, 2);
    scaledToText(steam, sizeof(steam), dummyValueScaled(node, "steam", 0, 100), 100, 2);
    scaledToText(ph, sizeof(ph), dummyValueScaled(node, "ph", 0, 100), 100, 2);
    scaledToText(kwh, sizeof(kwh), dummyValueScaled(node, "kwh", 0, 1000), 1000, 3);
    scaledToText(turbidity, sizeof(turbidity), dummyValueScaled(node, "turbidity", 0, 100), 100, 2);

    snprintf(out, sizeof(out),
             "{\"seq\":%u,\"ok\":true,\"node\":%d,\"data\":{\"flow\":%s,\"steam\":%s,\"ph\":%s,\"kwh\":%s,\"turbidity\":%s,\"pt100\":[%s]}}",
             seq, node, flow, steam, ph, kwh, turbidity, pt100);
    queueJson(seq, out);
    return;
  }

  if (hasText(json, "\"cmd\":\"get\"")) {
    char sensor[20];
    extractSensor(json, sensor, sizeof(sensor));
    const uint8_t channel = static_cast<uint8_t>(findJsonInt(json, "\"ch\"", 1));
    const bool threeDecimals = strcmp(sensor, "kwh") == 0;
    const uint16_t scale = threeDecimals ? 1000 : 100;
    const uint8_t decimals = threeDecimals ? 3 : 2;
    char value[20];
    scaledToText(value, sizeof(value), dummyValueScaled(node, sensor, channel, scale), scale, decimals);

    char out[220];
    snprintf(out, sizeof(out),
             "{\"seq\":%u,\"ok\":true,\"node\":%d,\"sensor\":\"%s\",\"ch\":%u,\"value\":%s,\"unit\":\"%s\"}",
             seq, node, sensor, channel, value, sensorUnit(sensor));
    queueJson(seq, out);
    return;
  }

  char out[100];
  snprintf(out, sizeof(out), "{\"seq\":%u,\"ok\":false,\"err\":\"unknown_cmd\"}", seq);
  queueJson(seq, out);
}

void handleRxReport(uint8_t cmd, uint8_t value)
{
  if (cmd == OUT_START) {
    activeSeq = value;
    rxJsonLen = 0;
    rxStreaming = true;
    rxPulseUntil = millis() + LED_PULSE_MS;
    return;
  }

  if (!rxStreaming) {
    return;
  }

  if (cmd == OUT_DATA) {
    if (rxJsonLen + 1 < sizeof(rxJson)) {
      rxJson[rxJsonLen++] = static_cast<char>(value);
      rxJson[rxJsonLen] = '\0';
    }
    rxPulseUntil = millis() + LED_PULSE_MS;
    return;
  }

  if (cmd == OUT_END) {
    rxStreaming = false;
    rxPulseUntil = millis() + LED_PULSE_MS;
    processJsonCommand(rxJson);
  }
}

bool sendReport(uint8_t cmd, uint8_t value)
{
  if (millis() < nextTxAllowedMs) {
    return false;
  }

  uint8_t report[CUSTOM_HID_REPORT_SIZE] = {cmd, value};
  const uint8_t status = USBD_CUSTOM_HID_SendReport(&hUSBD_Device_CustomHID, report, sizeof(report));
  if (status == USBD_OK) {
    txPulseUntil = millis() + LED_PULSE_MS;
    nextTxAllowedMs = millis() + 2;
    return true;
  }
  return false;
}

void sendNextTxReport()
{
  if (!txPending) {
    return;
  }

  if (txPhase == 0) {
    if (sendReport(IN_START, txSeq)) {
      txPhase = 1;
    }
    return;
  }

  if (txJsonOffset < txJsonLen) {
    if (sendReport(IN_DATA, static_cast<uint8_t>(txJson[txJsonOffset]))) {
      txJsonOffset++;
    }
    return;
  }

  if (sendReport(IN_END, 0)) {
    txPending = false;
  }
}

}

extern "C" {

USBD_HandleTypeDef hUSBD_Device_CustomHID;
static bool customHidInitialized = false;

__ALIGN_BEGIN static uint8_t customHidReportDescriptor[USBD_CUSTOM_HID_REPORT_DESC_SIZE] __ALIGN_END = {
  0x06, 0x00, 0xFF,
  0x09, 0x01,
  0xA1, 0x01,
  0x15, 0x00,
  0x26, 0xFF, 0x00,
  0x75, 0x08,
  0x95, 0x02,
  0x09, 0x01,
  0x81, 0x02,
  0x95, 0x02,
  0x09, 0x01,
  0x91, 0x02,
  0xC0
};

static int8_t customHidInit() { return USBD_OK; }
static int8_t customHidDeInit() { return USBD_OK; }

static int8_t customHidOutEvent(uint8_t cmd, uint8_t value)
{
  rxReport0 = cmd;
  rxReport1 = value;
  rxReportAvailable = true;
  USBD_CUSTOM_HID_ReceivePacket(&hUSBD_Device_CustomHID);
  return USBD_OK;
}

static USBD_CUSTOM_HID_ItfTypeDef customHidInterface = {
  customHidReportDescriptor,
  customHidInit,
  customHidDeInit,
  customHidOutEvent
};

}

void CustomHID_begin()
{
  if (customHidInitialized) {
    return;
  }

  delay(500);

  if (USBD_Init(&hUSBD_Device_CustomHID, &USBD_Desc, 0) != USBD_OK) return;
  if (USBD_RegisterClass(&hUSBD_Device_CustomHID, USBD_CUSTOM_HID_CLASS) != USBD_OK) return;
  if (USBD_CUSTOM_HID_RegisterInterface(&hUSBD_Device_CustomHID, &customHidInterface) != USBD_OK) return;
  if (USBD_Start(&hUSBD_Device_CustomHID) != USBD_OK) return;

  customHidInitialized = true;
}

void CustomHID_task()
{
  if (rxReportAvailable) {
    noInterrupts();
    const uint8_t cmd = rxReport0;
    const uint8_t value = rxReport1;
    rxReportAvailable = false;
    interrupts();
    handleRxReport(cmd, value);
  }

  sendNextTxReport();
}

bool CustomHID_rxActive()
{
  return millis() < rxPulseUntil;
}

bool CustomHID_txActive()
{
  return millis() < txPulseUntil;
}
