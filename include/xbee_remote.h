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

// How close to true center (per axis, 0-127 scale) counts as "recentered"
// for gesture capture -- closes out the current stroke so the next
// direction change can register. Real hardware doesn't reliably report
// true zero at rest: pot tolerance/calibration drift on a given remote can
// put its resting reading right at the edge of a naive small deadzone
// (measured as high as 10 on one axis in the field, consistently across
// many gesture attempts -- not sampling noise, a real per-remote floor), so
// this needs real margin above that. Still comfortably below the >50/>100
// thresholds that detect a genuine new stroke direction
// (drive_controllers.cpp), so there's no risk of this reading as
// "centered" while actually mid-deflection.
#ifndef GESTURE_CENTER_DEADZONE
#define GESTURE_CENTER_DEADZONE 20
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

  // Web-driven gesture capture (issue #138): reuses the same collection state
  // machine notify() drives from physical L3 presses, just diverting the
  // result to polling instead of live dispatch. Bodies are defined in
  // src/drive_controllers.cpp (need the complete AmidalaController type via
  // fDriver, same as notify()/process()).
  bool beginWebCapture();
  bool stopWebCapture();

  bool isWebCapturing() const { return fGestureCollect && fWebCapture; }
  bool isCaptureDone() const { return fCaptureDone; }
  const char *captureResult() const { return fGestureBuffer; }

  AmidalaController *fDriver;

protected:
  // See safety_stop_latch.h. Not force-re-enabled while fGestureCollect is
  // true; gesture start/end already manages enable/disable of the dome
  // controller for that window (see DomeController::notify()).
  SafetyStopLatch fSafetyStop;
  bool fGestureCollect = false;
  // true while the active capture was started via the web UI (POST
  // /api/gesture/capture/start) rather than a physical double-L3 press; see
  // beginWebCapture()/stopWebCapture() in src/drive_controllers.cpp.
  bool fWebCapture = false;
  // true once a web-initiated capture has finished (L3, the web "Done"
  // button, or the idle timeout) and fGestureBuffer holds the result for
  // polling. Empty fGestureBuffer with fCaptureDone true means "timed out".
  bool fCaptureDone = false;
  bool fAltEngagedAbsStick = false; ///< true if alt hold engaged abs-stick mode
  char fGestureBuffer[MAX_GESTURE_LENGTH + 1] = {};
  char *fGesturePtr = fGestureBuffer;
  char fGestureAxis = 0;
  uint32_t fGestureTimeOut = 0;
  // Diagnostic only (see process()): closest the stick has come to dead
  // center (both axes) since fGestureAxis was last set, i.e. while waiting
  // for a recenter to close out the current stroke. Lets a log line show
  // whether a centered sample was ever actually observed, rather than just
  // inferring it from the captured gesture text after the fact.
  int fGestureMinAbsLx = 999;
  int fGestureMinAbsLy = 999;

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
  //
  // fGestureAxis must be reset here too: it tracks "mid-stroke, waiting for
  // the stick to pass back through center before the next direction change
  // counts," and is normally cleared only when the stick recenters. If a
  // gesture ends (L3 released) while the stick is still deflected -- a very
  // natural way to finish a real gesture -- it's left stuck nonzero. Since
  // new direction detection in process() is gated on `!fGestureAxis`, the
  // *next* gesture attempt then can't register any strokes at all and always
  // submits empty, regardless of what was actually drawn. This bug predates
  // #163 but was masked by it: the stale gesture buffer #163 fixed used to
  // silently replay the old string instead of submitting empty, so it read
  // as "wrong gesture" rather than "no gesture ever registers again."
  void resetGestureState() {
    fGesturePtr = fGestureBuffer;
    fGestureBuffer[0] = '\0';
    fGestureAxis = 0;
    fGestureMinAbsLx = 999;
    fGestureMinAbsLy = 999;
  }

  // A capture naturally ends with the stick back at center -- that trailing
  // '5' was never a drawn stroke, just the recenter that closed out the last
  // real one, so strip it before the result is dispatched or handed back to
  // the web UI.
  void trimTrailingCenter() {
    unsigned glen = strlen(fGestureBuffer);
    if (glen > 0 && fGestureBuffer[glen - 1] == '5')
      fGestureBuffer[glen - 1] = '\0';
  }
};
