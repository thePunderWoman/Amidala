// button_dispatch.h
// Shared, remote-agnostic button-press classification and dispatch, used by
// XBeePocketRemote (include/xbee_remote.h), BTGamepad (src/bt_gamepad.cpp),
// and SnipsRemote (include/snips_remote.h). Previously each of these three
// hand-rolled its own copy of the same long-press timer / alt-modifier
// routing logic (confirmed byte-for-byte identical between XBee and BT) --
// this collapses all three onto one tested implementation.
//
// Depends only on plain integer types -- no Arduino/Reeltwo dependency, so
// it's unit-testable natively.

#pragma once

#include <stdint.h>

// Neither XBee's nor BT's existing long-press timer is backed by a live
// params/UI setting today (both were independently hardcoded to 3000ms) --
// this is a straight de-duplication of that constant, not a new runtime
// setting. Overrideable before include, same idiom as xbee_remote.h's
// LONG_PRESS_TIME (which now just forwards to this).
#ifndef DEFAULT_LONG_PRESS_MS
#define DEFAULT_LONG_PRESS_MS 3000
#endif

// Per-button long-press timer. Verbatim port of xbee_remote.h's
// CHECK_BUTTON_LONGPRESS macro body / bt_gamepad.cpp's _checkLongPress body
// (the two were already textually identical).
//
// update() returns a Result by value rather than taking `up` as a bool&
// out-param -- some callers' "up" storage (Reeltwo's Event bitfields, see
// xbee_remote.h) can't bind to a non-const reference, so the (possibly
// long-press-suppressed) result has to come back as a plain value instead.
class ButtonLongPress {
public:
  struct Result {
    bool up;      // `up` as given, or false if a long-press already fired
                  // during this hold (suppresses the trailing normal press).
    bool longUp;  // true exactly once per hold, on the tick the hold crosses
                  // longPressMs while still held.
  };

  // Call once per tick with this button's current down/up/held state.
  //   down: true exactly on the tick the button transitions to pressed.
  //   up:   true exactly on the tick the button transitions to released.
  //   held: true while the button is currently pressed.
  //   now:  current time in ms (caller's millis()).
  //   longPressMs: threshold to fire a long-press while still held.
  Result update(bool down, bool up, bool held, uint32_t now, uint32_t longPressMs) {
    Result r{up, false};
    if (down) {
      fPressTime = now;
      fLongPress = false;
    } else if (up) {
      fPressTime = 0;
      if (fLongPress) r.up = false;
      fLongPress = false;
    } else if (fPressTime != 0 && held) {
      if (fPressTime + longPressMs < now) {
        fPressTime = 0;
        fLongPress = true;
        r.longUp = true;
      }
    }
    return r;
  }

private:
  uint32_t fPressTime = 0;
  bool fLongPress = false;
};

// Alt-modifier-aware press dispatch. Verbatim port of drive_controllers.cpp's
// DISPATCH_BUTTON+DISPATCH_LONG macros / bt_gamepad.cpp's BT_DISPATCH macro.
// `Driver` just needs noteButtonUp(unsigned)/processAltButton(unsigned)/
// processLongButton(unsigned) -- AmidalaController (include/controller.h)
// satisfies this directly.
template <typename Driver>
inline void dispatchButtonPress(Driver &driver, unsigned num, bool up, bool longUp,
                                 int altbtn, bool altHeld) {
  if (up && altbtn != (int)num) {
    altHeld ? driver.processAltButton(num) : driver.noteButtonUp(num);
  }
  if (longUp && altbtn != (int)num && !altHeld) {
    driver.processLongButton(num);
  }
}
