#include "wifi_ap.h"
#include "web_pages.h"

#ifndef UNIT_TEST
#include <EEPROM.h>     // must precede params.h (via web_api.h)
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "esp_partition.h"
#include "SD.h"
#include "web_api.h"
#include "controller.h"
#include "drive_config.h"

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

// ---------------------------------------------------------------------------
// SD card config write-back
// ---------------------------------------------------------------------------

#include "config_file.h"

// ---------------------------------------------------------------------------
// Gadget configuration
// ---------------------------------------------------------------------------

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
    String sstrs;
    for (uint8_t i = 0; i < sUserSerialCount; i++) {
        sstrs += "sstr=";
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

// Serialize a ButtonAction's parameters as "type[,p1[,p2]]"
static String buttonActionStr(const ButtonAction& b) {
    String s = String(b.action);
    switch (b.action) {
    case ButtonAction::kSerialStr: s += "," + String(b.serial.serialstr);                                    break;
    case ButtonAction::kHCREmote:  s += "," + String(b.emote.emotion) + "," + String(b.emote.level);        break;
    case ButtonAction::kDomeCmd:   s += "," + String(b.dome.subcmd);                                         break;
    default: break;
    }
    return s;
}

// Parse "type[,p1[,p2]]" value string into a ButtonAction
static void parseButtonAction(ButtonAction& b, const String& value) {
    memset(&b, 0, sizeof(b));
    if (value.isEmpty() || value == "0") return;
    int c1   = value.indexOf(',');
    int type = value.substring(0, c1 > 0 ? c1 : (int)value.length()).toInt();
    switch (type) {
    case ButtonAction::kSerialStr:
        b.action = type;
        if (c1 > 0) b.serial.serialstr = (uint8_t)value.substring(c1 + 1).toInt();
        break;
    case ButtonAction::kHCREmote: {
        b.action = type;
        int c2 = c1 > 0 ? value.indexOf(',', c1 + 1) : -1;
        if (c1 > 0) b.emote.emotion = (uint8_t)value.substring(c1 + 1, c2 > 0 ? c2 : (int)value.length()).toInt();
        if (c2 > 0) b.emote.level   = (uint8_t)value.substring(c2 + 1).toInt();
        break;
    }
    case ButtonAction::kHCRMuse:
        b.action = type;
        break;
    case ButtonAction::kDomeCmd:
        b.action = type;
        if (c1 > 0) b.dome.subcmd = (uint8_t)value.substring(c1 + 1).toInt();
        break;
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
            if (!line.startsWith("b=") && !line.startsWith("lb=") && !line.startsWith("ab="))
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

    sServer.send(200, "application/json",
        buildInfoJson(drive, dome, audio,
                      sCtrl->params.wifiSSID,
                      WiFi.softAPIP().toString().c_str(),
                      sCtrl->params.serialcount,
                      ESP.getFreeHeap()));
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
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "applied but SD write failed");
        return;
    }

    // sstr_del_N — delete serial string at index N, shift remainder down
    if (key.startsWith("sstr_del_")) {
        int idx = key.substring(9).toInt();
        // Only allow deletion of user-defined strings (not builtin injected ones)
        if (idx < 0 || idx >= (int)sUserSerialCount) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
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
        bool ok = rewriteSerialStrings();
        if (gadgetsDirty) rewriteGadgetConfig();
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "deleted but SD write failed");
        return;
    }

    // sstr_N — update serial string at index N (or append when N == sUserSerialCount)
    if (key.startsWith("sstr_")) {
        int idx = key.substring(5).toInt();
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
            // Update gadget sstr indices: entries at or above new position shift up
            for (int g = 0; g < GADGET_COUNT; g++)
                for (uint8_t k = 0; k < sGadgets[g].sstrCnt; k++)
                    if (sGadgets[g].sstr[k] > (uint8_t)idx)
                        sGadgets[g].sstr[k]++;
            sUserSerialCount++;
        }
        setSerialStringFields(sCtrl->params.Str[idx], value.c_str());
        bool ok = rewriteSerialStrings();
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "applied but SD write failed");
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
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "deleted but SD write failed");
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
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "applied but SD write failed");
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
            sServer.send((ok && ok2) ? 200 : 207, "text/plain", (ok && ok2) ? "OK" : "applied but SD write failed");
            return;
        }
        if (value == "mutebutton") {
            sCtrl->params.mutebutton = (uint8_t)btnNum;
            memset(&sCtrl->params.B[idx], 0, sizeof(ButtonAction));
            bool ok  = updateConfigFile("mutebutton", String(btnNum).c_str());
            bool ok2 = rewriteButtons();
            sServer.send((ok && ok2) ? 200 : 207, "text/plain", (ok && ok2) ? "OK" : "applied but SD write failed");
            return;
        }

        // Regular action: clear alt/mute role if this button previously held it
        if (layer == "press") {
            if (sCtrl->params.altbtn    == (uint8_t)btnNum) { sCtrl->params.altbtn    = 0; updateConfigFile("altbtn",    "0"); }
            if (sCtrl->params.mutebutton == (uint8_t)btnNum) { sCtrl->params.mutebutton = 0; updateConfigFile("mutebutton","0"); }
        }
        ButtonAction* arr = (layer == "long") ? sCtrl->params.LB
                          : (layer == "alt")  ? sCtrl->params.AB
                          :                     sCtrl->params.B;
        parseButtonAction(arr[idx], value);
        bool ok = rewriteButtons();
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "applied but SD write failed");
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
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "deleted but SD write failed");
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
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "applied but SD write failed");
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
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "applied but SD write failed");
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
        sServer.send(207, "text/plain", "applied but SD write failed");
        return;
    }
    sServer.send(200, "text/plain", "OK");
}

