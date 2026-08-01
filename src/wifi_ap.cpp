#include "wifi_ap.h"
#include "web_pages.h"

#ifndef UNIT_TEST
#include <EEPROM.h>     // must precede params.h (via web_api.h)
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "SD.h"
#include "web_api.h"
#include "controller.h"
#include "drive_config.h"
#include "bt_gamepad.h"
#include <BLEDevice.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>

static WebServer          sServer(80);
static AmidalaController* sCtrl = nullptr;
// Count of user-defined serial strings, excluding built-in injected commands.
// Saved before injectBuiltinSerialCmds() runs; used to bound rewriteSerialStrings().
static uint8_t            sUserSerialCount = 0;

// ---------------------------------------------------------------------------
// Serial monitor log buffer
// ---------------------------------------------------------------------------

#define MONITOR_BUF_OWNER
#include "monitor_buf.h"
#include "monitor_drain.h"

// ---------------------------------------------------------------------------
// SD card config write-back
// ---------------------------------------------------------------------------

#include "config_file.h"

// ---------------------------------------------------------------------------
// Gadget configuration
// ---------------------------------------------------------------------------

// mDNS hostname -- deliberately independent of params.wifiSSID. MDNS.begin()
// used to be passed the AP's SSID directly, so renaming the WiFi network
// silently renamed the mDNS address too (http://amidala.local only worked
// if the SSID happened to still be "amidala"). The URL people actually type
// should stay stable regardless of what the AP is named.
static const char* kMdnsHostname = "amidala";

static const uint8_t GADGET_COUNT   = 7;
static const uint8_t GADGET_DISABLED = 0;
static const uint8_t GADGET_ENABLED  = 1;
static const uint8_t GADGET_UPPITY   = 2;  // Uppity Spinner (periscope)

struct GadgetCfg {
    uint8_t type;
    uint8_t sstr[16];   // 1-based serial string indices (0 = empty slot)
    uint8_t sstrCnt;
};
static GadgetCfg sGadgets[GADGET_COUNT];

static void parseGadgetLine(const String& val) {
    int c = val.indexOf(',');
    if (c < 0) return;
    int idx = val.substring(0, c).toInt();
    if (idx < 0 || idx >= GADGET_COUNT) return;
    String rest = val.substring(c + 1);
    c = rest.indexOf(',');
    uint8_t type;
    String sstrPart;
    if (c < 0) { type = (uint8_t)rest.toInt(); sstrPart = ""; }
    else        { type = (uint8_t)rest.substring(0, c).toInt(); sstrPart = rest.substring(c + 1); }
    sGadgets[idx].type    = type;
    sGadgets[idx].sstrCnt = 0;
    int pos = 0;
    while (pos <= (int)sstrPart.length() && sGadgets[idx].sstrCnt < 16) {
        int next = sstrPart.indexOf(',', pos);
        String part = (next < 0) ? sstrPart.substring(pos) : sstrPart.substring(pos, next);
        part.trim();
        if (part.length() > 0) {
            uint8_t si = (uint8_t)part.toInt();
            if (si > 0) sGadgets[idx].sstr[sGadgets[idx].sstrCnt++] = si;
        }
        if (next < 0) break;
        pos = next + 1;
    }
}

static void loadGadgetConfig() {
    memset(sGadgets, 0, sizeof(sGadgets));
    File f = SD.open("/config.txt", "r");
    if (!f) return;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("gadget="))
            parseGadgetLine(line.substring(7));
    }
    f.close();
}

static bool rewriteGadgetConfig() {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    if (!f) return false;
    String out;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (!line.startsWith("gadget=")) { out += line; out += "\n"; }
    }
    f.close();
    for (uint8_t i = 0; i < GADGET_COUNT; i++) {
        if (sGadgets[i].type == GADGET_DISABLED) continue;
        out += "gadget=";
        out += String(i); out += ","; out += String(sGadgets[i].type);
        // Periscope Uppity Spinner sstr is auto-injected at startup — don't persist
        bool skipSstr = (i == 0 && sGadgets[i].type == GADGET_UPPITY);
        if (!skipSstr) {
            for (uint8_t j = 0; j < sGadgets[i].sstrCnt; j++) {
                out += ","; out += String(sGadgets[i].sstr[j]);
            }
        }
        out += "\n";
    }
    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

static String buildGadgetsCfgJson() {
    String j = "[";
    for (uint8_t i = 0; i < GADGET_COUNT; i++) {
        if (i > 0) j += ",";
        j += "{\"type\":"; j += String(sGadgets[i].type);
        j += ",\"sstr\":[";
        for (uint8_t k = 0; k < sGadgets[i].sstrCnt; k++) {
            if (k > 0) j += ",";
            j += String(sGadgets[i].sstr[k]);
        }
        j += "]}";
    }
    j += "]";
    return j;
}

// ---------------------------------------------------------------------------
// Built-in gadget commands injected into params.Str[] at startup
// ---------------------------------------------------------------------------

struct BuiltinCmd { const char name[32]; const char str[16]; };

// Operational commands — injected into params.Str[] so they appear in button
// assignments and on the Droid Control → Gadgets tab.
static const BuiltinCmd UPPITY_CMDS[] = {
    {"Periscope: Home",           ":PH"},
    {"Periscope: Raise Full",     ":PP100"},
    {"Periscope: Raise Half",     ":PP50"},
    {"Periscope: Random Gentle",  ":PMG"},
    {"Periscope: Random Medium",  ":PMM"},
    {"Periscope: Random Strong",  ":PMA"},
    {"Periscope: Stop",           ":PX"},
    {"Periscope: Face Forward",   ":PA0"},
    {"Periscope: Spin CCW",       ":PR30"},
    {"Periscope: Spin CW",        ":PR-30"},
    {"Periscope: Stop Spin",      ":PR0"},
};
// Config/calibration commands are web-only (sent via /api/gadget-cmd) — see gadgets.html.
static constexpr uint8_t UPPITY_CMD_COUNT = sizeof(UPPITY_CMDS) / sizeof(UPPITY_CMDS[0]);

static void injectBuiltinSerialCmds() {
    if (!sCtrl || sGadgets[0].type != GADGET_UPPITY) return;
    AmidalaParameters& p = sCtrl->params;
    uint8_t base = p.serialcount;
    uint8_t added = 0;
    for (uint8_t i = 0; i < UPPITY_CMD_COUNT; i++) {
        if ((uint16_t)base + i >= 255) break;   // uint8_t serialstr index limit
        strlcpy(p.Str[base + i].name, UPPITY_CMDS[i].name, sizeof(p.Str[0].name));
        strlcpy(p.Str[base + i].str,  UPPITY_CMDS[i].str,  sizeof(p.Str[0].str));
        added++;
    }
    p.serialcount = base + added;
    sGadgets[0].sstrCnt = added;
    for (uint8_t i = 0; i < added; i++)
        sGadgets[0].sstr[i] = base + 1 + i; // 1-based indices
}

// ---------------------------------------------------------------------------
// Generic single-value config key helpers
// ---------------------------------------------------------------------------

// Replace (or insert) a "key=value\n" line in config.txt.
// key must include the trailing '=' (e.g. "btaddr=").
static bool updateConfigKey(const char* key, const char* value) {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    String out;
    out.reserve(4096);
    bool found = false;
    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            if (line.startsWith(key)) {
                out += String(key) + value + "\n";
                found = true;
            } else {
                out += line + "\n";
            }
        }
        f.close();
    } else {
        out = "#START\n#END\n";
    }
    if (!found) {
        // Insert before #END.
        int endIdx = out.lastIndexOf("#END");
        String ins = String(key) + value + "\n";
        if (endIdx >= 0)
            out = out.substring(0, endIdx) + ins + out.substring(endIdx);
        else
            out += ins;
    }
    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

// Remove all lines starting with the given key prefix from config.txt.
static bool removeConfigKey(const char* key) {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    if (!f) return false;
    String out;
    out.reserve(4096);
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.endsWith("\r")) line.remove(line.length() - 1);
        if (!line.startsWith(key))
            out += line + "\n";
    }
    f.close();
    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

// ---------------------------------------------------------------------------
// Serial-string config file helpers
// ---------------------------------------------------------------------------

static bool rewriteSerialStrings() {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    String out;
    out.reserve(8192);

    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            if (!line.startsWith("sstr="))
                out += line + "\n";
        }
        f.close();
    } else {
        out = "#START\n#END\n";
    }

    // Build new sstr= lines — only user-defined strings, not builtin injected ones.
    // Format: sstr=ID|Name|command
    String sstrs;
    for (uint8_t i = 0; i < sUserSerialCount; i++) {
        sstrs += "sstr=";
        sstrs += String(sCtrl->params.Str[i].id);
        sstrs += "|";
        sstrs += sCtrl->params.Str[i].name;
        sstrs += "|";
        sstrs += sCtrl->params.Str[i].str;
        sstrs += "\n";
    }

    // Insert before #END if present, otherwise append
    int endIdx = out.lastIndexOf("#END");
    if (endIdx >= 0)
        out = out.substring(0, endIdx) + sstrs + out.substring(endIdx);
    else
        out += sstrs;

    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

