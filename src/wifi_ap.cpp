#include "wifi_ap.h"
#include "web_pages.h"

#ifndef UNIT_TEST
#include <EEPROM.h>     // must precede params.h (via web_api.h)
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "SD.h"
#include "web_api.h"
#include "controller.h"
#include "drive_config.h"

static WebServer          sServer(80);
static DNSServer          sDNS;
static AmidalaController* sCtrl = nullptr;

// ---------------------------------------------------------------------------
// Serial monitor log buffer
// ---------------------------------------------------------------------------

#define MON_LINES    32
#define MON_LINE_LEN 96

struct MonLine { char text[MON_LINE_LEN]; char cls; }; // cls: 't'=tx 'r'=rx 'i'=info

static MonLine   sMonBuf[MON_LINES];
static uint8_t   sMonHead  = 0;
static uint8_t   sMonCount = 0;
static uint32_t  sMonSeq   = 0;

static void monAppend(const char* text, char cls = 'i') {
    strncpy(sMonBuf[sMonHead].text, text, MON_LINE_LEN - 1);
    sMonBuf[sMonHead].text[MON_LINE_LEN - 1] = '\0';
    sMonBuf[sMonHead].cls = cls;
    sMonHead = (sMonHead + 1) % MON_LINES;
    if (sMonCount < MON_LINES) sMonCount++;
    sMonSeq++;
}

// ---------------------------------------------------------------------------
// SD card config write-back
// ---------------------------------------------------------------------------

#include "config_file.h"

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

    // Build new sstr= lines
    String sstrs;
    for (uint8_t i = 0; i < sCtrl->params.serialcount; i++) {
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
        buildFullConfigJson(sCtrl->params));
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
        if (idx < 0 || idx >= (int)sCtrl->params.serialcount) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        for (int j = idx; j < (int)sCtrl->params.serialcount - 1; j++)
            sCtrl->params.Str[j] = sCtrl->params.Str[j + 1];
        memset(&sCtrl->params.Str[sCtrl->params.serialcount - 1], 0, sizeof(SerialString));
        sCtrl->params.serialcount--;
        bool ok = rewriteSerialStrings();
        sServer.send(ok ? 200 : 207, "text/plain", ok ? "OK" : "deleted but SD write failed");
        return;
    }

    // sstr_N — update (or append when N == serialcount) serial string at index N
    if (key.startsWith("sstr_")) {
        int idx = key.substring(5).toInt();
        if (idx < 0 || idx > (int)sCtrl->params.serialcount ||
            idx >= (int)sCtrl->params.getSerialStringCount()) {
            sServer.send(400, "text/plain", "index out of range"); return;
        }
        setSerialStringFields(sCtrl->params.Str[idx], value.c_str());
        if (idx == (int)sCtrl->params.serialcount)
            sCtrl->params.serialcount++;
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
    monAppend("! EMERGENCY STOP", 't');
    if (sCtrl) sCtrl->sendSerialString("ESTOP");
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

static void handleApiMonitorGet() {
    String json = "{\"seq\":";
    json += String(sMonSeq);
    json += ",\"lines\":[";
    uint8_t start = (sMonCount < MON_LINES) ? 0 : sMonHead;
    for (uint8_t i = 0; i < sMonCount; i++) {
        if (i > 0) json += ",";
        uint8_t idx = (start + i) % MON_LINES;
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

static void handleUpdateUpload() {
    HTTPUpload& upload = sServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        monAppend("OTA: upload started", 'i');
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            monAppend("OTA: begin failed", 'i');
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            monAppend("OTA: write error", 'i');
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            monAppend("OTA: flash complete, restarting", 'i');
        } else {
            monAppend("OTA: end failed", 'i');
        }
    }
}

static void handleUpdatePost() {
    bool ok = !Update.hasError();
    sServer.send(200, "text/plain", ok ? "OK" : "FAIL");
    delay(200);
    if (ok) ESP.restart();
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
static void handleComingSoon()          { sServer.send(200, "text/html", WEB_PAGE_COMING_SOON);     }

// ---------------------------------------------------------------------------
// AmidalaWiFiAP
// ---------------------------------------------------------------------------

void AmidalaWiFiAP::begin(const char* ssid, const char* password, AmidalaController* ctrl) {
    sCtrl = ctrl;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    IPAddress ip = WiFi.softAPIP();
    Serial.print(F("[WiFi] AP \""));
    Serial.print(ssid);
    Serial.print(F("\" @ "));
    Serial.println(ip);

    // Wildcard DNS: all hostnames → AP IP (captive-portal style)
    sDNS.start(53, "*", ip);

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
    sServer.on("/droid-control",        HTTP_GET, handleDroidControl);
    sServer.on("/monitor",              HTTP_GET,  handleMonitor);
    sServer.on("/api/monitor",          HTTP_GET,  handleApiMonitorGet);
    sServer.on("/api/monitor",          HTTP_POST, handleApiMonitorPost);
    sServer.on("/update",               HTTP_GET,  handleUpdatePage);
    sServer.on("/update",               HTTP_POST, handleUpdatePost, handleUpdateUpload);

    // REST API
    sServer.on("/api/info",   HTTP_GET,  handleApiInfo);
    sServer.on("/api/estop",  HTTP_POST, handleApiEstop);
    sServer.on("/api/dome",   HTTP_POST, handleApiDome);
    sServer.on("/api/serial", HTTP_POST, handleApiSerial);
    sServer.on("/api/hcr",    HTTP_POST, handleApiHCR);
    sServer.on("/api/config", HTTP_GET,  handleApiConfigGet);
    sServer.on("/api/config", HTTP_POST, handleApiConfigPost);

    // Catch-all: any other URL redirects home (supports captive-portal flow)
    sServer.onNotFound(handleHome);

    sServer.begin();
    Serial.println(F("[WiFi] HTTP server started"));
}

void AmidalaWiFiAP::handle() {
    sDNS.processNextRequest();
    sServer.handleClient();
}

#else  // UNIT_TEST

void AmidalaWiFiAP::begin(const char*, const char*, AmidalaController*) {}
void AmidalaWiFiAP::handle() {}

#endif