static void handleApiEstop() {
    monAppend("! EMERGENCY STOP", 'i');
    if (sCtrl) {
        sCtrl->emergencyStop();
        sCtrl->domeEmergencyStop();
        for (uint8_t i = 0; i < sCtrl->params.estopCmdCount; i++)
            sCtrl->sendSerialString(sCtrl->params.EstopCmds[i].str);
    }
    sServer.send(200, "text/plain", "OK");
}

static void handleApiResume() {
    monAppend("RESUME", 'i');
    if (sCtrl) {
        sCtrl->enableController();
        sCtrl->enableDomeController();
        for (uint8_t i = 0; i < sCtrl->params.resumeCmdCount; i++)
            sCtrl->sendSerialString(sCtrl->params.ResumeCmds[i].str);
    }
    sServer.send(200, "text/plain", "OK");
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

static void handleApiPins() {
    String json = "{\"dout\":[";
    json += String(digitalRead(DOUT1_PIN)); json += ",";
    json += String(digitalRead(DOUT2_PIN)); json += ",";
    json += String(digitalRead(DOUT3_PIN)); json += ",";
    json += String(digitalRead(DOUT4_PIN));
    json += "],\"ain\":[";
    json += String(analogRead(ANALOG1_PIN)); json += ",";
    json += String(analogRead(ANALOG2_PIN));
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
        for (const char* p = sMonBuf[idx].text; *p; p++) {
            if (*p == '"' || *p == '\\') json += '\\';
            json += *p;
        }
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

static void handleApiMonitorPost() {
    if (!sCtrl) { sServer.send(500, "text/plain", "no controller"); return; }
    String cmd = sServer.arg("cmd");
    if (cmd.isEmpty()) { sServer.send(400, "text/plain", "missing cmd"); return; }

    String tx = "> " + cmd;
    monAppend(tx.c_str(), 't');

    if (!sCtrl->fConfig.processConfig(cmd.c_str()))
        monAppend("  (unknown command)", 'i');

    sServer.send(200, "text/plain", "OK");
}

// ---------------------------------------------------------------------------
// Firmware update (OTA)
// ---------------------------------------------------------------------------

// Write directly to the factory partition using esp_partition_write(), bypassing
// esp_ota which requires an otadata partition we deliberately don't have.
// Sectors are erased on demand as data arrives.  Writes are kept 4-byte aligned
// by buffering any trailing bytes across chunk boundaries.

static constexpr uint32_t    kFwSectorSize = 4096;
static const esp_partition_t* sFwPart      = nullptr;
static uint32_t               sFwOffset    = 0;
static uint32_t               sFwEraseEnd  = 0;
static bool                   sFwOk        = false;
static uint8_t                sFwTail[4];
static uint8_t                sFwTailLen   = 0;

static bool fwFlushBytes(const uint8_t* data, size_t len) {
    uint32_t need = sFwOffset + (uint32_t)len;
    while (sFwEraseEnd < need) {
        if (esp_partition_erase_range(sFwPart, sFwEraseEnd, kFwSectorSize) != ESP_OK)
            return false;
        sFwEraseEnd += kFwSectorSize;
    }
    if (esp_partition_write(sFwPart, sFwOffset, data, len) != ESP_OK)
        return false;
    sFwOffset += (uint32_t)len;
    return true;
}

static void handleUpdateUpload() {
    HTTPUpload& upload = sServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        monAppend("OTA: upload started", 'i');
        sFwOk      = false;
        sFwOffset  = 0;
        sFwEraseEnd = 0;
        sFwTailLen = 0;
        sFwPart    = esp_partition_find_first(
                         ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "factory");
        if (!sFwPart) monAppend("OTA: factory partition not found", 'i');

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!sFwPart) return;
        const uint8_t* src = upload.buf;
        size_t         rem = upload.currentSize;

        // Drain the carry buffer first.
        if (sFwTailLen > 0) {
            uint8_t word[4];
            memcpy(word, sFwTail, sFwTailLen);
            size_t need = 4 - sFwTailLen;
            if (rem < need) {
                memcpy(sFwTail + sFwTailLen, src, rem);
                sFwTailLen += (uint8_t)rem;
                return;
            }
            memcpy(word + sFwTailLen, src, need);
            if (!fwFlushBytes(word, 4)) {
                monAppend("OTA: write error", 'i'); sFwPart = nullptr; return;
            }
            src += need; rem -= need; sFwTailLen = 0;
        }

        // Write the 4-byte-aligned bulk.
        size_t aligned = rem & ~3u;
        if (aligned > 0) {
            if (!fwFlushBytes(src, aligned)) {
                monAppend("OTA: write error", 'i'); sFwPart = nullptr; return;
            }
            src += aligned; rem -= aligned;
        }

        // Carry ≤3 trailing bytes into the next chunk.
        if (rem > 0) {
            memcpy(sFwTail, src, rem);
            sFwTailLen = (uint8_t)rem;
        }

    } else if (upload.status == UPLOAD_FILE_END) {
        if (!sFwPart) return;
        // Flush any remaining bytes padded with 0xFF (erased flash value).
        if (sFwTailLen > 0) {
            uint8_t padded[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            memcpy(padded, sFwTail, sFwTailLen);
            if (!fwFlushBytes(padded, 4)) {
                monAppend("OTA: write error", 'i'); sFwPart = nullptr; return;
            }
        }
        monAppend("OTA: flash complete, restarting", 'i');
        sFwOk   = true;
        sFwPart = nullptr;
    }
}

static void handleUpdatePost() {
    if (sFwOk) {
        sServer.send(200, "text/plain", "OK");
        delay(200);
        ESP.restart();
    } else {
        String err = "UPDATE FAILED: FLASH FAILED: see serial monitor";
        monAppend(err.c_str(), 'i');
        sServer.send(500, "text/plain", err);
    }
}

// ---------------------------------------------------------------------------
// Page handlers
// ---------------------------------------------------------------------------

static void handleHome() {
    sServer.send(200, "text/html", WEB_PAGE_HOME);
}

static void handleConfigGeneral()       { sServer.send(200, "text/html", WEB_PAGE_GENERAL);        }
static void handleConfigWifi()          { sServer.send(200, "text/html", WEB_PAGE_WIFI);            }
static void handleConfigXbee()          { sServer.send(200, "text/html", WEB_PAGE_XBEE);            }
static void handleConfigAudio()         { sServer.send(200, "text/html", WEB_PAGE_AUDIO);           }
static void handleConfigRcRadio()       { sServer.send(200, "text/html", WEB_PAGE_RC_RADIO);        }
static void handleConfigDome()          { sServer.send(200, "text/html", WEB_PAGE_DOME);            }
static void handleConfigSerialStrings() { sServer.send(200, "text/html", WEB_PAGE_SERIAL_STRINGS);  }
static void handleConfigServos()        { sServer.send(200, "text/html", WEB_PAGE_SERVOS);           }
static void handleConfigControllers()   { sServer.send(200, "text/html", WEB_PAGE_CONTROLLERS);     }
static void handleMonitor()             { sServer.send(200, "text/html", WEB_PAGE_MONITOR);         }
static void handleUpdatePage()          { sServer.send(200, "text/html", WEB_PAGE_UPDATE);          }
static void handleDroidControl()        { sServer.send(200, "text/html", WEB_PAGE_DROID_CONTROL);   }
static void handleConfigGadgets()       { sServer.send(200, "text/html", WEB_PAGE_GADGETS);         }
static void handleSafety()             { sServer.send(200, "text/html", WEB_PAGE_SAFETY);           }
static void handleComingSoon()          { sServer.send(200, "text/html", WEB_PAGE_COMING_SOON);     }
static void handleDiagnostics()         { sServer.send(200, "text/html", WEB_PAGE_DIAGNOSTICS);      }

// ---------------------------------------------------------------------------
// AmidalaWiFiAP
// ---------------------------------------------------------------------------

void AmidalaWiFiAP::begin(const char* ssid, const char* password, AmidalaController* ctrl) {
    sCtrl = ctrl;
    sCtrl->fSerialTxLog = [](const char* s) {
        char buf[MON_LINE_LEN];
        snprintf(buf, sizeof(buf), "S0: %s", s);
        monAppend(buf, 't');
    };
    loadGadgetConfig();
    sUserSerialCount = sCtrl->params.serialcount; // snapshot before builtin injection
    injectBuiltinSerialCmds();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    IPAddress ip = WiFi.softAPIP();
    Serial.print(F("[WiFi] AP \""));
    Serial.print(ssid);
    Serial.print(F("\" @ "));
    Serial.println(ip);

    // mDNS: advertise <ssid>.local
    if (MDNS.begin(ssid)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[WiFi] mDNS: http://%s.local\n", ssid);
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
    sServer.on("/api/dome",   HTTP_POST, handleApiDome);
    sServer.on("/api/gadget-cmd", HTTP_POST, handleApiGadgetCmd);
    sServer.on("/api/serial",     HTTP_POST, handleApiSerial);
    sServer.on("/api/hcr",        HTTP_POST, handleApiHCR);
    sServer.on("/api/config", HTTP_GET,  handleApiConfigGet);
    sServer.on("/api/config", HTTP_POST, handleApiConfigPost);
    sServer.on("/api/pins",   HTTP_GET,  handleApiPins);
    sServer.on("/diagnostics", HTTP_GET, handleDiagnostics);

    sServer.onNotFound([]() { sServer.send(404, "text/plain", "Not found"); });

    sServer.begin();
    Serial.println(F("[WiFi] HTTP server started"));
}

// Drain one serial port into the monitor.  Accumulates printable bytes into a
// line buffer and flushes on newline, when the buffer is full, or after 100 ms
// of silence.  Non-printable bytes (other than \r/\n) are silently skipped.
struct SerialMonPort {
    HardwareSerial* port;
    const char      label[5]; // "S0: ", "S1: ", "S2: "
    char            buf[MON_LINE_LEN];
    uint8_t         pos;
    uint32_t        lastMs;
};

static void monDrainSerial(SerialMonPort& p) {
    bool flushed = false;
    while (p.port->available()) {
        uint8_t b = (uint8_t)p.port->read();
        p.lastMs = millis();
        if (b == '\r' || b == '\n') {
            if (p.pos > 0) { p.buf[p.pos] = '\0'; monAppend(p.buf, 'r'); p.pos = 0; flushed = true; }
        } else if (b >= 0x20 && b < 0x7F) {
            if (p.pos < MON_LINE_LEN - 1) p.buf[p.pos++] = (char)b;
        }
        // non-printable bytes silently skipped
        if (p.pos >= MON_LINE_LEN - 1) { p.buf[p.pos] = '\0'; monAppend(p.buf, 'r'); p.pos = 0; flushed = true; }
    }
    // Flush on 100 ms silence
    if (!flushed && p.pos > 0 && (millis() - p.lastMs) > 100) {
        p.buf[p.pos] = '\0'; monAppend(p.buf, 'r'); p.pos = 0;
    }
}

void AmidalaWiFiAP::handle() {
    sServer.handleClient();

    // Serial RX monitoring.  ROBOCLAW_SERIAL is defined to Serial1 when the
    // RoboClaw dome drive is active — skip Serial1 in that case (binary protocol).
    static SerialMonPort sPortWCB = { &Serial0, "S0: ", {}, 0, 0 };
#ifndef ROBOCLAW_SERIAL
    static SerialMonPort sPortS1  = { &Serial1, "S1: ", {}, 0, 0 };
#endif
    static SerialMonPort sPortAux = { &Serial2, "S2: ", {}, 0, 0 };
    auto seedLabel = [](SerialMonPort& p) {
        if (p.pos == 0) {
            uint8_t len = (uint8_t)strlen(p.label);
            memcpy(p.buf, p.label, len);
            p.buf[len] = '\0';
            p.pos = len;
        }
    };
    seedLabel(sPortWCB);
    monDrainSerial(sPortWCB);
#ifndef ROBOCLAW_SERIAL
    seedLabel(sPortS1);
    monDrainSerial(sPortS1);
#endif
    if (sCtrl && sCtrl->params.auxserial3) {
        seedLabel(sPortAux);
        monDrainSerial(sPortAux);
    }
}

#else  // UNIT_TEST

void AmidalaWiFiAP::begin(const char*, const char*, AmidalaController*) {}
void AmidalaWiFiAP::handle() {}

#endif
