#include "debug.h"
#include "controller.h"

#ifdef USE_VOLUME_WHEEL_DEBUG
#define VOLUME_WHEEL_DEBUG_PRINT(raw, vol) \
    do { \
        DEBUG_PRINT("volwheel raw="); DEBUG_PRINT(raw); \
        DEBUG_PRINT(" vol="); DEBUG_PRINTLN(vol); \
    } while (0)
#define VOLUME_WHEEL_DEBUG_SKIP(raw) \
    do { DEBUG_PRINT("volwheel raw="); DEBUG_PRINT(raw); DEBUG_PRINTLN(" (skipped)"); } while (0)
#else
#define VOLUME_WHEEL_DEBUG_PRINT(raw, vol) do {} while (0)
#define VOLUME_WHEEL_DEBUG_SKIP(raw)       do {} while (0)
#endif

// ---------------------------------------------------------------------------
// Button numbering (1-based, matches B[]/LB[]/AB[] indices):
//   Drive stick: triangle=1, circle=2, cross=3, square=4, l3=5
//   Dome  stick: triangle=6, circle=7, cross=8, square=9
//   (Dome l3 is reserved for gesture input and cannot be configured as
//   the alt or mute button.)
//
// DISPATCH_BUTTON: on button_up, route to alt layer or normal layer.
//   Suppresses the button entirely if it is the configured alt modifier.
// DISPATCH_LONG: on long_button_up, only fires when alt is not held.
//   Suppresses long-press for the alt button itself.
//
// DRIVE_BTNFIELD / DOME_BTNFIELD: extract a boolean from a Ps3ButtonData
//   struct by button number, avoiding repetitive switch blocks.
// ---------------------------------------------------------------------------

#define DISPATCH_BUTTON(btnfield, num, altbtn, altHeld)                         \
  if (event.button_up.btnfield && (altbtn) != (num)) {                         \
    (altHeld) ? fDriver->processAltButton(num) : fDriver->noteButtonUp(num);   \
  }

#define DISPATCH_LONG(btnfield, num, altbtn, altHeld)                          \
  if (event.long_button_up.btnfield && (altbtn) != (num) && !(altHeld))       \
    fDriver->processLongButton(num);

// Map drive-stick button number (1–5) to a field in a Ps3ButtonData struct.
#define DRIVE_BTNFIELD(num, btns) \
  ((num)==1?(btns).triangle:(num)==2?(btns).circle:(num)==3?(btns).cross:(num)==4?(btns).square:(btns).l3)

// Map dome-stick button number (6–9) to a field in a Ps3ButtonData struct.
#define DOME_BTNFIELD(num, btns) \
  ((num)==6?(btns).triangle:(num)==7?(btns).circle:(num)==8?(btns).cross:(btns).square)

// ---------------------------------------------------------------------------
// DriveController
// ---------------------------------------------------------------------------

void DriveController::notify() {
  uint32_t lagTime = millis() - lastPacket;
  if (event.analog_changed.button.l2) {
    fDriver->setDriveThrottle(float(state.analog.button.l2) / 255.0);
  }
  if (lagTime > 5000) {
    DEBUG_PRINTLN("More than 5 seconds. Disconnect");
    fDriver->emergencyStop();
    fSafetyStop.trip();
    disconnect();
  } else if (lagTime > 500) {
    DEBUG_PRINTLN("It has been 500ms. Shutdown motors");
    fDriver->emergencyStop();
    fSafetyStop.trip();
  } else {
    // Packets are flowing normally again -- if the lag-based safety stop
    // above had tripped, undo it. Without this, a drop just under the
    // failsafe timeout (params.fst) never reaches onConnect() (the only
    // other path that re-enables the drive), so the drive stayed disabled
    // until the remote was fully power-cycled even though input kept working.
    if (fSafetyStop.recover()) {
      DEBUG_PRINTLN("Signal recovered. Re-enabling drive");
      fDriver->enableController();
    }

    // ---- Alt-button state (drive buttons 1–5) --------------------------------
    int altbtn = fDriver->params.altbtn;
    if (altbtn >= 1 && altbtn <= 5)
      fDriver->setAltHeld(DRIVE_BTNFIELD(altbtn, state.button));
    bool altHeld = fDriver->isAltHeld();

    // ---- Button dispatch -----------------------------------------------------
    DISPATCH_BUTTON(triangle, 1, altbtn, altHeld)
    DISPATCH_BUTTON(circle,   2, altbtn, altHeld)
    DISPATCH_BUTTON(cross,    3, altbtn, altHeld)
    DISPATCH_BUTTON(square,   4, altbtn, altHeld)
    DISPATCH_BUTTON(l3,       5, altbtn, altHeld)

    DISPATCH_LONG(triangle, 1, altbtn, altHeld)
    DISPATCH_LONG(circle,   2, altbtn, altHeld)
    DISPATCH_LONG(cross,    3, altbtn, altHeld)
    DISPATCH_LONG(square,   4, altbtn, altHeld)
    DISPATCH_LONG(l3,       5, altbtn, altHeld)

    // ---- Double-press mute detection (buttons 1–5) ---------------------------
    int muteBtn = fDriver->params.mutebutton;
    if (muteBtn >= 1 && muteBtn <= 5 && DRIVE_BTNFIELD(muteBtn, event.button_up))
      fDriver->noteMuteBtnUp();
  }
}

