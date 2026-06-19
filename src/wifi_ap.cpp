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

static bool updateConfigFile(const char* key, const char* value) {
    String path   = "/config.txt";
    String prefix = String(key) + "=";
    String newLine = prefix + String(value);

    File f = SD.open(path, "r");
    if (!f) {
        // No config.txt yet — create a minimal one
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
        // Trim trailing \r so the comparison works on Windows-style line endings
        if (line.endsWith("\r")) line.remove(line.length() - 1);

        // Replace a matching non-commented key= line
        if (!line.startsWith("#") && !line.startsWith("//") && line.startsWith(prefix)) {
            out += newLine + "\n";
            found = true;
        } else {
            out += line + "\n";
        }
    }
    f.close();

    if (!found) {
        // Insert before #END if present, otherwise append
        int endIdx = out.lastIndexOf("#END");
        if (endIdx >= 0) {
            out = out.substring(0, endIdx) + newLine + "\n" + out.substring(endIdx);
        } else {
            out += newLine + "\n";
        }
    }

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
static void handleMonitor()             { sServer.send(200, "text/html", WEB_PAGE_MONITOR);         }
static void handleUpdatePage()          { sServer.send(200, "text/html", WEB_PAGE_UPDATE);          }
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
    // Remaining config pages (stubs)
    sServer.on("/config/buttons",        HTTP_GET, handleComingSoon);
    sServer.on("/config/servos",         HTTP_GET, handleConfigServos);
    sServer.on("/config/serial-strings", HTTP_GET, handleConfigSerialStrings);
    sServer.on("/sequences",            HTTP_GET, handleComingSoon);
    sServer.on("/monitor",              HTTP_GET,  handleMonitor);
    sServer.on("/api/monitor",          HTTP_GET,  handleApiMonitorGet);
    sServer.on("/api/monitor",          HTTP_POST, handleApiMonitorPost);
    sServer.on("/update",               HTTP_GET,  handleUpdatePage);
    sServer.on("/update",               HTTP_POST, handleUpdatePost, handleUpdateUpload);

    // REST API
    sServer.on("/api/info",   HTTP_GET,  handleApiInfo);
    sServer.on("/api/estop",  HTTP_POST, handleApiEstop);
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
