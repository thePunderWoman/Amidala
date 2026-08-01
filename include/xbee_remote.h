// xbee_remote.h
// XBee pocket remote controller classes for Amidala Firmware.
//
// XBeePocketRemote — base class that wraps raw XBee/RC/failsafe input into a
//                    JoystickController-compatible state + event model with
//                    long-press detection.
// DriveController  — XBeePocketRemote subclass wired to AmidalaController's
//                    drive-side methods (throttle, buttons, connect/disconnect).
// DomeController   — XBeePocketRemote subclass wired to AmidalaController's
//                    dome-side methods (volume, dome throttle, gesture input).
//
// DriveController and DomeController method bodies that call AmidalaController
// are defined in src/drive_controllers.cpp to avoid a circular header
// dependency.
//
// Depends on: JoystickController.h (Reeltwo), core.h,
//             millis() (Arduino / arduino_mock.h),
//             map() (Arduino / arduino_mock.h)

#pragma once

#include "JoystickController.h"
#include "core.h"
#include "safety_stop_latch.h"

// ---- Timing constants (overrideable before including this header) -----------

#ifndef GESTURE_TIMEOUT_MS
#define GESTURE_TIMEOUT_MS 2000
#endif

#ifndef LONG_PRESS_TIME
#define LONG_PRESS_TIME 3000
#endif

// ---- Forward declaration ----------------------------------------------------

class AmidalaController;

// ---- XBeePocketRemote -------------------------------------------------------

class XBeePocketRemote : public JoystickController {
public:
  XBeePocketRemote() {
    addr = 0;
    y = 512;   // center; map(512,0,1024,127,-128) == 0 (neutral stick)
    x = 512;
    w1 = 0;
    w2 = 0;
    memset(button, 0, sizeof(button));
    type = kFailsafe;
    lastPacket = 0;
    memset(&state, '\0', sizeof(state));
    memset(&event, '\0', sizeof(event));
    memset(&longpress, '\0', sizeof(longpress));
    fConnecting = true;
    fConnected = false;
    failsafeNotice = true;
  }

  uint32_t addr;
  uint16_t y;
  uint16_t x;
  uint16_t w1;
  uint16_t w2;
  bool button[5];
  enum Type { kFailsafe, kXBee, kRC };
  struct LongPress {
    uint32_t pressTime;
    bool longPress;
  };
  struct {
    LongPress l3;
    LongPress triangle;
    LongPress circle;
    LongPress cross;
    LongPress square;
  } longpress;
  Type type;
  bool failsafeNotice;
  uint32_t lastPacket;

  bool failsafe() { return (type == XBeePocketRemote::kFailsafe); }