void DriveController::onConnect() {
  DEBUG_PRINTLN("Drive Stick Connected");
  fDriver->enableController();
}

void DriveController::onDisconnect() {
  DEBUG_PRINTLN("Drive Stick Disconnected");
  // Clear alt state if the alt button is on this controller.
  int altbtn = fDriver->params.altbtn;
  if (altbtn >= 1 && altbtn <= 5)
    fDriver->setAltHeld(false);
  int muteBtn = fDriver->params.mutebutton;
  if (muteBtn >= 1 && muteBtn <= 5)
    fDriver->resetMutePressTimer();
  fDriver->disableController();
}

// ---------------------------------------------------------------------------
// DomeController
// ---------------------------------------------------------------------------

void DomeController::notify() {
  uint32_t lagTime = millis() - lastPacket;
  if (lagTime > 5000) {
    DEBUG_PRINTLN("More than 5 seconds. Disconnect");
    fDriver->domeEmergencyStop();
    fSafetyStop.trip();
    disconnect();
  } else if (lagTime > 500) {
    DEBUG_PRINTLN("It has been 500ms. Shutdown motors");
    fDriver->domeEmergencyStop();
    fSafetyStop.trip();
  } else {
    // Packets are flowing normally again -- if the lag-based safety stop
    // above had tripped, undo it. Without this, a drop just under the
    // failsafe timeout (params.fst) never reaches onConnect() (the only
    // other path that re-enables the dome), so it stayed disabled until the
    // remote was fully power-cycled even though input kept working.
    // recover() always consumes the trip so a stale flag doesn't leak into
    // whatever happens after a gesture, but skip the actual re-enable while
    // a gesture is being collected -- that's an intentional disable (see
    // process() below) that manages its own enable/disable independent of
    // connection state.
    if (fSafetyStop.recover() && !fGestureCollect) {
      DEBUG_PRINTLN("Signal recovered. Re-enabling dome");
      fDriver->enableDomeController();
    }
    process();
  }
}

void DomeController::process() {
  if (event.analog_changed.button.l1) {
    /* Volume — skip the release event (raw==0) to avoid zeroing volume */
    uint8_t raw = state.analog.button.l1;
    if (raw > 0) {
      uint8_t vol = map(raw, 0, 255, 0, 100);
      VOLUME_WHEEL_DEBUG_PRINT(raw, vol);
      if (fDriver->isAltHeld())
        fDriver->setAltVolumeNoResponse(vol);
      else
        fDriver->setVolumeNoResponse(vol);
    } else {
      VOLUME_WHEEL_DEBUG_SKIP(raw);
    }
  }
  if (event.analog_changed.button.l2) {
    /* Throttle */
    fDriver->setDomeThrottle(float(state.analog.button.l2) / 255.0);
  }

  // ---- Alt-button state (dome buttons 6–9) ----------------------------------
  int altbtn = fDriver->params.altbtn;
  if (altbtn >= 6 && altbtn <= 9)
    fDriver->setAltHeld(DOME_BTNFIELD(altbtn, state.button));

  // ---- Alt dome-stick mode --------------------------------------------------
  // altdomestick=1: while alt is held, engage abs-stick mode on the dome.
  // When alt is released the mode is restored (if we engaged it).
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fDriver->params.altbtn != 0 && fDriver->params.altdomestick == 1) {
    bool altHeld = fDriver->isAltHeld();
    if (altHeld && !fAltEngagedAbsStick
        && fDriver->fDomeDrive.isHomed()
        && fDriver->fDomeDrive.isCalibrated()
        && !fDriver->fDomeDrive.isAbsoluteStickMode()
        && !fDriver->fDomeDrive.isRandomMode()) {
      fDriver->fDomeDrive.enableAbsoluteStickMode();
      fAltEngagedAbsStick = true;
    } else if (!altHeld && fAltEngagedAbsStick) {
      fDriver->fDomeDrive.disableAbsoluteStickMode();
      fAltEngagedAbsStick = false;
    }
  }
