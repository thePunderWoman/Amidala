// debug_monitor_tee.h
// Tees ReelTwo's DEBUG_PRINT/DEBUG_PRINTLN/DEBUG_PRINTF/DEBUG_PRINT_HEX/
// DEBUG_PRINTLN_HEX output (see debug.h's USE_DEBUG etc.) into the web
// Monitor page's ring buffer, in addition to the raw serial port it's
// always gone to -- the same "LOG: " tee pattern console.cpp already uses
// for AmidalaConsole::write() (see teeConsoleToMonitor()), applied to debug
// output so it's visible without a laptop physically plugged in. Prompted
// by issue #185: a field debug capture from Robert required him to have a
// serial monitor open locally: with USE_DEBUG on, this makes the same
// output show up in the browser instead (or in addition).
//
// Must be #include'd AFTER ReelTwo.h in any translation unit that wants
// its DEBUG_PRINT et al calls tee'd -- it needs DEBUG_SERIAL/DEBUG_PRINT
// already defined so it can #undef and replace them. A complete no-op
// (does nothing, defines nothing) unless USE_DEBUG is set, since ReelTwo.h
// itself leaves DEBUG_PRINT et al as `while (0)` in that case -- nothing to
// tee. Never included by anything the native test env compiles (DEBUG_SERIAL
// is never defined there), so there's nothing to unit-test here directly.
#pragma once

#include "monitor_buf.h"
#include "monitor_drain.h"

#ifdef DEBUG_SERIAL

// Print-derived so it picks up every DEBUG_SERIAL.print()/.println()
// overload (int, float, String, F()-flash strings, HEX/DEC/OCT/BIN bases,
// ...) for free -- Arduino's Print base class formats all of those down to
// write() calls, which is the only thing this class actually needs to
// override to duplicate output to both destinations.
class DebugMonitorTeeWriter : public Print {
public:
  size_t write(uint8_t b) override {
    Serial.write(b);
    monDrainByte(drainState(), b, millis());
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t size) override {
    Serial.write(buffer, size);
    for (size_t i = 0; i < size; i++) monDrainByte(drainState(), buffer[i], millis());
    return size;
  }

private:
  static MonDrainState& drainState() {
    static MonDrainState s;
    static bool ready = false;
    if (!ready) {
      monDrainInit(s, "DBG: ", 'i');
      ready = true;
    }
    monDrainSeedLabel(s);
    return s;
  }
};

inline DebugMonitorTeeWriter& debugMonitorTeeWriter() {
  static DebugMonitorTeeWriter w;
  return w;
}

#undef DEBUG_PRINT
#undef DEBUG_PRINTLN
#undef DEBUG_PRINTF
#undef DEBUG_PRINT_HEX
#undef DEBUG_PRINTLN_HEX
#undef DEBUG_FLUSH

#define DEBUG_PRINT(s)       debugMonitorTeeWriter().print(s)
#define DEBUG_PRINTLN(s)     debugMonitorTeeWriter().println(s)
#define DEBUG_PRINTF(...)    debugMonitorTeeWriter().printf(__VA_ARGS__)
#define DEBUG_PRINT_HEX(s)   debugMonitorTeeWriter().print(s, HEX)
#define DEBUG_PRINTLN_HEX(s) debugMonitorTeeWriter().println(s, HEX)
#define DEBUG_FLUSH()        Serial.flush()

#endif  // DEBUG_SERIAL
