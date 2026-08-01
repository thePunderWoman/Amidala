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
#include "pin_assignment.h"

// ---------------------------------------------------------------------------
// Hex string helper (8 uppercase hex digits, no prefix)
// ---------------------------------------------------------------------------
inline String hexStr(uint32_t v) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%08X", (unsigned int)v);
    return String(buf);
}

inline String hexStr(uint8_t v) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", (unsigned int)v);
    return String(buf);
}

// ---------------------------------------------------------------------------
// GET /api/info
// drive / dome / audio: compile-time strings; pass nullptr to emit JSON null.
// ssid / ip: runtime strings from params / WiFi.softAPIP().
// ---------------------------------------------------------------------------
inline String buildInfoJson(const char* drive, const char* dome,
                             const char* audio, const char* ssid,
                             const char* ip,
                             uint8_t sstrUsed = 0, uint32_t freeHeap = 0,
                             bool xbeeDrive = false, bool xbeeDome = false,
                             bool btConnected = false,
                             bool domeHomed = false, int domeDegrees = 0,
                             const char* serial1Role = nullptr,
                             const char* serial2Role = nullptr) {
    String driveVal = drive ? (String("\"") + drive + "\"") : String("null");
    String domeVal  = dome  ? (String("\"") + dome  + "\"") : String("null");
    String serial1Val = serial1Role ? (String("\"") + serial1Role + "\"") : String("null");
    String serial2Val = serial2Role ? (String("\"") + serial2Role + "\"") : String("null");

    String json = "{";
    json += "\"version\":\""   FIRMWARE_VERSION "\",";
    json += "\"board_rev\":\"" BOARD_REV        "\",";
    json += "\"mcu\":\""       MCU_VARIANT      "\",";
    json += "\"date\":\"" __DATE__ "\",";
    json += "\"drive\":"    + driveVal + ",";
    json += "\"dome\":"     + domeVal  + ",";
    json += "\"audio\":\""  + String(audio ? audio : "") + "\",";
    json += "\"wifi_ssid\":\"" + String(ssid ? ssid : "") + "\",";
    json += "\"wifi_ip\":\""   + String(ip   ? ip   : "") + "\",";
    json += "\"sstr_used\":"   + String(sstrUsed) + ",";
    json += "\"free_heap\":"   + String(freeHeap) + ",";
    json += "\"xbee_drive\":"  + String(xbeeDrive  ? "true" : "false") + ",";
    json += "\"xbee_dome\":"   + String(xbeeDome   ? "true" : "false") + ",";
    json += "\"bt_connected\":" + String(btConnected ? "true" : "false") + ",";
    json += "\"dome_homed\":"  + String(domeHomed  ? "true" : "false") + ",";
    json += "\"dome_degrees\":" + String(domeDegrees) + ",";
    // Which role (issue #147) is actually resolved onto each reassignable
    // physical port in the CURRENT config -- one of "roboclaw"/"roboteq"/
    // "sabertooth"/"aux"/"unused". Lets the Serial Monitor page (and any
    // other UI) adapt to reassignment instead of assuming a fixed
    // port-to-role mapping from the compiled drive/dome type alone.
    json += "\"serial1_role\":" + serial1Val + ",";
    json += "\"serial2_role\":" + serial2Val;
    json += "}";
    return json;
}

// ---------------------------------------------------------------------------
// buttonActionJson — serialize one ButtonAction to a compact JSON object.
// Only emits the fields that are relevant to the action type:
//   {t:TYPE}             for kNone / kHCRMuse
//   {t:5, x:ID}         for kSerialStr   (stable ID of serial string)
//   {t:7, x:EMO, y:LVL} for kHCREmote
//   {t:9, x:SUBCMD}     for kDomeCmd
// ---------------------------------------------------------------------------
inline String buttonActionJson(const ButtonAction& b) {
    String j = "{\"t\":";
    j += String(b.action);
    switch (b.action) {
    case ButtonAction::kSerialStr:
        j += ",\"x\":"; j += String(b.serialid);
        break;
    case ButtonAction::kHCREmote:
        j += ",\"x\":"; j += String(b.emote.emotion);
        j += ",\"y\":"; j += String(b.emote.level);
        break;
    case ButtonAction::kDomeCmd:
        j += ",\"x\":"; j += String(b.dome.subcmd);
        break;
    default: break;
    }
    j += "}";
    return j;
}

