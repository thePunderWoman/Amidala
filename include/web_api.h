// web_api.h
// Pure JSON-building functions for the Amidala web API.
//
// Header-only so they compile in both the firmware (Arduino String from the
// ESP32 framework) and native unit tests (String from arduino_mock.h).
//
// Nothing here depends on WiFi, WebServer, or SD — those live in wifi_ap.cpp.

#pragma once

#include "params.h"

// ---------------------------------------------------------------------------
// GET /api/info
// drive / dome / audio: compile-time strings; pass nullptr to emit JSON null.
// ssid / ip: runtime strings from params / WiFi.softAPIP().
// ---------------------------------------------------------------------------
inline String buildInfoJson(const char* drive, const char* dome,
                             const char* audio, const char* ssid,
                             const char* ip) {
    String driveVal = drive ? (String("\"") + drive + "\"") : String("null");
    String domeVal  = dome  ? (String("\"") + dome  + "\"") : String("null");

    String json = "{";
    json += "\"firmware\":\"Amidala RC\",";
    json += "\"version\":\"1.3\",";
    json += "\"date\":\"" __DATE__ "\",";
    json += "\"drive\":"    + driveVal + ",";
    json += "\"dome\":"     + domeVal  + ",";
    json += "\"audio\":\""  + String(audio ? audio : "") + "\",";
    json += "\"wifi_ssid\":\"" + String(ssid ? ssid : "") + "\",";
    json += "\"wifi_ip\":\""   + String(ip   ? ip   : "") + "\"";
    json += "}";
    return json;
}

// ---------------------------------------------------------------------------
// GET /api/config/general
// ---------------------------------------------------------------------------
inline String buildGeneralConfigJson(const AmidalaParameters& p) {
    String json = "{";
    json += "\"volume\":"      + String(p.volume)                  + ",";
    json += "\"startup\":\""   + String(p.startup     ? "y" : "n") + "\",";
    json += "\"rndon\":\""     + String(p.rndon       ? "y" : "n") + "\",";
    json += "\"mindelay\":"    + String(p.mindelay)                + ",";
    json += "\"maxdelay\":"    + String(p.maxdelay)                + ",";
    json += "\"ackon\":\""     + String(p.ackon       ? "y" : "n") + "\",";
    json += "\"goslow\":\""    + String(p.goslow      ? "y" : "n") + "\",";
    json += "\"mix12\":\""     + String(p.mix12       ? "y" : "n") + "\",";
    json += "\"auto\":\""      + String(p.autocorrect ? "y" : "n") + "\",";
    json += "\"serialbaud\":"  + String(p.serialbaud)              + ",";
    json += "\"serialdelim\":" + String(p.serialdelim)             + ",";
    json += "\"serialeol\":"   + String(p.serialeol)               + ",";
    json += "\"myi2c\":"       + String(p.myi2c);
    json += "}";
    return json;
}
