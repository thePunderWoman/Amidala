// monitor_buf.h
// Circular ring buffer for the serial monitor log.
// Extracted so it can be unit-tested without pulling in WiFi/WebServer.
//
// Usage: define MONITOR_BUF_OWNER in exactly one translation unit before
// including this header to get the storage definitions; all other TUs that
// need read access get the extern declarations.

#pragma once

#include <stdint.h>
#include <string.h>

#define MON_LINES    256
#define MON_LINE_LEN 96

struct MonLine {
    char text[MON_LINE_LEN];
    char cls; // 't'=tx  'r'=rx  'i'=info
};

#ifdef MONITOR_BUF_OWNER
// sMonBuf is ~24.8KB (MON_LINES * sizeof(MonLine)) -- as a static array it
// would sit in internal SRAM for the life of the process. Heap-allocate it
// from PSRAM instead on real hardware (issue #172); native unit tests
// (UNIT_TEST, see platformio.ini's env:native) have no PSRAM/ESP-IDF heap
// caps, so they keep a plain static array.
#ifndef UNIT_TEST
#include <esp_heap_caps.h>
inline MonLine* allocMonBufPSRAM() {
    MonLine* buf = static_cast<MonLine*>(
        heap_caps_malloc(sizeof(MonLine) * MON_LINES, MALLOC_CAP_SPIRAM));
    memset(buf, 0, sizeof(MonLine) * MON_LINES);
    return buf;
}
MonLine*  sMonBuf   = allocMonBufPSRAM();
#else
static MonLine sMonBufStorage[MON_LINES];
MonLine*  sMonBuf   = sMonBufStorage;
#endif
uint16_t  sMonHead  = 0;
uint16_t  sMonCount = 0;
uint32_t  sMonSeq   = 0;
#else
extern MonLine*  sMonBuf;
extern uint16_t  sMonHead;
extern uint16_t  sMonCount;
extern uint32_t  sMonSeq;
#endif

inline void monAppend(const char* text, char cls = 'i') {
    strncpy(sMonBuf[sMonHead].text, text, MON_LINE_LEN - 1);
    sMonBuf[sMonHead].text[MON_LINE_LEN - 1] = '\0';
    sMonBuf[sMonHead].cls = cls;
    sMonHead = (sMonHead + 1) % MON_LINES;
    if (sMonCount < MON_LINES) sMonCount++;
    sMonSeq++;
}

// Appends `text` to `out` as a JSON string body (the part between the
// quotes), escaping everything ECMA-404 requires. monAppend() itself never
// filters its input -- most callers only ever pass printable ASCII (the
// serial-port drain in monitor_drain.h filters to 0x20-0x7E before it ever
// reaches monAppend()), but callers that log other sources verbatim (e.g.
// WCBClientController::poll() logging a received mesh command straight
// through) can end up storing a raw control byte. Escaping only '"' and
// '\\' -- which is all the API endpoint used to do -- left literal control
// characters embedded in the JSON response; net effect was that any single
// bad line permanently broke every future response.json() parse in the
// browser (JSON.parse rejects unescaped control characters in a string),
// which read as the serial monitor going "disconnected" forever even
// though the device and its actual traffic were fine.
template <typename StringT>
inline void monJsonAppendEscaped(StringT& out, const char* text) {
    static const char* const kHex = "0123456789abcdef";
    for (const char* p = text; *p; p++) {
        uint8_t c = (uint8_t)*p;
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c < 0x20) {
            char esc[7] = "\\u0000";
            esc[4] = kHex[(c >> 4) & 0xF];
            esc[5] = kHex[c & 0xF];
            out += esc;
        } else {
            out += (char)c;
        }
    }
}