// Rewrite f=, hidden=, and cat= lines in config.txt from current params state.
static bool rewriteSstrMeta() {
    if (!sCtrl) return false;
    String path = "/config.txt";
    File f = SD.open(path, "r");
    String out;
    out.reserve(8192);
    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            if (!line.startsWith("f=") && !line.startsWith("hidden=") && !line.startsWith("cat="))
                out += line + "\n";
        }
        f.close();
    } else {
        out = "#START\n#END\n";
    }

    AmidalaParameters& p = sCtrl->params;
    String meta;

    // Favorites
    if (p.sstr_fav_cnt > 0) {
        meta += "f=";
        for (uint8_t i = 0; i < p.sstr_fav_cnt; i++) {
            if (i > 0) meta += ",";
            meta += String(p.sstr_favs[i]);
        }
        meta += "\n";
    }

    // Hidden
    if (p.sstr_hidden_cnt > 0) {
        meta += "hidden=";
        for (uint8_t i = 0; i < p.sstr_hidden_cnt; i++) {
            if (i > 0) meta += ",";
            meta += String(p.sstr_hidden[i]);
        }
        meta += "\n";
    }

    // Categories
    for (uint8_t i = 0; i < p.sstr_cat_count; i++) {
        meta += "cat=";
        meta += p.sstr_cats[i].name;
        meta += "|";
        for (uint8_t j = 0; j < p.sstr_cats[i].cnt; j++) {
            if (j > 0) meta += ",";
            meta += String(p.sstr_cats[i].idx[j]);
        }
        meta += "\n";
    }

    int endIdx = out.lastIndexOf("#END");
    if (endIdx >= 0)
        out = out.substring(0, endIdx) + meta + out.substring(endIdx);
    else
        out += meta;

    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

static bool rewriteSafetyCmds() {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    String out;
    out.reserve(8192);
    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            if (!line.startsWith("estopstr=") && !line.startsWith("resumestr="))
                out += line + "\n";
        }
        f.close();
    } else {
        out = "#START\n#END\n";
    }
    String block;
    for (uint8_t i = 0; i < sCtrl->params.estopCmdCount; i++)
        block += "estopstr=" + String(sCtrl->params.EstopCmds[i].str) + "\n";
    for (uint8_t i = 0; i < sCtrl->params.resumeCmdCount; i++)
        block += "resumestr=" + String(sCtrl->params.ResumeCmds[i].str) + "\n";
    int endIdx = out.lastIndexOf("#END");
    if (endIdx >= 0)
        out = out.substring(0, endIdx) + block + out.substring(endIdx);
    else
        out += block;
    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

// ---------------------------------------------------------------------------
// Servo config file helpers
// ---------------------------------------------------------------------------

static bool rewriteServos() {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    String out;
    out.reserve(8192);

    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            if (!line.startsWith("s="))
                out += line + "\n";
        }
        f.close();
    } else {
        out = "#START\n#END\n";
    }

    String lines;
    uint8_t count = sCtrl->params.getServoCount();
    for (uint8_t i = 0; i < count; i++) {
        const AmidalaParameters::Channel& ch = sCtrl->params.S[i];
        lines += "s=";
        lines += String(i + 1) + "," + String(ch.min) + "," + String(ch.max) + ",";
        lines += String(ch.n)  + "," + String(ch.d)   + "," + String(ch.t)   + ",";
        lines += String(ch.s)  + "," + String(ch.r ? 1 : 0);
        lines += "\n";
    }

    int endIdx = out.lastIndexOf("#END");
    if (endIdx >= 0)
        out = out.substring(0, endIdx) + lines + out.substring(endIdx);
    else
        out += lines;

    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

static bool rewriteSoundBanks() {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    String out;
    out.reserve(8192);

    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            if (!line.startsWith("sb="))
                out += line + "\n";
        }
        f.close();
    } else {
        out = "#START\n#END\n";
    }

    String lines;
    for (uint8_t i = 0; i < sCtrl->params.sbcount; i++) {
        const AmidalaParameters::SoundBank& sb = sCtrl->params.SB[i];
        lines += "sb=";
        lines += sb.dir;
        lines += ",";
        lines += String(sb.numfiles);
        lines += ",";
        lines += sb.random ? "r" : "s";
        lines += "\n";
    }

    int endIdx = out.lastIndexOf("#END");
    if (endIdx >= 0)
        out = out.substring(0, endIdx) + lines + out.substring(endIdx);
    else
        out += lines;

    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

// ---------------------------------------------------------------------------
// Button / gesture config file helpers
// ---------------------------------------------------------------------------

// Serialize a ButtonAction as "type[,arg1[,arg2[,name]]]"
static String buttonActionStr(const ButtonAction& b) {
    String s = String(b.action);
    switch (b.action) {
    case ButtonAction::kSerialStr:
        s += "," + String(b.serialid);
        break;
    case ButtonAction::kI2CStr:
        s += "," + String(b.i2cstr.target) + "," + String(b.serialid);
        break;
    case ButtonAction::kHCREmote:
        s += "," + String(b.emote.emotion) + "," + String(b.emote.level);
        if (b.serialid) s += "," + String(b.serialid);
        break;
    case ButtonAction::kDomeCmd:
        s += "," + String(b.dome.subcmd);
        if (b.dome.arg || b.serialid) s += "," + String(b.dome.arg);
        if (b.serialid) s += "," + String(b.serialid);
        break;
    default:
        if (b.serialid) s += "," + String(b.serialid);
        break;
    }
    return s;
}

// Parse "type[,arg1[,arg2[,serialid]]]" value string into a ButtonAction.
static void parseButtonAction(ButtonAction& b, const String& value) {
    memset(&b, 0, sizeof(b));
    if (value.isEmpty() || value == "0") return;
    int c1   = value.indexOf(',');
    int type = value.substring(0, c1 > 0 ? c1 : (int)value.length()).toInt();
    switch (type) {
    case ButtonAction::kSerialStr:
        b.action = type;
        if (c1 > 0) b.serialid = (uint16_t)value.substring(c1 + 1).toInt();
        break;
    case ButtonAction::kI2CStr: {
        b.action = type;
        int c2 = c1 > 0 ? value.indexOf(',', c1 + 1) : -1;
        if (c1 > 0) b.i2cstr.target = (uint8_t)value.substring(c1 + 1, c2 > 0 ? c2 : (int)value.length()).toInt();
        if (c2 > 0) b.serialid = (uint16_t)value.substring(c2 + 1).toInt();
        break;
    }
    case ButtonAction::kHCREmote: {
        b.action = type;
        int c2 = c1 > 0 ? value.indexOf(',', c1 + 1) : -1;
        int c3 = c2 > 0 ? value.indexOf(',', c2 + 1) : -1;
        if (c1 > 0) b.emote.emotion = (uint8_t)value.substring(c1 + 1, c2 > 0 ? c2 : (int)value.length()).toInt();
        if (c2 > 0) b.emote.level   = (uint8_t)value.substring(c2 + 1, c3 > 0 ? c3 : (int)value.length()).toInt();
        if (c3 > 0) b.serialid = (uint16_t)value.substring(c3 + 1).toInt();
        break;
    }
    case ButtonAction::kHCRMuse:
        b.action = type;
        if (c1 > 0) b.serialid = (uint16_t)value.substring(c1 + 1).toInt();
        break;
    case ButtonAction::kDomeCmd: {
        b.action = type;
        int c2 = c1 > 0 ? value.indexOf(',', c1 + 1) : -1;
        int c3 = c2 > 0 ? value.indexOf(',', c2 + 1) : -1;
        if (c1 > 0) b.dome.subcmd = (uint8_t)value.substring(c1 + 1, c2 > 0 ? c2 : (int)value.length()).toInt();
        if (c2 > 0) b.dome.arg    = (uint8_t)value.substring(c2 + 1, c3 > 0 ? c3 : (int)value.length()).toInt();
        if (c3 > 0) b.serialid = (uint16_t)value.substring(c3 + 1).toInt();
        break;
    }
    default: break;
    }
}

// Rewrite all b=, lb=, ab= lines in config.txt from current params
static bool rewriteButtons() {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    String out;
    out.reserve(8192);
    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            if (!line.startsWith("b=") && !line.startsWith("lb=") && !line.startsWith("ab=") && !line.startsWith("db="))
                out += line + "\n";
        }
        f.close();
    } else {
        out = "#START\n#END\n";
    }
    String lines;
    for (int i = 0; i < 9; i++) {
        if (sCtrl->params.B[i].action  != ButtonAction::kNone)
            lines += "b="  + String(i + 1) + "," + buttonActionStr(sCtrl->params.B[i])  + "\n";
        if (sCtrl->params.LB[i].action != ButtonAction::kNone)
            lines += "lb=" + String(i + 1) + "," + buttonActionStr(sCtrl->params.LB[i]) + "\n";
        if (sCtrl->params.AB[i].action != ButtonAction::kNone)
            lines += "ab=" + String(i + 1) + "," + buttonActionStr(sCtrl->params.AB[i]) + "\n";
        if (sCtrl->params.DB[i].action != ButtonAction::kNone)
            lines += "db=" + String(i + 1) + "," + buttonActionStr(sCtrl->params.DB[i]) + "\n";
    }
    int endIdx = out.lastIndexOf("#END");
    if (endIdx >= 0)
        out = out.substring(0, endIdx) + lines + out.substring(endIdx);
    else
        out += lines;
    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

// Rewrite all g= lines in config.txt from current params
static bool rewriteGestures() {
    String path = "/config.txt";
    File f = SD.open(path, "r");
    String out;
    out.reserve(8192);
    if (f) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.endsWith("\r")) line.remove(line.length() - 1);
            if (!line.startsWith("g="))
                out += line + "\n";
        }
        f.close();
    } else {
        out = "#START\n#END\n";
    }
    String lines;
    char seqbuf[MAX_GESTURE_LENGTH + 1];
    for (uint8_t i = 0; i < sCtrl->params.gcount; i++) {
        if (sCtrl->params.G[i].gesture.isEmpty()) continue;
        sCtrl->params.G[i].gesture.getGestureString(seqbuf);
        lines += "g=" + String(seqbuf) + "," + buttonActionStr(sCtrl->params.G[i].action) + "\n";
    }
    int endIdx = out.lastIndexOf("#END");
    if (endIdx >= 0)
        out = out.substring(0, endIdx) + lines + out.substring(endIdx);
    else
        out += lines;
    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}

