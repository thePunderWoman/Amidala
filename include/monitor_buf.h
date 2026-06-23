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
MonLine   sMonBuf[MON_LINES];
uint16_t  sMonHead  = 0;
uint16_t  sMonCount = 0;
uint32_t  sMonSeq   = 0;
#else
extern MonLine   sMonBuf[MON_LINES];
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