  void update() {
    Event evt = {};
    State prev = state;

    state.analog.stick.lx = map(x, 0, 1024, 127, -128);
    state.analog.stick.ly = map(y, 0, 1024, 127, -128);
    state.analog.stick.rx = state.analog.stick.lx;
    state.analog.stick.ry = state.analog.stick.ly;
    state.analog.button.l1 = map(w1, 0, 1024, 255, 0);
    state.analog.button.l2 = map(w2, 0, 1024, 255, 0);
    state.analog.button.r1 = state.analog.button.l1;
    state.analog.button.r2 = state.analog.button.l2;
    state.button.triangle = button[0];
    state.button.circle = button[1];
    state.button.cross = button[2];
    state.button.square = button[3];
    state.button.l3 = button[4];

#define CHECK_BUTTON_DOWN(b)                                                   \
  evt.button_down.b = (!prev.button.b && state.button.b)
    CHECK_BUTTON_DOWN(l3);
    CHECK_BUTTON_DOWN(triangle);
    CHECK_BUTTON_DOWN(circle);
    CHECK_BUTTON_DOWN(cross);
    CHECK_BUTTON_DOWN(square);
#define CHECK_BUTTON_UP(b) evt.button_up.b = (prev.button.b && !state.button.b)
    CHECK_BUTTON_UP(l3);
    CHECK_BUTTON_UP(triangle);
    CHECK_BUTTON_UP(circle);
    CHECK_BUTTON_UP(cross);
    CHECK_BUTTON_UP(square);
#define CHECK_BUTTON_LONGPRESS(b)                                              \
  {                                                                            \
    evt.long_button_up.b = false;                                              \
    if (evt.button_down.b) {                                                   \
      longpress.b.pressTime = millis();                                        \
      longpress.b.longPress = false;                                           \
    } else if (evt.button_up.b) {                                              \
      longpress.b.pressTime = 0;                                               \
      if (longpress.b.longPress)                                               \
        evt.button_up.b = false;                                               \
      longpress.b.longPress = false;                                           \
    } else if (longpress.b.pressTime != 0 && state.button.b) {                 \
      if (longpress.b.pressTime + LONG_PRESS_TIME < millis()) {                \
        longpress.b.pressTime = 0;                                             \
        longpress.b.longPress = true;                                          \
        evt.long_button_up.b = true;                                           \
      }                                                                        \
    }                                                                          \
  }
    CHECK_BUTTON_LONGPRESS(l3);
    CHECK_BUTTON_LONGPRESS(triangle);
    CHECK_BUTTON_LONGPRESS(circle);
    CHECK_BUTTON_LONGPRESS(cross);
    CHECK_BUTTON_LONGPRESS(square);

    /* Analog events */
    evt.analog_changed.stick.lx =
        state.analog.stick.lx - prev.analog.stick.lx;
    evt.analog_changed.stick.ly =
        state.analog.stick.ly - prev.analog.stick.ly;
    evt.analog_changed.button.l1 =
        state.analog.button.l1 - prev.analog.button.l1;
    evt.analog_changed.button.l2 =
        state.analog.button.l2 - prev.analog.button.l2;
    evt.analog_changed.button.r1 =
        state.analog.button.r1 - prev.analog.button.r1;
    evt.analog_changed.button.r2 =
        state.analog.button.r2 - prev.analog.button.r2;
    if (fConnecting) {
      fConnecting = false;
      fConnected = true;
      onConnect();
    }
    if (fConnected) {
      event = evt;
      notify();
      if (failsafe()) {
        fConnected = false;
        fConnecting = true;
        onDisconnect();
      }
    }
  }
};

// ---- DriveController --------------------------------------------------------
// Method bodies (notify, onConnect, onDisconnect) are defined after
// AmidalaController in src/drive_controllers.cpp.

class DriveController : public XBeePocketRemote {
public:
  DriveController(AmidalaController *driver) : fDriver(driver) {}

  virtual void notify() override;
  virtual void onConnect() override;
  virtual void onDisconnect() override;

  AmidalaController *fDriver;

protected:
  // See safety_stop_latch.h.
  SafetyStopLatch fSafetyStop;
};

// ---- DomeController ---------------------------------------------------------
// Method bodies (notify, process, onConnect, onDisconnect) are defined after
// AmidalaController in src/drive_controllers.cpp.

class DomeController : public XBeePocketRemote {
public:
  DomeController(AmidalaController *driver) : fDriver(driver) {}

  virtual void notify() override;
  void process();
  virtual void onConnect() override;
  virtual void onDisconnect() override;

  AmidalaController *fDriver;

protected:
  // See safety_stop_latch.h. Not force-re-enabled while fGestureCollect is
  // true; gesture start/end already manages enable/disable of the dome
  // controller for that window (see DomeController::notify()).
  SafetyStopLatch fSafetyStop;
  bool fGestureCollect = false;
  bool fAltEngagedAbsStick = false; ///< true if alt hold engaged abs-stick mode
  char fGestureBuffer[MAX_GESTURE_LENGTH + 1] = {};
  char *fGesturePtr = fGestureBuffer;
  char fGestureAxis = 0;
  uint32_t fGestureTimeOut = 0;

  void addGesture(char ch) {
    if (size_t(fGesturePtr - fGestureBuffer) < sizeof(fGestureBuffer) - 1) {
      *fGesturePtr++ = ch;
      *fGesturePtr = '\0';
      fGestureTimeOut = millis() + GESTURE_TIMEOUT_MS;
    }
  }

  // Resets collection state back to an empty gesture. Resetting fGesturePtr
  // alone is not enough: addGesture() null-terminates as it writes, but a
  // gesture with zero strokes (a plain L3 click with no stick movement, or a
  // timeout) never calls addGesture() at all, leaving the previous gesture's
  // text sitting in the buffer. Without clearing it here, that stale text
  // gets replayed as if it were the new gesture (issue #163).
  void resetGestureBuffer() {
    fGesturePtr = fGestureBuffer;
    fGestureBuffer[0] = '\0';
  }
};