// ---------------------------------------------------------------------------
// REST API handlers
// ---------------------------------------------------------------------------

static void handleApiInfo() {
    if (!sCtrl) { sServer.send(500, "application/json", "{}"); return; }

#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER
    const char* drive = "sabertooth";
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_PWM
    const char* drive = "pwm";
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL
    const char* drive = "roboteq-serial";
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM
    const char* drive = "roboteq-pwm";
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
    const char* drive = "roboteq-pwm-serial";
#else
    const char* drive = nullptr;
#endif
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    const char* dome = "roboclaw";
#elif DOME_DRIVE == DOME_DRIVE_PWM
    const char* dome = "pwm";
#elif DOME_DRIVE == DOME_DRIVE_SABER
    const char* dome = "sabertooth";
#else
    const char* dome = nullptr;
#endif
#ifdef VMUSIC_SERIAL
    const char* audio = "vmusic";
#else
    const char* audio = "hcr";
#endif

    // Which role (issue #147) actually occupies each reassignable physical
    // port right now -- lets web/monitor.html adapt to reassignment instead
    // of assuming a fixed port-to-role mapping from the compiled drive/dome
    // type alone. "aux" = up but unclaimed by a built-in protocol (auxserial3
    // turned it on anyway); "unused" = not running at all. Serial1 has no
    // "force on anyway" toggle, so it's simply "unused" when unclaimed.
    const char* serial1Role = "unused";
    const char* serial2Role = sCtrl->params.auxserial3 ? "aux" : "unused";
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW || DOME_DRIVE == DOME_DRIVE_SABER
    if (sCtrl->params.domeSerialPort == SerialPortId::kSerial1) serial1Role = dome;
    else serial2Role = dome;
#endif
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER || \
    DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL || \
    DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
    if (sCtrl->params.driveSerialPort == SerialPortId::kSerial1) serial1Role = drive;
    else serial2Role = drive;
#endif

    sServer.send(200, "application/json",
        buildInfoJson(drive, dome, audio,
                      sCtrl->params.wifiSSID,
                      WiFi.softAPIP().toString().c_str(),
                      sCtrl->params.serialcount,
                      ESP.getFreeHeap(),
                      sCtrl->fDriveStick.isConnected(),
                      sCtrl->fDomeStick.isConnected(),
                      gBTGamepad.isConnected(),
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
                      sCtrl->fDomeDrive->isHomed(),
                      sCtrl->fDomeDrive->getCurrentDegrees(),
#else
                      false, 0,
#endif
                      serial1Role, serial2Role));
}

static void handleApiConfigGet() {
    if (!sCtrl) { sServer.send(500, "application/json", "{}"); return; }
    sServer.send(200, "application/json",
        buildFullConfigJson(sCtrl->params, buildGadgetsCfgJson(), sUserSerialCount));
}

static void setSerialStringFields(SerialString& s, const char* val) {
    const char* pipe = strchr(val, '|');
    if (pipe) {
        size_t nlen = min((size_t)(pipe - val), sizeof(s.name) - 1);
        memcpy(s.name, val, nlen);
        s.name[nlen] = '\0';
        strncpy(s.str, pipe + 1, sizeof(s.str) - 1);
        s.str[sizeof(s.str) - 1] = '\0';
    } else {
        s.name[0] = '\0';
        strncpy(s.str, val, sizeof(s.str) - 1);
        s.str[sizeof(s.str) - 1] = '\0';
    }
}