// ---------------------------------------------------------------------------
// GET /api/config  (all tuneable settings in one flat object)
// Every config sub-page fetches this single endpoint and reads the keys it
// needs — no per-page endpoints required.
// ---------------------------------------------------------------------------
inline String buildFullConfigJson(const AmidalaParameters& p) {
    const char* hw = (p.audiohw == AUDIO_HW_VMUSIC) ? "vmusic" : "hcr";
    String json = "{";
    // String::concat() reserves the buffer at the exact new length on every
    // += that outgrows capacity (no geometric growth -- see WString.cpp) --
    // building this via ~200+ unreserved += (many more with serial strings
    // configured, since Str[] can hold up to MAX_SERIAL_STRINGS entries)
    // means that many reallocs+copies per call, on an endpoint every config
    // page fetches on load. One upfront reserve() turns that into ~1.
    json.reserve(4096 + (size_t)p.serialcount * 170 +
                 (size_t)p.getServoCount() * 64 + (size_t)p.gcount * 64);

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

    // Aux serial
    json += "\"auxserial3\":\"" + String(p.auxserial3 ? "y" : "n") + "\",";

    // WiFi
    json += "\"wifion\":\""       + String(p.wifion ? "y" : "n")      + "\",";
    json += "\"wifissid\":\""     + String(p.wifiSSID)                 + "\",";
    json += "\"wifipassword\":\"" + String(p.wifiPassword)             + "\",";
    json += "\"wifichannel\":"    + String(p.wifichannel)              + ",";

    // XBee
    json += "\"xbr\":\"" + hexStr(p.xbr) + "\",";
    json += "\"xbl\":\"" + hexStr(p.xbl) + "\",";

    // Bluetooth
    json += "\"btcontrolleron\":\"" + String(p.btcontrolleron ? "y" : "n") + "\",";
    json += "\"btaddr\":\"" + String(p.btaddr) + "\",";

    // WCB Client
    json += "\"wcbenable\":\"" + String(p.wcbenable ? "y" : "n") + "\",";
    json += "\"wcboct2\":\"" + hexStr(p.wcboct2) + "\",";
    json += "\"wcboct3\":\"" + hexStr(p.wcboct3) + "\",";
    json += "\"wcbpassword\":\"" + String(p.wcbpassword) + "\",";
    json += "\"wcbquantity\":" + String(p.wcbquantity) + ",";
    json += "\"wcbid\":" + String(p.wcbid) + ",";
    json += "\"outboundserial\":" + String(p.outboundserial) + ",";

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
    json += "\"domestall\":"     + String(p.domestall)     + ",";
    json += "\"domeerrlog\":\""  + String(p.domeerrlog ? "y" : "n") + "\",";

    // Servos — abbreviated keys to save flash; "sp" = speed to distinguish
    // from sstr "s" (serial string value)
    json += "\"servos\":[";
    for (uint8_t i = 0; i < p.getServoCount(); i++) {
        if (i > 0) json += ",";
        json += "{\"min\":"; json += String(p.S[i].min);
        json += ",\"max\":"; json += String(p.S[i].max);
        json += ",\"n\":";   json += String(p.S[i].n);
        json += ",\"d\":";   json += String(p.S[i].d);
        json += ",\"t\":";   json += String(p.S[i].t);
        json += ",\"sp\":";  json += String(p.S[i].s);
        json += ",\"r\":";   json += String(p.S[i].r ? 1 : 0);
        json += "}";
    }
    json += "],";

    // Global servo pulse width limits
    json += "\"minpulse\":" + String(p.minpulse) + ",";
    json += "\"maxpulse\":" + String(p.maxpulse) + ",";

    // Dome hardware type (compile-time, exposed for UI gating)
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    json += "\"domehw\":\"roboclaw\",";
#elif DOME_DRIVE == DOME_DRIVE_SABER
    json += "\"domehw\":\"saber\",";
#elif DOME_DRIVE == DOME_DRIVE_PWM
    json += "\"domehw\":\"pwm\",";
#else
    json += "\"domehw\":\"analog\",";
#endif

    // Drive hardware type (compile-time, exposed for UI gating) -- mirrors
    // domehw above. Used by the Serial Ports config page to decide whether
    // the drive system consumes a serial port at all in this build.
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER
    json += "\"drivehw\":\"sabertooth\",";
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_PWM
    json += "\"drivehw\":\"pwm\",";
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM
    json += "\"drivehw\":\"roboteq-pwm\",";
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL
    json += "\"drivehw\":\"roboteq-serial\",";
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
    json += "\"drivehw\":\"roboteq-pwm-serial\",";
#endif

    // Sound banks — only meaningful for VMusic; always emitted so UI can gate on audiohw
    json += "\"sbs\":[";
    for (uint8_t i = 0; i < p.sbcount; i++) {
        if (i > 0) json += ",";
        json += "{\"dir\":\"";
        json += String(p.SB[i].dir);
        json += "\",\"n\":";
        json += String(p.SB[i].numfiles);
        json += ",\"r\":";
        json += p.SB[i].random ? "true" : "false";
        json += "}";
    }
    json += "],";

    // Serial strings — abbreviated keys {id, n, s} to save flash
    json += "\"sstr\":[";
    for (uint8_t i = 0; i < p.serialcount && i < MAX_SERIAL_STRINGS; i++) {
        if (i > 0) json += ",";
        json += "{\"id\":";
        json += String(p.Str[i].id);
        json += ",\"n\":\"";
        json += String(p.Str[i].name);
        json += "\",\"s\":\"";
        json += String(p.Str[i].str);
        json += "\"}";
    }
    json += "],";

    // Favorites list — 1-based sstr indices
    json += "\"sstr_favs\":[";
    for (uint8_t i = 0; i < p.sstr_fav_cnt; i++) {
        if (i > 0) json += ",";
        json += String(p.sstr_favs[i]);
    }
    json += "],";

    // Hidden list — 1-based sstr indices hidden from Droid Control
    json += "\"sstr_hidden\":[";
    for (uint8_t i = 0; i < p.sstr_hidden_cnt; i++) {
        if (i > 0) json += ",";
        json += String(p.sstr_hidden[i]);
    }
    json += "],";

    // Categories — [{name, idx:[...]}]
    json += "\"sstr_cats\":[";
    for (uint8_t i = 0; i < p.sstr_cat_count; i++) {
        if (i > 0) json += ",";
        json += "{\"name\":\"";
        json += String(p.sstr_cats[i].name);
        json += "\",\"idx\":[";
        for (uint8_t j = 0; j < p.sstr_cats[i].cnt; j++) {
            if (j > 0) json += ",";
            json += String(p.sstr_cats[i].idx[j]);
        }
        json += "]}";
    }
    json += "],";

    // Controller settings
    json += "\"altbtn\":"       + String(p.altbtn)       + ",";
    json += "\"mutebutton\":"   + String(p.mutebutton)   + ",";
    json += "\"altdomestick\":" + String(p.altdomestick) + ",";
    json += "\"dbtimeout\":"    + String(p.dbtimeout)    + ",";

    // Button assignments — 9 buttons × 4 layers {p=press, l=long, a=alt, d=double}
    json += "\"buttons\":[";
    for (unsigned i = 0; i < sizeof(p.B) / sizeof(p.B[0]); i++) {
        if (i > 0) json += ",";
        json += "{\"p\":"; json += buttonActionJson(p.B[i]);
        json += ",\"l\":"; json += buttonActionJson(p.LB[i]);
        json += ",\"a\":"; json += buttonActionJson(p.AB[i]);
        json += ",\"d\":"; json += buttonActionJson(p.DB[i]);
        json += "}";
    }
    json += "],";

    // Safety broadcast commands
    json += "\"estop_cmds\":[";
    for (uint8_t i = 0; i < p.estopCmdCount; i++) {
        if (i > 0) json += ",";
        json += "\"";
        json += String(p.EstopCmds[i].str);
        json += "\"";
    }
    json += "],";
    json += "\"resume_cmds\":[";
    for (uint8_t i = 0; i < p.resumeCmdCount; i++) {
        if (i > 0) json += ",";
        json += "\"";
        json += String(p.ResumeCmds[i].str);
        json += "\"";
    }
    json += "],";

    // Gesture assignments
    json += "\"gestures\":[";
    char gseq[MAX_GESTURE_LENGTH + 1];
    for (uint8_t gi = 0; gi < p.gcount; gi++) {
        if (gi > 0) json += ",";
        p.G[gi].gesture.getGestureString(gseq);
        json += "{\"seq\":\""; json += String(gseq); json += "\"";
        json += ",\"t\":";     json += String(p.G[gi].action.action);
        switch (p.G[gi].action.action) {
        case ButtonAction::kSerialStr:
            json += ",\"x\":"; json += String(p.G[gi].action.serialid);
            break;
        case ButtonAction::kHCREmote:
            json += ",\"x\":"; json += String(p.G[gi].action.emote.emotion);
            json += ",\"y\":"; json += String(p.G[gi].action.emote.level);
            break;
        case ButtonAction::kDomeCmd:
            json += ",\"x\":"; json += String(p.G[gi].action.dome.subcmd);
            break;
        default: break;
        }
        json += "}";
    }
    json += "]";

    // Reassignable GPIO pin roles (issue #133). assignablePins is the single
    // source of truth for the pool of physical pins; pinRoles is the
    // current role (string-encoded, see pin_assignment.h's
    // pinRoleToString()) of each, same order/indexing as assignablePins --
    // drives the Pins config page's per-pin dropdowns. doutPins/analogPins
    // are the derived, ordered GPIO lists for however many pins currently
    // have that role -- the Droid Status page's row count tracks live
    // reassignment rather than a fixed 4/2.
    json += ",\"assignablePins\":[";
    for (uint8_t i = 0; i < sizeof(kAssignablePins) / sizeof(kAssignablePins[0]); i++) {
        if (i > 0) json += ",";
        json += String(kAssignablePins[i]);
    }
    json += "],\"pinRoles\":[";
    for (uint8_t i = 0; i < 11; i++) {
        if (i > 0) json += ",";
        json += "\""; json += String(pinRoleToString(p.pinRole[i])); json += "\"";
    }
    json += "],\"doutPins\":[";
    {
        uint8_t doutCount = countPinsWithRole(p.pinRole, PinRoleType::kDout);
        for (uint8_t i = 0; i < doutCount; i++) {
            if (i > 0) json += ",";
            json += String(nthPinWithRole(p.pinRole, PinRoleType::kDout, i));
        }
    }
    json += "],\"analogPins\":[";
    {
        uint8_t analogCount = countPinsWithRole(p.pinRole, PinRoleType::kAnalog);
        for (uint8_t i = 0; i < analogCount; i++) {
            if (i > 0) json += ",";
            json += String(nthPinWithRole(p.pinRole, PinRoleType::kAnalog, i));
        }
    }
    json += "]";

    // Reassignable dome/drive serial ports (issue #147). Only meaningful
    // when domehw/drivehw (above) indicate this build's dome/drive actually
    // consumes a serial port -- always emitted regardless so config.txt
    // stays portable across builds (same reasoning as pinRoles above).
    json += ",\"domeserialport\":\"";
    json += String(serialPortToString(p.domeSerialPort));
    json += "\",\"driveserialport\":\"";
    json += String(serialPortToString(p.driveSerialPort));
    json += "\"";

    json += "}";
    return json;
}

// Overload that appends gadget configuration (JSON array string built by caller).
inline String buildFullConfigJson(const AmidalaParameters& p, const String& gadgetsCfgJson) {
    String j = buildFullConfigJson(p);
    j.reserve(j.length() + gadgetsCfgJson.length() + 20);
    j = j.substring(0, j.length() - 1);
    j += ",\"gadgets_cfg\":";
    j += gadgetsCfgJson;
    j += "}";
    return j;
}

// Overload that also includes sstr_user_cnt — the number of user-defined serial
// strings, excluding any builtin strings injected at startup.  The serial-strings
// config page uses this to avoid showing or editing builtin commands.
inline String buildFullConfigJson(const AmidalaParameters& p, const String& gadgetsCfgJson,
                                   uint8_t userSstrCnt) {
    String j = buildFullConfigJson(p);
    j.reserve(j.length() + gadgetsCfgJson.length() + 40);
    j = j.substring(0, j.length() - 1);
    j += ",\"gadgets_cfg\":";
    j += gadgetsCfgJson;
    j += ",\"sstr_user_cnt\":";
    j += String(userSstrCnt);
    j += "}";
    return j;
}
