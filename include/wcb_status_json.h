// wcb_status_json.h
// Pure JSON builder for /api/wcb/status -- takes plain data, not a live
// WCB_Client&, so it's natively testable (unlike wcb_client_controller.h,
// this header has zero ESP32/WCB_Client/HCR dependencies).
// Deliberately does NOT #include <WString.h> (Arduino-core-only, not present
// under env:native) -- relies on String already being visible by the time
// this header is included, same as web_api.h's buildFullConfigJson().
#pragma once

#include <stdint.h>

inline String buildWCBStatusJson(bool enabled, bool configured, bool running, bool joined,
                                  uint8_t neighborCount, uint8_t deviceId, uint8_t quantity,
                                  bool rebootRequired) {
    String json = "{";
    json += "\"enabled\":";          json += enabled ? "true" : "false";
    json += ",\"configured\":";      json += configured ? "true" : "false";
    json += ",\"running\":";         json += running ? "true" : "false";
    json += ",\"joined\":";          json += joined ? "true" : "false";
    json += ",\"neighbor_count\":";  json += String(neighborCount);
    json += ",\"device_id\":";       json += String(deviceId);
    json += ",\"quantity\":";        json += String(quantity);
    json += ",\"reboot_required\":"; json += rebootRequired ? "true" : "false";
    json += "}";
    return json;
}