static void handleApiConfigPost() {
    if (!sCtrl) { sServer.send(500, "text/plain", "no controller"); return; }

    String key   = sServer.arg("key");
    String value = sServer.arg("value");
    if (key.isEmpty()) { sServer.send(400, "text/plain", "missing key"); return; }

    // s=N,min,max,n,d,t,speed,reversed — servo channel config
    if (key == "s") {
        String cmd = "s=" + value;
        if (!sCtrl->fConfig.processConfig(cmd.c_str())) {
            sServer.send(400, "text/plain", "invalid servo config"); return;
        }
        bool ok = rewriteServos();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — change applied in memory only");
        return;
    }

    // Reassignable GPIO pin roles (issue #133) -- validate directly so the
    // client gets the SPECIFIC rejection reason (e.g. "servo channels full
    // (8/8)") instead of the generic fallback's misleading "unknown
    // setting" message for a known key with an invalid value. Key format
    // matches config.txt's pin<N>role= scheme (see config.cpp), e.g.
    // "pin1role"/"pin39role"/"pin40role".
    for (uint8_t i = 0; i < 11; i++) {
        String pinKey = "pin" + String(kAssignablePins[i]) + "role";
        if (key != pinKey) continue;
        AmidalaParameters &params = sCtrl->params;
        PinRoleType newRole;
        if (!pinRoleFromString(value.c_str(), &newRole)) {
            sServer.send(400, "text/plain", "unrecognized role");
            return;
        }
        PinRoleValidationResult r =
            validateRoleChange(kAssignablePins[i], newRole, params.pinRole);
        if (!r.ok) {
            sServer.send(400, "text/plain", r.reason);
            return;
        }
        params.pinRole[i] = newRole;
        if (!updateConfigFile(pinKey.c_str(), value.c_str())) {
            sServer.send(500, "text/plain", "SD write failed — change applied in memory only");
            return;
        }
        sServer.send(200, "text/plain", "OK");
        return;
    }

    // Reassignable dome/drive serial ports (issue #147) -- validate
    // directly so the client gets the SPECIFIC rejection reason ("port
    // already used by the other serial subsystem") instead of the generic
    // fallback's misleading "unknown setting" message. Key format matches
    // config.txt's domeserialport=/driveserialport= scheme (see config.cpp).
    if (key == "domeserialport" || key == "driveserialport") {
        AmidalaParameters &params = sCtrl->params;
        SerialConsumer consumer =
            (key == "domeserialport") ? SerialConsumer::kDome : SerialConsumer::kDrive;
        SerialPortId newPort;
        if (!serialPortFromString(value.c_str(), &newPort)) {
            sServer.send(400, "text/plain", "unrecognized port");
            return;
        }
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW || DOME_DRIVE == DOME_DRIVE_SABER
        constexpr bool kDomeActive = true;
#else
        constexpr bool kDomeActive = false;
#endif
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER || \
    DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL || \
    DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
        constexpr bool kDriveActive = true;
#else
        constexpr bool kDriveActive = false;
#endif
        bool otherActive = (consumer == SerialConsumer::kDome) ? kDriveActive : kDomeActive;
        SerialPortId otherPort = (consumer == SerialConsumer::kDome) ? params.driveSerialPort
                                                                      : params.domeSerialPort;
        SerialPortValidationResult r =
            validateSerialPortChange(consumer, newPort, otherActive, otherPort);
        if (!r.ok) {
            sServer.send(400, "text/plain", r.reason);
            return;
        }
        if (consumer == SerialConsumer::kDome) params.domeSerialPort = newPort;
        else params.driveSerialPort = newPort;
        if (!updateConfigFile(key.c_str(), value.c_str())) {
            sServer.send(500, "text/plain", "SD write failed — change applied in memory only");
            return;
        }
        sServer.send(200, "text/plain", "OK");
        return;
    }

    // sstr_del_N — delete serial string at index N, shift remainder down
    if (key.startsWith("sstr_del_")) {
        int idx = key.substring(9).toInt();
        // Only allow deletion of user-defined strings (not builtin injected ones)
        if (idx < 0 || idx >= (int)sUserSerialCount) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        uint16_t deletedId = sCtrl->params.Str[idx].id;
        // Shift all strings (including injected) down by one
        for (int j = idx; j < (int)sCtrl->params.serialcount - 1; j++)
            sCtrl->params.Str[j] = sCtrl->params.Str[j + 1];
        memset(&sCtrl->params.Str[sCtrl->params.serialcount - 1], 0, sizeof(SerialString));
        sCtrl->params.serialcount--;
        sUserSerialCount--;
        // Update gadget sstr indices: entries pointing past idx shift down; the
        // deleted entry (idx+1, 1-based) is removed from any gadget assignment.
        bool gadgetsDirty = false;
        for (int g = 0; g < GADGET_COUNT; g++) {
            uint8_t wk = 0;
            for (uint8_t k = 0; k < sGadgets[g].sstrCnt; k++) {
                uint8_t ref = sGadgets[g].sstr[k]; // 1-based
                if (ref == (uint8_t)(idx + 1)) { gadgetsDirty = true; continue; }
                if (ref > (uint8_t)(idx + 1))  { ref--; gadgetsDirty = true; }
                sGadgets[g].sstr[wk++] = ref;
            }
            sGadgets[g].sstrCnt = wk;
        }
        // Clear any button/gesture refs pointing to the deleted string's ID.
        bool buttonsDirty = false;
        if (deletedId != 0) {
            AmidalaParameters& p = sCtrl->params;
            ButtonAction* layers[4] = {p.B, p.LB, p.AB, p.DB};
            for (int l = 0; l < 4; l++) {
                for (int i = 0; i < (int)p.getButtonCount(); i++) {
                    if (layers[l][i].serialid == deletedId) {
                        layers[l][i].serialid = 0;
                        buttonsDirty = true;
                    }
                }
            }
            for (int i = 0; i < (int)p.gcount; i++) {
                if (p.G[i].action.serialid == deletedId) {
                    p.G[i].action.serialid = 0;
                    buttonsDirty = true;
                }
            }
        }
        bool ok = rewriteSerialStrings();
        if (gadgetsDirty) rewriteGadgetConfig();
        if (buttonsDirty) rewriteButtons();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — delete applied in memory only");
        return;
    }

    // sstr_add — append a new serial string; sstr_N — update string at index N.
    // sstr_add must be checked explicitly: "add".toInt() == 0, so without this
    // guard it would silently overwrite Str[0] whenever entries already exist.
    if (key == "sstr_add" || (key.startsWith("sstr_") && key.length() > 5 && isDigit(key.charAt(5)))) {
        int idx = (key == "sstr_add") ? (int)sUserSerialCount : key.substring(5).toInt();
        bool isAppend = (idx == (int)sUserSerialCount);
        bool isEdit   = (idx >= 0 && idx < (int)sUserSerialCount);
        if (!isEdit && !isAppend) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        if (isAppend) {
            // Guard against full Str[] array
            if ((int)sCtrl->params.serialcount >= (int)sCtrl->params.getSerialStringCount()) {
                sServer.send(507, "text/plain", "serial string buffer full"); return;
            }
            // Shift injected strings up one slot to insert the new user string
            for (int j = (int)sCtrl->params.serialcount; j > (int)sUserSerialCount; j--)
                sCtrl->params.Str[j] = sCtrl->params.Str[j - 1];
            memset(&sCtrl->params.Str[sUserSerialCount], 0, sizeof(SerialString));
            sCtrl->params.serialcount++;
            // Assign a stable ID that will never shift even if the array is reordered.
            sCtrl->params.Str[sUserSerialCount].id = sCtrl->params.nextSstrId++;
            // Update gadget sstr indices: entries at or above new position shift up
            for (int g = 0; g < GADGET_COUNT; g++)
                for (uint8_t k = 0; k < sGadgets[g].sstrCnt; k++)
                    if (sGadgets[g].sstr[k] > (uint8_t)idx)
                        sGadgets[g].sstr[k]++;
            sUserSerialCount++;
        }
        // Rename is a no-op for button refs — they track by ID, not name.
        setSerialStringFields(sCtrl->params.Str[idx], value.c_str());
        bool ok = rewriteSerialStrings();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — change applied in memory only");
        return;
    }

    // sstr_favs — replace entire favorites list; value: "1,3,5"
    if (key == "sstr_favs") {
        AmidalaParameters& p = sCtrl->params;
        p.sstr_fav_cnt = 0;
        const char* ptr = value.c_str();
        while (*ptr && p.sstr_fav_cnt < MAX_SSTR_FAVS) {
            uint16_t v = 0;
            while (*ptr >= '0' && *ptr <= '9') v = v * 10 + (*ptr++ - '0');
            if (v > 0) p.sstr_favs[p.sstr_fav_cnt++] = v;
            if (*ptr == ',') ptr++;
        }
        bool ok = rewriteSstrMeta();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed");
        return;
    }

    // sstr_hidden — replace entire hidden list; value: "2,4"
    if (key == "sstr_hidden") {
        AmidalaParameters& p = sCtrl->params;
        p.sstr_hidden_cnt = 0;
        const char* ptr = value.c_str();
        while (*ptr && p.sstr_hidden_cnt < MAX_SSTR_HIDDEN) {
            uint16_t v = 0;
            while (*ptr >= '0' && *ptr <= '9') v = v * 10 + (*ptr++ - '0');
            if (v > 0) p.sstr_hidden[p.sstr_hidden_cnt++] = v;
            if (*ptr == ',') ptr++;
        }
        bool ok = rewriteSstrMeta();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed");
        return;
    }

    // sstr_cat_del_N — delete category at index N
    if (key.startsWith("sstr_cat_del_")) {
        int idx = key.substring(13).toInt();
        AmidalaParameters& p = sCtrl->params;
        if (idx >= 0 && idx < (int)p.sstr_cat_count) {
            for (int j = idx; j < (int)p.sstr_cat_count - 1; j++)
                p.sstr_cats[j] = p.sstr_cats[j + 1];
            p.sstr_cat_count--;
        }
        bool ok = rewriteSstrMeta();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed");
        return;
    }

    // sstr_cat_add — append new category; sstr_cat_N — update category at index N.
    // value format: "Name|1,3,5"
    if (key == "sstr_cat_add" || (key.startsWith("sstr_cat_") && key.length() > 9 && isDigit(key.charAt(9)))) {
        AmidalaParameters& p = sCtrl->params;
        int idx = (key == "sstr_cat_add") ? (int)p.sstr_cat_count : key.substring(9).toInt();
        bool isAppend = (idx == (int)p.sstr_cat_count);
        bool isEdit   = (idx >= 0 && idx < (int)p.sstr_cat_count);
        if (!isEdit && !isAppend) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        if (isAppend) {
            if (p.sstr_cat_count >= MAX_SSTR_CATS) {
                sServer.send(507, "text/plain", "category limit reached"); return;
            }
            memset(&p.sstr_cats[p.sstr_cat_count], 0, sizeof(p.sstr_cats[0]));
            p.sstr_cat_count++;
        }
        AmidalaParameters::SstrCat& cat = p.sstr_cats[idx];
        int pipe = value.indexOf('|');
        if (pipe < 0) { sServer.send(400, "text/plain", "bad format"); return; }
        String name = value.substring(0, pipe);
        strncpy(cat.name, name.c_str(), sizeof(cat.name) - 1);
        cat.name[sizeof(cat.name) - 1] = '\0';
        cat.cnt = 0;
        const char* ptr = value.c_str() + pipe + 1;
        while (*ptr && cat.cnt < MAX_SSTR_CAT_ENTRIES) {
            uint16_t v = 0;
            while (*ptr >= '0' && *ptr <= '9') v = v * 10 + (*ptr++ - '0');
            if (v > 0) cat.idx[cat.cnt++] = v;
            if (*ptr == ',') ptr++;
        }
        bool ok = rewriteSstrMeta();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed");
        return;
    }

    // sb_del_N — delete sound bank at index N, shift remainder down
    if (key.startsWith("sb_del_")) {
        int idx = key.substring(7).toInt();
        if (idx < 0 || idx >= (int)sCtrl->params.sbcount) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        for (int j = idx; j < (int)sCtrl->params.sbcount - 1; j++)
            sCtrl->params.SB[j] = sCtrl->params.SB[j + 1];
        memset(&sCtrl->params.SB[sCtrl->params.sbcount - 1], 0, sizeof(AmidalaParameters::SoundBank));
        sCtrl->params.sbcount--;
        bool ok = rewriteSoundBanks();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — delete applied in memory only");
        return;
    }

    // sb_N — update (or append when N == sbcount) sound bank at index N
    // value format: DIR,numfiles,[s|r]
    if (key.startsWith("sb_") && key.length() > 3 && isDigit(key[3])) {
        int idx = key.substring(3).toInt();
        if (idx < 0 || idx > (int)sCtrl->params.sbcount ||
            idx >= (int)sCtrl->params.getSoundBankCount()) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        // Parse "DIR,numfiles,[s|r]"
        int c1 = value.indexOf(',');
        int c2 = c1 >= 0 ? value.indexOf(',', c1 + 1) : -1;
        if (c1 < 0 || c2 < 0) {
            sServer.send(400, "text/plain", "bad format: DIR,numfiles,[s|r]"); return;
        }
        AmidalaParameters::SoundBank& sb = sCtrl->params.SB[idx];
        memset(&sb, 0, sizeof(sb));
        String dir = value.substring(0, c1);
        dir.toCharArray(sb.dir, sizeof(sb.dir));
        sb.numfiles = (uint8_t)value.substring(c1 + 1, c2).toInt();
        sb.random   = value.charAt(c2 + 1) == 'r';
        if (idx == (int)sCtrl->params.sbcount)
            sCtrl->params.sbcount++;
        bool ok = rewriteSoundBanks();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — change applied in memory only");
        return;
    }

    // btn_N_press / btn_N_long / btn_N_alt — update a single button layer
    if (key.startsWith("btn_")) {
        int us = key.indexOf('_', 4);
        if (us < 0) { sServer.send(400, "text/plain", "bad key format"); return; }
        int    btnNum = key.substring(4, us).toInt();
        String layer  = key.substring(us + 1);  // "press", "long", or "alt"
        if (btnNum < 1 || btnNum > 9) { sServer.send(400, "text/plain", "button 1–9 only"); return; }
        int idx = btnNum - 1;

        if (value == "altbtn") {
            sCtrl->params.altbtn = (uint8_t)btnNum;
            memset(&sCtrl->params.B[idx], 0, sizeof(ButtonAction));
            bool ok  = updateConfigFile("altbtn", String(btnNum).c_str());
            bool ok2 = rewriteButtons();
            sServer.send((ok && ok2) ? 200 : 500, "text/plain", (ok && ok2) ? "OK" : "SD write failed — change applied in memory only");
            return;
        }
        if (value == "mutebutton") {
            sCtrl->params.mutebutton = (uint8_t)btnNum;
            memset(&sCtrl->params.B[idx], 0, sizeof(ButtonAction));
            bool ok  = updateConfigFile("mutebutton", String(btnNum).c_str());
            bool ok2 = rewriteButtons();
            sServer.send((ok && ok2) ? 200 : 500, "text/plain", (ok && ok2) ? "OK" : "SD write failed — change applied in memory only");
            return;
        }

        // Regular action: clear alt/mute role if this button previously held it
        if (layer == "press") {
            if (sCtrl->params.altbtn    == (uint8_t)btnNum) { sCtrl->params.altbtn    = 0; updateConfigFile("altbtn",    "0"); }
            if (sCtrl->params.mutebutton == (uint8_t)btnNum) { sCtrl->params.mutebutton = 0; updateConfigFile("mutebutton","0"); }
        }
        ButtonAction* arr = (layer == "long")   ? sCtrl->params.LB
                          : (layer == "alt")    ? sCtrl->params.AB
                          : (layer == "double") ? sCtrl->params.DB
                          :                      sCtrl->params.B;
        parseButtonAction(arr[idx], value);
        bool ok = rewriteButtons();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — change applied in memory only");
        return;
    }

    // gesture_del_N — delete gesture at index N and shift remainder down
    if (key.startsWith("gesture_del_")) {
        int idx = key.substring(12).toInt();
        if (idx < 0 || idx >= (int)sCtrl->params.gcount) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        for (int j = idx; j < (int)sCtrl->params.gcount - 1; j++)
            sCtrl->params.G[j] = sCtrl->params.G[j + 1];
        memset(&sCtrl->params.G[sCtrl->params.gcount - 1], 0, sizeof(GestureAction));
        sCtrl->params.gcount--;
        bool ok = rewriteGestures();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — delete applied in memory only");
        return;
    }

    // gesture_add — append a new gesture; value: "SEQ,type[,p1[,p2]]"
    if (key == "gesture_add") {
        if ((int)sCtrl->params.gcount >= (int)sCtrl->params.getGestureCount()) {
            sServer.send(400, "text/plain", "gesture limit reached"); return;
        }
        int    c1  = value.indexOf(',');
        String seq = c1 > 0 ? value.substring(0, c1) : value;
        String act = c1 > 0 ? value.substring(c1 + 1) : "0";
        GestureAction& g = sCtrl->params.G[sCtrl->params.gcount];
        memset(&g, 0, sizeof(g));
        g.gesture.setGesture(seq.c_str());
        parseButtonAction(g.action, act);
        sCtrl->params.gcount++;
        bool ok = rewriteGestures();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — change applied in memory only");
        return;
    }

    // gesture_N — update gesture at index N; value: "SEQ,type[,p1[,p2]]"
    if (key.startsWith("gesture_") && key.length() > 8 && isDigit(key.charAt(8))) {
        int idx = key.substring(8).toInt();
        if (idx < 0 || idx >= (int)sCtrl->params.gcount) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        int    c1  = value.indexOf(',');
        String seq = c1 > 0 ? value.substring(0, c1) : value;
        String act = c1 > 0 ? value.substring(c1 + 1) : "0";
        sCtrl->params.G[idx].gesture.setGesture(seq.c_str());
        parseButtonAction(sCtrl->params.G[idx].action, act);
        bool ok = rewriteGestures();
        sServer.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "SD write failed — change applied in memory only");
        return;
    }

    // gadget_N_type — set gadget N's hardware type
    if (key.startsWith("gadget_") && key.endsWith("_type")) {
        int idx = key.substring(7, key.length() - 5).toInt();
        if (idx >= 0 && idx < GADGET_COUNT) {
            sGadgets[idx].type = (uint8_t)value.toInt();
            rewriteGadgetConfig();
            sServer.send(200, "text/plain", "OK");
            return;
        }
    }

    // gadget_N_sstr — set gadget N's serial string list (comma-separated 1-based indices)
    if (key.startsWith("gadget_") && key.endsWith("_sstr")) {
        int idx = key.substring(7, key.length() - 5).toInt();
        if (idx >= 0 && idx < GADGET_COUNT) {
            sGadgets[idx].sstrCnt = 0;
            int pos = 0;
            while (pos <= (int)value.length() && sGadgets[idx].sstrCnt < 16) {
                int next = value.indexOf(',', pos);
                String part = (next < 0) ? value.substring(pos) : value.substring(pos, next);
                part.trim();
                if (part.length() > 0) {
                    uint8_t si = (uint8_t)part.toInt();
                    if (si > 0) sGadgets[idx].sstr[sGadgets[idx].sstrCnt++] = si;
                }
                if (next < 0) break;
                pos = next + 1;
            }
            rewriteGadgetConfig();
            sServer.send(200, "text/plain", "OK");
            return;
        }
    }

    // estopstr_del_N / estopstr_N / estopstr_add
    // resumestr_del_N / resumestr_N / resumestr_add
    for (int pass = 0; pass < 2; pass++) {
        const char* prefix   = (pass == 0) ? "estopstr" : "resumestr";
        uint8_t&    cnt      = (pass == 0) ? sCtrl->params.estopCmdCount
                                           : sCtrl->params.resumeCmdCount;
        AmidalaParameters::SafetyCmd* arr =
            (pass == 0) ? sCtrl->params.EstopCmds : sCtrl->params.ResumeCmds;

        String delPfx = String(prefix) + "_del_";
        String setPfx = String(prefix) + "_";

        if (key.startsWith(delPfx)) {
            int idx = key.substring(delPfx.length()).toInt();
            if (idx >= 0 && idx < (int)cnt) {
                memmove(&arr[idx], &arr[idx + 1],
                        (cnt - idx - 1) * sizeof(AmidalaParameters::SafetyCmd));
                cnt--;
                rewriteSafetyCmds();
                sServer.send(200, "text/plain", "OK");
            } else {
                sServer.send(400, "text/plain", "index out of range");
            }
            return;
        }
        if (key == String(prefix) + "_add") {
            if (cnt < MAX_SAFETY_CMDS && value.length() > 0) {
                strncpy(arr[cnt].str, value.c_str(), sizeof(arr[cnt].str) - 1);
                arr[cnt].str[sizeof(arr[cnt].str) - 1] = '\0';
                cnt++;
                rewriteSafetyCmds();
                sServer.send(200, "text/plain", "OK");
            } else {
                sServer.send(cnt >= MAX_SAFETY_CMDS ? 507 : 400,
                             "text/plain",
                             cnt >= MAX_SAFETY_CMDS ? "list full" : "empty string");
            }
            return;
        }
        if (key.startsWith(setPfx) && key.length() > setPfx.length() &&
            isDigit(key.charAt(setPfx.length()))) {
            int idx = key.substring(setPfx.length()).toInt();
            if (idx >= 0 && idx < (int)cnt && value.length() > 0) {
                strncpy(arr[idx].str, value.c_str(), sizeof(arr[idx].str) - 1);
                arr[idx].str[sizeof(arr[idx].str) - 1] = '\0';
                rewriteSafetyCmds();
                sServer.send(200, "text/plain", "OK");
            } else {
                sServer.send(400, "text/plain", "index out of range or empty");
            }
            return;
        }
    }

    // Generic key=value — delegate to processConfig
    String cmd = key + "=" + value;
    if (!sCtrl->fConfig.processConfig(cmd.c_str())) {
        sServer.send(400, "text/plain", "unknown setting: " + key);
        return;
    }
    if (!updateConfigFile(key.c_str(), value.c_str())) {
        sServer.send(500, "text/plain", "SD write failed — change applied in memory only");
        return;
    }
    // Apply BT controller enable/disable immediately — no reboot required.
    if (key == "btcontrolleron") {
        if (sCtrl->params.btcontrolleron) {
            gBTGamepad.setup();
            gBTGamepad.setTargetAddr(sCtrl->params.btaddr);
        } else {
            gBTGamepad.disable();
        }
    }
    // WCB Client: construct+join live on first enable — safe, since
    // construction only ever happens once (WCB_Client has no reconfigure
    // API). Editing an identity field after the client already exists can't
    // be applied live either way, so it's persisted above but flagged for a
    // reboot instead of attempted immediately. outboundserial is NOT an
    // identity field — it's applied live every animate() tick by
    // WCBClientController::poll(), no reboot needed.
    if (key == "wcbenable") {
#ifndef VMUSIC_SERIAL
        if (sCtrl->params.wcbenable && !sCtrl->fWCB.isRunning()) {
            sCtrl->fWCB.begin(sCtrl->params, sCtrl->fHCR, sCtrl->fConsole);
        }
#endif
    } else if ((key == "wcboct2" || key == "wcboct3" || key == "wcbpassword" ||
                key == "wcbquantity" || key == "wcbid") &&
               sCtrl->fWCB.isRunning()) {
        sCtrl->fWCB.flagRebootRequired();
    }
    sServer.send(200, "text/plain", "OK");
}

