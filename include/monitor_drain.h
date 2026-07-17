// monitor_drain.h
// Byte-level line-buffering state machine used to tap a serial port's RX
// stream into the monitor ring buffer (monitor_buf.h). Extracted from
// wifi_ap.cpp so it can be unit-tested without a real HardwareSerial.
//
// A port's label (e.g. "S0: ") is pre-seeded into the line buffer so a
// flushed line reads "S0: hello" without a separate concat step. That
// seeding must NOT by itself count as "pending data" -- see
// monDrainHasContent() below.

#pragma once

#include <stdint.h>
#include <string.h>
#include "monitor_buf.h"

struct MonDrainState {
    char        buf[MON_LINE_LEN];
    uint8_t     pos;
    uint32_t    lastMs;
    const char* label;
    uint8_t     labelLen;
    char        cls; // MonLine.cls to flush with -- 'r' for RX serial ports
                      // (default), 'i' for sources that aren't a tx/rx wire
                      // exchange (e.g. the console log tee) so they aren't
                      // gated by the RX toggle in web/monitor.html.
};

inline void monDrainInit(MonDrainState& s, const char* label, char cls = 'r') {
    s.pos      = 0;
    s.lastMs   = 0;
    s.label    = label;
    s.labelLen = (uint8_t)strlen(label);
    s.cls      = cls;
}

// Re-seeds the label prefix once the buffer has been flushed/reset (pos==0).
inline void monDrainSeedLabel(MonDrainState& s) {
    if (s.pos == 0) {
        memcpy(s.buf, s.label, s.labelLen);
        s.buf[s.labelLen] = '\0';
        s.pos = s.labelLen;
    }
}

// True once real bytes have been appended beyond the seeded label -- a
// buffer holding only the label has no content worth flushing.
inline bool monDrainHasContent(const MonDrainState& s) {
    return s.pos > s.labelLen;
}

static void monDrainFlush(MonDrainState& s) {
    s.buf[s.pos] = '\0';
    monAppend(s.buf, s.cls);
    s.pos = 0;
}

// Feeds one received byte in. Returns true if it caused a line flush
// (newline or line-buffer-full), so the caller can skip the silence check
// for this tick.
inline bool monDrainByte(MonDrainState& s, uint8_t b, uint32_t nowMs) {
    s.lastMs = nowMs;
    bool flushed = false;
    if (b == '\r' || b == '\n') {
        if (monDrainHasContent(s)) { monDrainFlush(s); flushed = true; }
    } else if (b >= 0x20 && b < 0x7F) {
        if (s.pos < MON_LINE_LEN - 1) s.buf[s.pos++] = (char)b;
    }
    // non-printable bytes silently skipped
    if (s.pos >= MON_LINE_LEN - 1) { monDrainFlush(s); flushed = true; }
    return flushed;
}

// Flushes a partial line after 100 ms of silence. Only fires when real
// content is buffered -- an idle port that's had nothing but its own label
// re-seeded into it must never flush (that was the bug: a label-only buffer
// with a stale/zero lastMs looked "pending" and flushed on every tick).
inline void monDrainSilenceCheck(MonDrainState& s, uint32_t nowMs) {
    if (monDrainHasContent(s) && (nowMs - s.lastMs) > 100) {
        monDrainFlush(s);
    }
}
