// web_api.h
// Pure JSON-building functions for the Amidala web API.
//
// Header-only so they compile in both the firmware (Arduino String from the
// ESP32 framework) and native unit tests (String from arduino_mock.h).
//
// Nothing here depends on WiFi, WebServer, or SD — those live in wifi_ap.cpp.

#pragma once

#include "params.h"
#include "version.h"

// ---------------------------------------------------------------------------
// Hex string helper (8 uppercase hex digits, no prefix)
// ---------------------------------------------------------------------------
inline String hexStr(uint32_t v) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%08X", (unsigned int)v);
    return String(buf);
}

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
    json += "\"version\":\""   FIRMWARE_VERSION "\",";
    json += "\"board_rev\":\"" BOARD_REV        "\",";
    json += "\"mcu\":\""       MCU_VARIANT      "\",";
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
// GET /api/config  (all tuneable settings in one flat object)
// Every config sub-page fetches this single endpoint and reads the keys it
// needs — no per-page endpoints required.
// ---------------------------------------------------------------------------
inline String buildFullConfigJson(const AmidalaParameters& p) {
    const char* hw = (p.audiohw == AUDIO_HW_VMUSIC) ? "vmusic" : "hcr";
    String json = "{";

    // General
    json += "\"volume\":"        + String(p.volume)                   + ",";
    json += "\"startup\":\""     + String(p.startup     ? "y" : "n")  + "\",";
    json += "\"rndon\":\""       + String(p.rndon       ? "y" : "n")  + "\",";
    json += "\"mindelay\":"      + String(p.mindelay)                  + ",";
    json += "\"maxdelay\":"      + String(p.maxdelay)                  + ",";
    json += "\"ackon\":\""       + String(p.ackon       ? "y" : "n")  + "\",";
    json += "\"goslow\":\""      + String(p.goslow      ? "y" : "n")  + "\",";
    json += "\"mix12\":\""       + String(p.mix12       ? "y" : "n")  + "\",";
    json += "\"auto\":\""        + String(p.autocorrect ? "y" : "n")  + "\",";
    json += "\"serialbaud\":"    + String(p.serialbaud)                + ",";
    json += "\"serialdelim\":"   + String(p.serialdelim)               + ",";
    json += "\"serialeol\":"     + String(p.serialeol)                 + ",";
    json += "\"myi2c\":"         + String(p.myi2c)                    + ",";

    // WiFi
    json += "\"wifion\":\""       + String(p.wifion ? "y" : "n")      + "\",";
    json += "\"wifissid\":\""     + String(p.wifiSSID)                 + "\",";
    json += "\"wifipassword\":\"" + String(p.wifiPassword)             + "\",";

    // XBee
    json += "\"xbr\":\"" + hexStr(p.xbr) + "\",";
    json += "\"xbl\":\"" + hexStr(p.xbl) + "\",";

    // Audio
    json += "\"audiohw\":\""      + String(hw)                        + "\",";
    json += "\"volumeChA\":"      + String(p.volumeChA)               + ",";
    json += "\"volumeChB\":"      + String(p.volumeChB)               + ",";
    json += "\"volumewheel\":"    + String(p.volumewheel)             + ",";
    json += "\"altvolumewheel\":" + String(p.altvolumewheel)          + ",";
    json += "\"startupem\":"      + String(p.startupem)               + ",";
    json += "\"startuplvl\":"     + String(p.startuplvl)              + ",";
    json += "\"ackem\":"          + String(p.ackem)                   + ",";
    json += "\"acklvl\":"         + String(p.acklvl)                  + ",";

    // RC Radio
    json += "\"rcchn\":"   + String(p.rcchn)  + ",";
    json += "\"rcd\":"     + String(p.rcd)    + ",";
    json += "\"rcj\":"     + String(p.rcj)    + ",";
    json += "\"fst\":"     + String(p.fst)    + ",";
    json += "\"rvrmin\":"  + String(p.rvrmin) + ",";
    json += "\"rvrmax\":"  + String(p.rvrmax) + ",";
    json += "\"rvlmin\":"  + String(p.rvlmin) + ",";
    json += "\"rvlmax\":"  + String(p.rvlmax) + ",";
    json += "\"j1adjv\":"  + String(p.j1adjv) + ",";
    json += "\"j1adjh\":"  + String(p.j1adjh) + ",";

    // Dome
    json += "\"domespeed\":"     + String(p.domespeed)     + ",";
    json += "\"domespeedhome\":" + String(p.domespeedhome) + ",";
    json += "\"domespeedseek\":" + String(p.domespeedseek) + ",";
    json += "\"domespeedmin\":"  + String(p.domespeedmin)  + ",";
    json += "\"domedecelzone\":" + String(p.domedecelzone) + ",";
    json += "\"domehome\":"      + String(p.domehome)      + ",";
    json += "\"domeflip\":\""    + String(p.domeflip ? "y" : "n") + "\",";
    json += "\"domeimu\":\""     + String(p.domeimu  ? "y" : "n") + "\",";
    json += "\"domech6\":\""     + String(p.domech6  ? "y" : "n") + "\",";
    json += "\"domeseekl\":"     + String(p.domeseekl)     + ",";
    json += "\"domeseekr\":"     + String(p.domeseekr)     + ",";
    json += "\"domefudge\":"     + String(p.domefudge)     + ",";
    json += "\"domercaddr\":"    + String(p.domercaddr)    + ",";
    json += "\"domercchan\":"    + String(p.domercchan)    + ",";
    json += "\"domercqpps\":"    + String(p.domercqpps)    + ",";
    json += "\"domefront\":"     + String(p.domefront)     + ",";
    json += "\"domestall\":"     + String(p.domestall);

    json += "}";
    return json;
}