// Shared by handleApiEstop()/handleApiResume() below and the OTA upload
// handler (issue #152, see handleUpdateUpload()) so both trigger paths stay
// in sync -- broadcasts across the WCB mesh too, since sendSerialString()
// routes through fWCB.routeOutbound() before falling back to wired serial.
static void triggerEstop() {
    monAppend("! EMERGENCY STOP", 'i');
    if (!sCtrl) return;
    sCtrl->emergencyStop();
    sCtrl->domeEmergencyStop();
    for (uint8_t i = 0; i < sCtrl->params.estopCmdCount; i++)
        sCtrl->sendSerialString(sCtrl->params.EstopCmds[i].str);
}

static void triggerResume() {
    monAppend("RESUME", 'i');
    if (!sCtrl) return;
    sCtrl->enableController();
    sCtrl->enableDomeController();
    for (uint8_t i = 0; i < sCtrl->params.resumeCmdCount; i++)
        sCtrl->sendSerialString(sCtrl->params.ResumeCmds[i].str);
}

static void handleApiEstop() {
    triggerEstop();
    sServer.send(200, "text/plain", "OK");
}

static void handleApiResume() {
    triggerResume();
    sServer.send(200, "text/plain", "OK");
}

// Dedicated restart trigger for the web UI's "Restart Now" button. The only
// other way to reboot from the browser was piggybacking on /api/monitor's
// cmd=reboot passthrough, which hits config.cpp's null-function-pointer
// crash trick with no response flush first. This mirrors the OTA update
// path's clean restart instead (see handleUpdatePost() above): flush a 200
// with Connection: close, give it a moment to actually go out, then restart.
static void handleApiReboot() {
    monAppend("RESTART requested via web UI", 'i');
    sServer.sendHeader("Connection", "close");
    sServer.send(200, "text/plain", "OK");
    delay(200);
    ESP.restart();
}

