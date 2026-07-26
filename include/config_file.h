// config_file.h
// SD card config.txt write-back helpers.
//
// Header-only so it compiles in both the firmware (SD.h from the ESP32 Arduino
// framework) and native unit tests (SD / File from test/arduino_mock.h).
//
// Nothing here depends on WiFi, WebServer, or controller state — all it needs
// is an SD global that provides open(path, mode) / remove(path) and a File
// type with available() / readStringUntil() / print() / close().

#pragma once

#include "params.h"

// ---------------------------------------------------------------------------
// updateConfigFile — update or insert a single key=value line in /config.txt.
//
// Behaviour:
//   - If the file does not exist, creates a minimal one: #START / key=value / #END
//   - If the file exists and contains an uncommented "key=" line, replaces it.
//   - If the key is not present, inserts key=value immediately before the
//     last "#END" marker (or appends if there is no #END).
//   - Lines starting with '#' or '//' are never treated as key matches.
//   - Windows-style \r\n endings are normalised (trailing \r stripped).
//
// Returns false only on SD write failure.
// ---------------------------------------------------------------------------
inline bool updateConfigFile(const char* key, const char* value) {
    String path    = "/config.txt";
    String prefix  = String(key) + "=";
    String newLine = prefix + String(value);

    File f = SD.open(path, "r");
    if (!f) {
        File wf = SD.open(path, "w");
        if (!wf) return false;
        wf.println("#START");
        wf.println(newLine);
        wf.println("#END");
        wf.close();
        return true;
    }

    String out;
    out.reserve(8192);
    bool found = false;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.endsWith("\r")) line.remove(line.length() - 1);

        if (!line.startsWith("#") && !line.startsWith("//") && line.startsWith(prefix)) {
            out += newLine + "\n";
            found = true;
        } else {
            out += line + "\n";
        }
    }
    f.close();

    if (!found) {
        int endIdx = out.lastIndexOf("#END");
        if (endIdx >= 0)
            out = out.substring(0, endIdx) + newLine + "\n" + out.substring(endIdx);
        else
            out += newLine + "\n";
    }

    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

// ---------------------------------------------------------------------------
// ensureConfigDefaults — append "key=value" to /config.txt for any scalar
// setting that isn't already present, using its current (already-defaulted)
// value from params.
//
// Unlike updateConfigFile(), this NEVER overwrites an existing key -- it only
// adds lines for keys that are completely absent, so a setting added after a
// user's config.txt was written self-heals into the file (instead of
// silently living only in an in-memory default forever, invisible to anyone
// reading the file) without ever touching a value the user deliberately set.
//
// Deliberately excludes repeatable/list-style keys (b=, lb=, ab=, db=, g=,
// sstr=, sb=, gadget=, cat=, estopstr=, resumestr=, f=) since "0 or more of
// these" has no single sensible default line to add, and domespmin=/
// domespmax=, whose config.txt parsing is currently commented out in
// AmidalaConfig::processConfig() (not a working option yet).
// Keep this list in sync with the scalar keys processConfig() understands.
//
// Returns false only on SD read/write failure; true (including when nothing
// was missing) otherwise.
// ---------------------------------------------------------------------------
inline bool ensureConfigDefaults(const AmidalaParameters& params) {
    struct DefaultableConfigKey {
        const char* key;  // includes trailing '='
        String value;
    };
    auto fmtBool = [](bool b) { return String(b ? "y" : "n"); };
    auto fmtHex2 = [](uint8_t v) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02X", v);
        return String(buf);
    };
    const DefaultableConfigKey keys[] = {
        {"volumeChA=",      String(params.volumeChA)},
        {"volumeChB=",      String(params.volumeChB)},
        {"auxserial3=",     fmtBool(params.auxserial3)},
        {"domedecelzone=",  String(params.domedecelzone)},
        {"domeimu=",        fmtBool(params.domeimu)},
        {"wifion=",         fmtBool(params.wifion)},
        {"wifissid=",       String(params.wifiSSID)},
        {"wifipassword=",   String(params.wifiPassword)},
        {"wifichannel=",    String(params.wifichannel)},
        {"btcontrolleron=", fmtBool(params.btcontrolleron)},
        {"btaddr=",         String(params.btaddr)},
        {"wcbenable=",      fmtBool(params.wcbenable)},
        {"wcboct2=",        fmtHex2(params.wcboct2)},
        {"wcboct3=",        fmtHex2(params.wcboct3)},
        {"wcbpassword=",    String(params.wcbpassword)},
        {"wcbquantity=",    String(params.wcbquantity)},
        {"wcbid=",          String(params.wcbid)},
        {"outboundserial=", String(params.outboundserial)},
        {"mutebutton=",     String(params.mutebutton)},
        {"b9=",             String(params.b9)},
        {"dbtimeout=",      String(params.dbtimeout)},
        {"pin1role=",       String(pinRoleToString(params.pinRole[0]))},
        {"pin2role=",       String(pinRoleToString(params.pinRole[1]))},
        {"pin3role=",       String(pinRoleToString(params.pinRole[2]))},
        {"pin4role=",       String(pinRoleToString(params.pinRole[3]))},
        {"pin5role=",       String(pinRoleToString(params.pinRole[4]))},
        {"pin6role=",       String(pinRoleToString(params.pinRole[5]))},
        {"pin39role=",      String(pinRoleToString(params.pinRole[6]))},
        {"pin40role=",      String(pinRoleToString(params.pinRole[7]))},
        {"pin41role=",      String(pinRoleToString(params.pinRole[8]))},
        {"pin42role=",      String(pinRoleToString(params.pinRole[9]))},
        {"pin47role=",      String(pinRoleToString(params.pinRole[10]))},
        {"domeserialport=",  String(serialPortToString(params.domeSerialPort))},
        {"driveserialport=", String(serialPortToString(params.driveSerialPort))},
    };
    constexpr unsigned kNumKeys = sizeof(keys) / sizeof(keys[0]);

    File f = SD.open("/config.txt", "r");
    if (!f) return false;

    bool present[kNumKeys] = {};
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.endsWith("\r")) line.remove(line.length() - 1);
        if (line.startsWith("#") || line.startsWith("//")) continue;
        for (unsigned i = 0; i < kNumKeys; i++) {
            if (!present[i] && line.startsWith(keys[i].key))
                present[i] = true;
        }
    }
    f.close();

    String toAdd;
    for (unsigned i = 0; i < kNumKeys; i++) {
        if (!present[i])
            toAdd += String(keys[i].key) + keys[i].value + "\n";
    }
    if (toAdd.length() == 0) return true;  // nothing missing

    File out = SD.open("/config.txt", "a");
    if (!out) return false;
    out.print("\n# --- Auto-added defaults for settings missing from this file ---\n");
    out.print(toAdd);
    out.close();
    return true;
}