#endif

  if (!fGestureCollect) {
    if (event.button_up.l3) {
      DEBUG_PRINTLN("GESTURE START COLLECTING");
      fDriver->disableDomeController();
      fGestureCollect = true;
      fGesturePtr = fGestureBuffer;
      fGestureTimeOut = millis() + GESTURE_TIMEOUT_MS;
    } else {
      bool altHeld = fDriver->isAltHeld();

      DISPATCH_BUTTON(triangle, 6, altbtn, altHeld)
      DISPATCH_BUTTON(circle,   7, altbtn, altHeld)
      DISPATCH_BUTTON(cross,    8, altbtn, altHeld)
      DISPATCH_BUTTON(square,   9, altbtn, altHeld)

      DISPATCH_LONG(triangle, 6, altbtn, altHeld)
      DISPATCH_LONG(circle,   7, altbtn, altHeld)
      DISPATCH_LONG(cross,    8, altbtn, altHeld)
      DISPATCH_LONG(square,   9, altbtn, altHeld)

      // ---- Double-press mute detection (buttons 6–9) -------------------------
      int muteBtn = fDriver->params.mutebutton;
      if (muteBtn >= 6 && muteBtn <= 9 && DOME_BTNFIELD(muteBtn, event.button_up))
        fDriver->noteMuteBtnUp();
    }
    return;
  } else if (fGestureTimeOut < millis()) {
    DEBUG_PRINTLN("GESTURE TIMEOUT");
    fDriver->enableDomeController();
    fGesturePtr = fGestureBuffer;
    fGestureCollect = false;
  } else {
    if (event.button_up.l3) {
      // delete trailing '5' from gesture
      unsigned glen = strlen(fGestureBuffer);
      if (glen > 0 && fGestureBuffer[glen - 1] == '5')
        fGestureBuffer[glen - 1] = 0;
      fDriver->enableDomeController();
      fGestureCollect = false;
      fDriver->processGesture(fGestureBuffer);
      return;
    }

    if (event.button_up.triangle)
      addGesture('A');
    if (event.button_up.circle)
      addGesture('B');
    if (event.button_up.cross)
      addGesture('C');
    if (event.button_up.square)
      addGesture('D');
    if (!fGestureAxis) {
      if (abs(state.analog.stick.lx) > 50 &&
          abs(state.analog.stick.ly) > 50) {
        // Diagonal
        if (state.analog.stick.lx < 0)
          fGestureAxis = (state.analog.stick.ly < 0) ? '1' : '7';
        else
          fGestureAxis = (state.analog.stick.ly < 0) ? '3' : '9';
        addGesture(fGestureAxis);
      } else if (abs(state.analog.stick.lx) > 100) {
        // Horizontal
        fGestureAxis = (state.analog.stick.lx < 0) ? '4' : '6';
        addGesture(fGestureAxis);
      } else if (abs(state.analog.stick.ly) > 100) {
        // Vertical
        fGestureAxis = (state.analog.stick.ly < 0) ? '2' : '8';
        addGesture(fGestureAxis);
      }
    }
    if (fGestureAxis && abs(state.analog.stick.lx) < 10 &&
        abs(state.analog.stick.ly) < 10) {
      addGesture('5');
      fGestureAxis = 0;
    }
  }
}

void DomeController::onConnect() {
  DEBUG_PRINTLN("Dome Stick Connected");
  fDriver->enableDomeController();
}

void DomeController::onDisconnect() {
  DEBUG_PRINTLN("Dome Stick Disconnected");
  // Clear alt state if the alt button is on this controller.
  int altbtn = fDriver->params.altbtn;
  if (altbtn >= 6 && altbtn <= 9)
    fDriver->setAltHeld(false);
  int muteBtn = fDriver->params.mutebutton;
  if (muteBtn >= 6 && muteBtn <= 9)
    fDriver->resetMutePressTimer();
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fAltEngagedAbsStick) {
    fDriver->fDomeDrive.disableAbsoluteStickMode();
    fAltEngagedAbsStick = false;
  }
#endif
  fDriver->disableDomeController();
}