static void handleApiDome() {
    if (!sCtrl) { sServer.send(500, "text/plain", "no controller"); return; }
    String cmd = sServer.arg("cmd");
    if (cmd.isEmpty()) { sServer.send(400, "text/plain", "missing cmd"); return; }
    String log = "dome=" + cmd;
    monAppend(log.c_str(), 't');
    sCtrl->processDomeCommand(cmd.c_str());
    sServer.send(200, "text/plain", "OK");
}

static void handleApiGadgetCmd() {
    if (!sCtrl) { sServer.send(500, "text/plain", "no controller"); return; }
    String cmd = sServer.arg("cmd");
    if (cmd.length() == 0) { sServer.send(400, "text/plain", "cmd required"); return; }
    sCtrl->sendSerialString(cmd.c_str());
    sServer.send(200, "text/plain", "OK");
}


static void handleApiSerial() {
    if (!sCtrl) { sServer.send(500, "text/plain", "no controller"); return; }
    int idx = sServer.arg("idx").toInt(); // 1-based
    if (idx < 1 || idx > (int)sCtrl->params.serialcount) {
        sServer.send(400, "text/plain", "idx out of range");
        return;
    }
    sCtrl->sendSerialString(sCtrl->params.Str[idx - 1].str);
    sServer.send(200, "text/plain", "OK");
}

static void handleApiHCR() {
    if (!sCtrl) { sServer.send(500, "text/plain", "no controller"); return; }
    String cmd = sServer.arg("cmd");
    if (cmd == "muse") {
        sCtrl->fAudio.toggleMuse();
    } else if (cmd == "emote") {
        uint8_t emotion = (uint8_t)sServer.arg("emotion").toInt();
        uint8_t level   = (uint8_t)sServer.arg("level").toInt();
        sCtrl->fAudio.playEmote(emotion, level);
    } else {
        sServer.send(400, "text/plain", "unknown cmd");
        return;
    }
    sServer.send(200, "text/plain", "OK");
}

static void handleApiVolume() {
    if (!sCtrl) { sServer.send(500, "text/plain", "no controller"); return; }
    int vol = sServer.arg("vol").toInt();
    int ch  = sServer.arg("ch").toInt();
    if (vol < 0 || vol > 100) { sServer.send(400, "text/plain", "vol out of range"); return; }
    if (ch  < 0 || ch  > 4)  { sServer.send(400, "text/plain", "ch out of range");  return; }
    sCtrl->fAudio.setChannelVolume((uint8_t)ch, (uint8_t)vol);
    sServer.send(200, "text/plain", "OK");
}

static void handleApiPins() {
    if (!sCtrl) { sServer.send(500, "application/json", "{}"); return; }
    AmidalaParameters &params = sCtrl->params;
    // dout/ain are variable-length now (issue #133) -- however many pool
    // pins currently have that role, not a fixed 4/2. Same order as
    // buildFullConfigJson()'s doutPins/analogPins so the client can zip
    // labels from /api/config with live values from here by index.
    String json = "{\"dout\":[";
    uint8_t doutCount = countPinsWithRole(params.pinRole, PinRoleType::kDout);
    for (uint8_t i = 0; i < doutCount; i++) {
        if (i > 0) json += ",";
        json += String(digitalRead(nthPinWithRole(params.pinRole, PinRoleType::kDout, i)));
    }
    json += "],\"ain\":[";
    uint8_t analogCount = countPinsWithRole(params.pinRole, PinRoleType::kAnalog);
    for (uint8_t i = 0; i < analogCount; i++) {
        if (i > 0) json += ",";
        json += String(analogRead(nthPinWithRole(params.pinRole, PinRoleType::kAnalog, i)));
    }
    json += "]}";
    sServer.send(200, "application/json", json);
}

static void handleApiMonitorGet() {
    String json = "{\"seq\":";
    json += String(sMonSeq);
    json += ",\"lines\":[";
    uint16_t start = (sMonCount < MON_LINES) ? 0 : sMonHead;
    for (uint16_t i = 0; i < sMonCount; i++) {
        if (i > 0) json += ",";
        uint16_t idx = (start + i) % MON_LINES;
        json += "{\"t\":\"";
        monJsonAppendEscaped(json, sMonBuf[idx].text);
        json += "\",\"c\":\"";
        switch (sMonBuf[idx].cls) {
            case 't': json += "tx"; break;
            case 'r': json += "rx"; break;
            default:  json += "info"; break;
        }
        json += "\"}";
    }
    json += "]}";
    sServer.send(200, "application/json", json);
}

// True if a binary/packet-serial protocol (RoboClaw, Sabertooth, or
// Roboteq-serial) currently occupies `port` -- reading or writing arbitrary
// bytes on such a link races (RX) or corrupts (TX) the motor controller's
// own traffic. Runtime check (issue #147): which port (if either) that is
// now depends on domeSerialPort/driveSerialPort, not a fixed compile-time
// port-to-role mapping.
static bool portCarriesBinaryProtocol(SerialPortId port) {
    if (!sCtrl) return false;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW || DOME_DRIVE == DOME_DRIVE_SABER
    if (sCtrl->params.domeSerialPort == port) return true;
#endif
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER || \
    DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL || \
    DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
    if (sCtrl->params.driveSerialPort == port) return true;
#endif
    return false;
}

static void handleApiMonitorPost() {
    if (!sCtrl) { sServer.send(500, "text/plain", "no controller"); return; }
    String cmd = sServer.arg("cmd");
    if (cmd.isEmpty()) { sServer.send(400, "text/plain", "missing cmd"); return; }

    String tx = "> " + cmd;
    monAppend(tx.c_str(), 't');

    if (!sCtrl->fConfig.processConfig(cmd.c_str()))
        monAppend("  (unknown command)", 'i');

    // Also transmit to whichever physical/mesh channels the monitor's S0/S1/
    // S2/WCB toggle buttons had selected when Send was pressed -- independent
    // of the local processConfig() interpretation above, this is a manual
    // multicast to actual hardware, e.g. to replay/test a gadget command
    // exactly as sendSerialString()/routeOutbound() would send it.
    AmidalaParameters &params = sCtrl->params;
    if (sServer.arg("s0") == "1") {
        sendSerialStringTo(SERIAL, cmd.c_str(), params.serialdelim, params.serialeol);
        monAppend(("S0: " + cmd).c_str(), 't');
    }
    // Skipped when the port is claimed by a binary packet-serial protocol
    // (RoboClaw/Sabertooth/Roboteq-serial, issue #147) -- writing arbitrary
    // text into that link could corrupt a motor command mid-packet. See
    // portCarriesBinaryProtocol()'s matching check on the RX side above.
    if (sServer.arg("s1") == "1" && !portCarriesBinaryProtocol(SerialPortId::kSerial1)) {
        sendSerialStringTo(Serial1, cmd.c_str(), params.serialdelim, params.serialeol);
        monAppend(("S1: " + cmd).c_str(), 't');
    }
    if (params.auxserial3 && sServer.arg("s2") == "1" &&
        !portCarriesBinaryProtocol(SerialPortId::kSerial2)) {
        sendSerialStringTo(AUX_SERIAL, cmd.c_str(), params.serialdelim, params.serialeol);
        monAppend(("S2: " + cmd).c_str(), 't');
    }
    if (sServer.arg("wcb") == "1") {
        // Explicit manual selection -- always attempt the mesh regardless of
        // outboundserial (that setting only governs the *automatic* gadget/
        // HCR routing choice). Silently no-ops if the mesh isn't live, same
        // as an unavailable S1/S2 above -- no log line, nothing was sent.
        if (sCtrl->fWCB.routeOutbound(cmd.c_str(), true, params.serialdelim))
            monAppend(("MESH: " + cmd).c_str(), 't');
    }

    sServer.send(200, "text/plain", "OK");
}

// ---------------------------------------------------------------------------
// Firmware update (OTA)
// ---------------------------------------------------------------------------

// Uses the standard Arduino ESP32 Update class which writes to the inactive
// OTA partition (ota_0 / ota_1) and switches the boot slot on completion.
// This requires the OTA partition layout (ota_8MB.csv / ota_16MB.csv).

