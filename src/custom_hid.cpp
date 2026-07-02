#include "custom_hid.h"

#include <string.h>
#include "usbd_core.h"
#include "usbd_customhid.h"
#include "usbd_desc.h"

extern "C" USBD_HandleTypeDef hUSBD_Device_CustomHID;

namespace {

constexpr uint8_t REPORT_TYPE_JSON = 0x01;
constexpr size_t JSON_BUFFER_SIZE = 640;
constexpr uint16_t LED_PULSE_MS = 80;
constexpr uint16_t TX_SPACING_MS = 2;

volatile bool rxReportAvailable = false;
uint8_t rxReport[CUSTOM_HID_REPORT_SIZE] = {};

char rxJson[JSON_BUFFER_SIZE] = {};
size_t rxJsonLen = 0;
uint8_t activeSeq = 0;
uint8_t expectedChunks = 0;
uint8_t receivedChunks = 0;

char txJson[JSON_BUFFER_SIZE] = {};
size_t txJsonLen = 0;
size_t txJsonOffset = 0;
uint8_t txSeq = 0;
uint8_t txChunkIndex = 0;
uint8_t txTotalChunks = 0;
bool txPending = false;

uint32_t rxPulseUntil = 0;
uint32_t txPulseUntil = 0;
uint32_t nextTxAllowedMs = 0;

int findJsonInt(const char *json, const char *key, int fallback)
{
  const char *pos = strstr(json, key);
  if (pos == nullptr) return fallback;
  pos = strchr(pos, ':');
  if (pos == nullptr) return fallback;
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

void appendScaled(char *buffer, size_t bufferSize, size_t &offset, int32_t scaled, uint16_t scale)
{
  const int32_t whole = scaled / scale;
  const int32_t fraction = scaled % scale;
  offset += snprintf(buffer + offset, bufferSize - offset, "%ld.", static_cast<long>(whole));
  int32_t divisor = scale / 10;
  while (divisor > 1) {
    if (fraction < divisor) offset += snprintf(buffer + offset, bufferSize - offset, "0");
    divisor /= 10;
  }
  offset += snprintf(buffer + offset, bufferSize - offset, "%ld", static_cast<long>(fraction));
}

void scaledToText(char *buffer, size_t bufferSize, int32_t scaled, uint16_t scale)
{
  size_t offset = 0;
  appendScaled(buffer, bufferSize, offset, scaled, scale);
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
  txChunkIndex = 0;
  txTotalChunks = max<uint8_t>(1, (txJsonLen + CUSTOM_HID_JSON_PAYLOAD_SIZE - 1) / CUSTOM_HID_JSON_PAYLOAD_SIZE);
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
    snprintf(out, sizeof(out), "{\"seq\":%u,\"ok\":true,\"device\":\"nucleo_f446re_hid_can_dummy64\",\"nodes\":48}", seq);
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
      appendScaled(pt100, sizeof(pt100), off, dummyValueScaled(node, "pt100", ch, 100), 100);
    }

    char flow[16], steam[16], ph[16], kwh[20], turbidity[16];
    scaledToText(flow, sizeof(flow), dummyValueScaled(node, "flow", 0, 100), 100);
    scaledToText(steam, sizeof(steam), dummyValueScaled(node, "steam", 0, 100), 100);
    scaledToText(ph, sizeof(ph), dummyValueScaled(node, "ph", 0, 100), 100);
    scaledToText(kwh, sizeof(kwh), dummyValueScaled(node, "kwh", 0, 1000), 1000);
    scaledToText(turbidity, sizeof(turbidity), dummyValueScaled(node, "turbidity", 0, 100), 100);

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
    const uint16_t scale = strcmp(sensor, "kwh") == 0 ? 1000 : 100;
    char value[20];
    scaledToText(value, sizeof(value), dummyValueScaled(node, sensor, channel, scale), scale);

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

void handleRxReport(const uint8_t *report)
{
  if (report[0] != REPORT_TYPE_JSON) return;

  const uint8_t seq = report[1];
  const uint8_t chunkIndex = report[2];
  const uint8_t totalChunks = report[3];
  const uint8_t payloadLen = report[4];
  if (payloadLen > CUSTOM_HID_JSON_PAYLOAD_SIZE || totalChunks == 0) return;

  if (chunkIndex == 0 || seq != activeSeq) {
    rxJsonLen = 0;
    activeSeq = seq;
    expectedChunks = totalChunks;
    receivedChunks = 0;
  }

  if (seq != activeSeq || totalChunks != expectedChunks || chunkIndex != receivedChunks) {
    rxJsonLen = 0;
    receivedChunks = 0;
    return;
  }

  if (rxJsonLen + payloadLen >= sizeof(rxJson)) {
    rxJsonLen = 0;
    receivedChunks = 0;
    return;
  }

  memcpy(rxJson + rxJsonLen, report + 5, payloadLen);
  rxJsonLen += payloadLen;
  rxJson[rxJsonLen] = '\0';
  receivedChunks++;
  rxPulseUntil = millis() + LED_PULSE_MS;

  if (receivedChunks >= expectedChunks) {
    processJsonCommand(rxJson);
    rxJsonLen = 0;
    receivedChunks = 0;
  }
}

bool sendReport(const uint8_t *report)
{
  if (millis() < nextTxAllowedMs) return false;
  const uint8_t status = USBD_CUSTOM_HID_SendReport(&hUSBD_Device_CustomHID, const_cast<uint8_t *>(report), CUSTOM_HID_REPORT_SIZE);
  if (status == USBD_OK) {
    txPulseUntil = millis() + LED_PULSE_MS;
    nextTxAllowedMs = millis() + TX_SPACING_MS;
    return true;
  }
  return false;
}

void sendNextTxChunk()
{
  if (!txPending) return;

  uint8_t report[CUSTOM_HID_REPORT_SIZE] = {};
  const size_t remaining = txJsonLen - txJsonOffset;
  const uint8_t payloadLen = static_cast<uint8_t>(min(static_cast<size_t>(CUSTOM_HID_JSON_PAYLOAD_SIZE), remaining));

  report[0] = REPORT_TYPE_JSON;
  report[1] = txSeq;
  report[2] = txChunkIndex;
  report[3] = txTotalChunks;
  report[4] = payloadLen;
  memcpy(report + 5, txJson + txJsonOffset, payloadLen);

  if (!sendReport(report)) return;

  txJsonOffset += payloadLen;
  txChunkIndex++;
  if (txChunkIndex >= txTotalChunks) {
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
  0x95, 0x40,
  0x09, 0x01,
  0x81, 0x02,
  0x95, 0x40,
  0x09, 0x01,
  0x91, 0x02,
  0xC0
};

static int8_t customHidInit() { return USBD_OK; }
static int8_t customHidDeInit() { return USBD_OK; }

static int8_t customHidOutEvent(uint8_t, uint8_t)
{
  USBD_CUSTOM_HID_HandleTypeDef *hid =
      reinterpret_cast<USBD_CUSTOM_HID_HandleTypeDef *>(hUSBD_Device_CustomHID.pClassDataCmsit[hUSBD_Device_CustomHID.classId]);
  if (hid != nullptr) {
    memcpy(rxReport, hid->Report_buf, sizeof(rxReport));
    rxReportAvailable = true;
  }
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
  if (customHidInitialized) return;

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
    uint8_t report[CUSTOM_HID_REPORT_SIZE];
    noInterrupts();
    memcpy(report, rxReport, sizeof(report));
    rxReportAvailable = false;
    interrupts();
    handleRxReport(report);
  }

  sendNextTxChunk();
}

bool CustomHID_rxActive()
{
  return millis() < rxPulseUntil;
}

bool CustomHID_txActive()
{
  return millis() < txPulseUntil;
}