static void handleUpdateUpload() {
    HTTPUpload& upload = sServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        monAppend("OTA: upload started", 'i');
        // E-stop as soon as the flash upload begins (issue #152) --
        // sServer.handleClient() blocks synchronously reading this upload's
        // chunks for the whole request, so AnimatedEvent::process()'s other
        // animate() calls (joystick/throttle updates, etc.) don't run again
        // until it completes. Without this, the droid keeps executing
        // whatever was last commanded for the entire flash. Broadcasts
        // across the WCB mesh too, so other boards stop as well.
        triggerEstop();
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            monAppend("OTA: begin failed", 'i');
            triggerResume();  // nothing is actually happening -- don't leave it e-stopped
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            monAppend("OTA: write error", 'i');
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            monAppend("OTA: flash complete, restarting", 'i');
            // Resume before restarting (issue #152) -- this board is about
            // to reboot into new firmware regardless (see
            // handleUpdatePost()), but other boards on the WCB mesh need
            // the explicit broadcast to start responding to input again.
            triggerResume();
        } else {
            monAppend("OTA: end failed", 'i');
            triggerResume();  // flash didn't take -- old firmware still active, safe to resume
        }
    }
}

static void handleUpdatePost() {
    if (Update.hasError()) {
        sServer.send(500, "text/plain", "UPDATE FAILED: see serial monitor");
    } else {
        sServer.sendHeader("Connection", "close");
        sServer.send(200, "text/plain", "OK");
        delay(200);
        ESP.restart();
    }
}

// ---------------------------------------------------------------------------
// Page handlers
// ---------------------------------------------------------------------------

static void handleHome() {
    sServer.send_P(200, "text/html", WEB_PAGE_HOME);
}

static void handleConfigGeneral()       { sServer.send_P(200, "text/html", WEB_PAGE_GENERAL);        }
static void handleConfigWifi()          { sServer.send_P(200, "text/html", WEB_PAGE_WIFI);            }
static void handleConfigXbee()          { sServer.send_P(200, "text/html", WEB_PAGE_XBEE);            }
static void handleConfigAudio()         { sServer.send_P(200, "text/html", WEB_PAGE_AUDIO);           }
static void handleConfigRcRadio()       { sServer.send_P(200, "text/html", WEB_PAGE_RC_RADIO);        }
static void handleConfigDome()          { sServer.send_P(200, "text/html", WEB_PAGE_DOME);            }
static void handleConfigSerialStrings() { sServer.send_P(200, "text/html", WEB_PAGE_SERIAL_STRINGS);  }
static void handleConfigServos()        { sServer.send_P(200, "text/html", WEB_PAGE_SERVOS);           }
static void handleConfigPins()          { sServer.send_P(200, "text/html", WEB_PAGE_PINS);             }
static void handleConfigSerialPorts()   { sServer.send_P(200, "text/html", WEB_PAGE_SERIAL_PORTS);      }
static void handleConfigControllers()   { sServer.send_P(200, "text/html", WEB_PAGE_CONTROLLERS);     }
static void handleMonitor()             { sServer.send_P(200, "text/html", WEB_PAGE_MONITOR);         }
static void handleUpdatePage()          { sServer.send_P(200, "text/html", WEB_PAGE_UPDATE);          }
static void handleDroidControl()        { sServer.send_P(200, "text/html", WEB_PAGE_DROID_CONTROL);   }
static void handleConfigGadgets()       { sServer.send_P(200, "text/html", WEB_PAGE_GADGETS);         }
static void handleSafety()             { sServer.send_P(200, "text/html", WEB_PAGE_SAFETY);           }
static void handleComingSoon()          { sServer.send_P(200, "text/html", WEB_PAGE_COMING_SOON);     }
static void handleDiagnostics()         { sServer.send_P(200, "text/html", WEB_PAGE_DIAGNOSTICS);      }
static void handleConfigConnectivity()  { sServer.send_P(200, "text/html", WEB_PAGE_CONFIG_CONNECTIVITY); }

// ---------------------------------------------------------------------------
// BT API endpoints
// ---------------------------------------------------------------------------

static void handleApiBtStatus() {
    bool enabled = sCtrl && sCtrl->params.btcontrolleron;
    String json = "{";
    json += "\"enabled\":";
    json += enabled ? "true" : "false";
    json += ",\"connected\":";
    json += gBTGamepad.isConnected() ? "true" : "false";
    json += ",\"addr\":\"";
    json += String(gBTGamepad.connectedAddr());
    json += "\",\"scanning\":";
    json += gBTGamepad.isScanRunning() ? "true" : "false";
    json += ",\"local_addr\":\"";
    // BLEDevice may not be initialized yet if the controller is disabled.
    json += enabled ? String(BLEDevice::getAddress().toString().c_str()) : String("");
    json += "\"}";
    sServer.send(200, "application/json", json);
}

static void handleApiWcbStatus() {
    if (!sCtrl) {
        sServer.send(200, "application/json", buildWCBStatusJson(false, false, false, false, 0, 0, 0, false));
        return;
    }
    sServer.send(200, "application/json", sCtrl->fWCB.statusJson(sCtrl->params));
}

static void handleApiBtScan() {
    if (!sCtrl || !sCtrl->params.btcontrolleron) {
        sServer.send(400, "text/plain", "Bluetooth controller is disabled");
        return;
    }
    gBTGamepad.requestPairing();
    // Immediately return — results are polled via /api/bt/status
    sServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleApiBtResults() {
    String json = "[";
    for (int i = 0; i < gBTGamepad.getScanResultCount(); i++) {
        if (i > 0) json += ",";
        const BTScanResult& r = gBTGamepad.getScanResults()[i];
        json += "{\"addr\":\"";
        json += String(r.addr);
        json += "\",\"name\":\"";
        json += String(r.name);
        json += "\",\"rssi\":";
        json += String(r.rssi);
        json += "}";
    }
    json += "]";
    sServer.send(200, "application/json", json);
}

static void handleApiBtPair() {
    if (!sCtrl || !sCtrl->params.btcontrolleron) {
        sServer.send(400, "text/plain", "Bluetooth controller is disabled");
        return;
    }
    String addr = sServer.arg("addr");
    if (addr.length() == 0) {
        sServer.send(400, "text/plain", "addr required");
        return;
    }
    // Persist to config file.
    if (sCtrl) {
        strncpy(sCtrl->params.btaddr, addr.c_str(), sizeof(sCtrl->params.btaddr) - 1);
        sCtrl->params.btaddr[sizeof(sCtrl->params.btaddr) - 1] = '\0';
        // Write to SD.
        updateConfigKey("btaddr=", addr.c_str());
    }
    gBTGamepad.pairWith(addr.c_str());
    monAppend(("BT: pairing with " + addr).c_str(), 'i');
    sServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleApiBtForget() {
    if (sCtrl) {
        sCtrl->params.btaddr[0] = '\0';
        removeConfigKey("btaddr=");
    }
    gBTGamepad.forget();
    monAppend("BT: cleared pairing", 'i');
    sServer.send(200, "application/json", "{\"ok\":true}");
}

// ---------------------------------------------------------------------------
// AmidalaWiFiAP
// ---------------------------------------------------------------------------

// arduino-esp32 mutes the underlying esp_wifi driver's own "wifi" tag logging
// by default, so client join/auth failures are otherwise completely silent on
// the console -- log AP client connect/disconnect (with the raw 802.11
// disconnect reason code) so a failed join can actually be diagnosed.
static const char* wifiDisconnectReasonStr(uint16_t reason) {
    switch (reason) {
        case 2:  return "AUTH_EXPIRE";
        case 6:  return "CLASS2_FRAME_FROM_NONAUTH_STA";
        case 7:  return "CLASS3_FRAME_FROM_NONASSOC_STA";
        case 8:  return "DISASSOC_STA_HAS_LEFT";
        case 14: return "MIC_FAILURE (wrong password)";
        case 15: return "4WAY_HANDSHAKE_TIMEOUT (wrong password or too weak a signal)";
        case 16: return "GROUP_KEY_UPDATE_TIMEOUT";
        case 202:return "AUTH_FAIL";
        default: return "unknown";
    }
}

static void onWiFiApStaEvent(arduino_event_id_t event, arduino_event_info_t info) {
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        Serial.printf(
            "[WiFi] station " MACSTR " connected (AID %d)\n",
            MAC2STR(info.wifi_ap_staconnected.mac), info.wifi_ap_staconnected.aid
        );
    } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        Serial.printf(
            "[WiFi] station " MACSTR " disconnected, reason=%d (%s)\n",
            MAC2STR(info.wifi_ap_stadisconnected.mac), info.wifi_ap_stadisconnected.reason,
            wifiDisconnectReasonStr(info.wifi_ap_stadisconnected.reason)
        );
    }
}

void AmidalaWiFiAP::begin(const char* ssid, const char* password, AmidalaController* ctrl) {
    sCtrl = ctrl;
    sCtrl->fSerialTxLog = [](const char* s, bool wentToMesh) {
        char buf[MON_LINE_LEN];
        snprintf(buf, sizeof(buf), "%s %s", wentToMesh ? "MESH:" : "S0:", s);
        monAppend(buf, 't');
    };
    loadGadgetConfig();
    sUserSerialCount = sCtrl->params.serialcount; // snapshot before builtin injection
    injectBuiltinSerialCmds();

    esp_log_level_set("wifi", ESP_LOG_INFO);
    WiFi.onEvent(onWiFiApStaEvent, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(onWiFiApStaEvent, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

    // AP_STA (not plain AP): lets WCB Client's begin() (called later in
    // setup(), after this) detect the already-active SoftAP and ride its
    // radio channel via ESP-NOW instead of forcing pure STA and dropping
    // the AP. Harmless when WCB Client is disabled -- STA just sits idle.
    WiFi.mode(WIFI_AP_STA);
    // Modem power-save timing on the AP interface can miss/mistime the WPA2
    // 4-way handshake, intermittently making a correct password appear to be
    // rejected (a long-standing arduino-esp32 quirk, not password-related --
    // see espressif/arduino-esp32#5806). This robot isn't power-constrained
    // enough for WiFi power-save to be worth that tradeoff.
    esp_wifi_set_ps(WIFI_PS_NONE);
    // WCB_Client has no way to be told which WiFi channel to use yet (that's
    // planned for a future WCBClient release) -- until then, every other
    // board on the mesh with no SoftAP of its own lands on whatever the
    // ESP-NOW/WiFi stack defaults to with no channel specified, which is
    // channel 1. Since ESP-NOW rides whatever channel this AP starts on, it
    // has to match that exactly or this board simply won't hear the rest of
    // the mesh. params.wifichannel defaults to 1 (see AmidalaParameters::init())
    // to match that, and is user-configurable (1-13) for the rare case where
    // WiFi interference matters more than mesh compatibility -- passed
    // explicitly rather than relying on WiFi.softAP()'s own default
    // parameter, which is also 1 today but is exactly the kind of implicit
    // cross-library coincidence that stops matching the moment either
    // library's default changes.
    bool apOk = WiFi.softAP(ssid, password, sCtrl->params.wifichannel);
    IPAddress ip = WiFi.softAPIP();
    Serial.print(F("[WiFi] AP \""));
    Serial.print(ssid);
    Serial.print(F("\" @ "));
    Serial.print(ip);
    Serial.printf(" (softAP()=%s)\n", apOk ? "ok" : "FAILED");
    {
        // This whole block previously only went to the physical serial port
        // -- with no laptop plugged in at a show, there was no way to tell
        // after the fact whether the AP or mDNS actually came up. Mirror it
        // to the web Monitor page too.
        char buf[MON_LINE_LEN];
        snprintf(buf, sizeof(buf), "WiFi AP \"%s\" @ %s (softAP()=%s)",
                 ssid, ip.toString().c_str(), apOk ? "ok" : "FAILED");
        monAppend(buf, 'i');
    }

    wifi_config_t apConf;
    if (esp_wifi_get_config(WIFI_IF_AP, &apConf) == ESP_OK) {
        Serial.printf(
            "[WiFi] applied config: ssid=\"%s\" authmode=%d channel=%d password=\"%s\"\n",
            apConf.ap.ssid, apConf.ap.authmode, apConf.ap.channel, apConf.ap.password
        );
    }

    // mDNS: always advertise http://amidala.local, regardless of the AP's
    // configured SSID (see kMdnsHostname above). ESP32 SoftAP multicast
    // delivery to stations is known to be unreliable (client WiFi power-save
    // can delay multicast frames until the next DTIM beacon, and some OSes
    // just don't attempt <name>.local resolution on a network with no
    // internet route) -- the IP address logged above always works and is
    // the reliable fallback. This just makes sure we can tell, after the
    // fact, whether MDNS.begin() itself ever actually started rather than
    // the resolution simply not reaching the client.
    if (MDNS.begin(kMdnsHostname)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[WiFi] mDNS: http://%s.local\n", kMdnsHostname);
        char buf[MON_LINE_LEN];
        snprintf(buf, sizeof(buf), "mDNS started: http://%s.local (or use http://%s)",
                 kMdnsHostname, ip.toString().c_str());
        monAppend(buf, 'i');
    } else {
        Serial.println(F("[WiFi] mDNS: FAILED to start"));
        char buf[MON_LINE_LEN];
        snprintf(buf, sizeof(buf), "mDNS FAILED to start -- use http://%s instead",
                 ip.toString().c_str());
        monAppend(buf, 'i');
    }

    // Pages
    sServer.on("/",                      HTTP_GET, handleHome);
    sServer.on("/index.html",            HTTP_GET, handleHome);
    sServer.on("/config/general",        HTTP_GET, handleConfigGeneral);
    sServer.on("/config/wifi",           HTTP_GET, handleConfigWifi);
    sServer.on("/config/xbee",           HTTP_GET, handleConfigXbee);
    sServer.on("/config/audio",          HTTP_GET, handleConfigAudio);
    sServer.on("/config/rc-radio",       HTTP_GET, handleConfigRcRadio);
    sServer.on("/config/dome",           HTTP_GET, handleConfigDome);
    sServer.on("/config/controllers",    HTTP_GET, handleConfigControllers);
    sServer.on("/config/buttons",        HTTP_GET, handleConfigControllers);  // legacy alias
    sServer.on("/config/servos",         HTTP_GET, handleConfigServos);
    sServer.on("/config/pins",           HTTP_GET, handleConfigPins);
    sServer.on("/config/serial-ports",   HTTP_GET, handleConfigSerialPorts);
    sServer.on("/config/serial-strings", HTTP_GET, handleConfigSerialStrings);
    sServer.on("/config/gadgets",        HTTP_GET, handleConfigGadgets);
    sServer.on("/droid-control",        HTTP_GET, handleDroidControl);
    sServer.on("/safety",               HTTP_GET,  handleSafety);
    sServer.on("/monitor",              HTTP_GET,  handleMonitor);
    sServer.on("/api/monitor",          HTTP_GET,  handleApiMonitorGet);
    sServer.on("/api/monitor",          HTTP_POST, handleApiMonitorPost);
    sServer.on("/update",               HTTP_GET,  handleUpdatePage);
    sServer.on("/update",               HTTP_POST, handleUpdatePost, handleUpdateUpload);

    // REST API
    sServer.on("/api/info",   HTTP_GET,  handleApiInfo);
    sServer.on("/api/estop",  HTTP_POST, handleApiEstop);
    sServer.on("/api/resume", HTTP_POST, handleApiResume);
    sServer.on("/api/reboot", HTTP_POST, handleApiReboot);
    sServer.on("/api/dome",   HTTP_POST, handleApiDome);
    sServer.on("/api/gadget-cmd", HTTP_POST, handleApiGadgetCmd);
    sServer.on("/api/serial",     HTTP_POST, handleApiSerial);
    sServer.on("/api/hcr",        HTTP_POST, handleApiHCR);
    sServer.on("/api/volume",     HTTP_POST, handleApiVolume);
    sServer.on("/api/config", HTTP_GET,  handleApiConfigGet);
    sServer.on("/api/config", HTTP_POST, handleApiConfigPost);
    sServer.on("/api/pins",   HTTP_GET,  handleApiPins);
    sServer.on("/diagnostics", HTTP_GET, handleDiagnostics);
    sServer.on("/config/connectivity",   HTTP_GET,  handleConfigConnectivity);
    sServer.on("/api/bt/status",         HTTP_GET,  handleApiBtStatus);
    sServer.on("/api/bt/scan",           HTTP_POST, handleApiBtScan);
    sServer.on("/api/bt/results",        HTTP_GET,  handleApiBtResults);
    sServer.on("/api/bt/pair",           HTTP_POST, handleApiBtPair);
    sServer.on("/api/bt/forget",         HTTP_POST, handleApiBtForget);
    sServer.on("/api/wcb/status",        HTTP_GET,  handleApiWcbStatus);

    sServer.onNotFound([]() { sServer.send(404, "text/plain", "Not found"); });

    sServer.begin();
    Serial.println(F("[WiFi] HTTP server started"));
}

// Drain one serial port into the monitor.  Accumulates printable bytes into a
// line buffer and flushes on newline, when the buffer is full, or after 100 ms
// of silence.  Non-printable bytes (other than \r/\n) are silently skipped.
// The byte-level state machine lives in monitor_drain.h so it can be unit
// tested without a real HardwareSerial.
struct SerialMonPort {
    HardwareSerial* port;
    MonDrainState   state;
};

static void monDrainSerial(SerialMonPort& p) {
    monDrainSeedLabel(p.state);
    bool flushed = false;
    while (p.port->available()) {
        uint8_t b = (uint8_t)p.port->read();
        if (monDrainByte(p.state, b, millis())) flushed = true;
    }
    if (!flushed) monDrainSilenceCheck(p.state, millis());
}

void AmidalaWiFiAP::handle() {
    sServer.handleClient();

    // Serial RX monitoring. Skip a port's binary-protocol traffic (RoboClaw/
    // Sabertooth/Roboteq-serial, issue #147, see portCarriesBinaryProtocol())
    // -- reading its bytes here would race the motor controller's own reads
    // on the same UART FIFO, not just garble the log. Runtime check since
    // which port (if either) that is now depends on domeSerialPort/
    // driveSerialPort, not a fixed compile-time port-to-role mapping.
    static SerialMonPort sPortWCB = { &Serial0, {} };
    static SerialMonPort sPortS1  = { &Serial1, {} };
    static SerialMonPort sPortAux = { &Serial2, {} };
    static bool sMonInit = false;
    if (!sMonInit) {
        monDrainInit(sPortWCB.state, "S0: ");
        monDrainInit(sPortS1.state, "S1: ");
        monDrainInit(sPortAux.state, "S2: ");
        sMonInit = true;
    }

    monDrainSerial(sPortWCB);
    if (!portCarriesBinaryProtocol(SerialPortId::kSerial1)) {
        monDrainSerial(sPortS1);
    }
    if (sCtrl && sCtrl->params.auxserial3 && !portCarriesBinaryProtocol(SerialPortId::kSerial2)) {
        monDrainSerial(sPortAux);
    }
    if (sCtrl) sCtrl->fConsole.tickMonitor();
}

#else  // UNIT_TEST

void AmidalaWiFiAP::begin(const char*, const char*, AmidalaController*) {}
void AmidalaWiFiAP::handle() {}

#endif
