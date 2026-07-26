#pragma once

///////////////////////////////////////////////////////////////////////////////
// AmidalaController class declaration.
//
// The constructor, setup(), animate(), emergencyStop(), and domeEmergencyStop()
// are declared here and defined in src/controller.cpp — they reference
// servoDispatch, panservo, and tiltservo, which are globals defined in
// src/globals.cpp (accessed via extern declarations in controller.cpp).
///////////////////////////////////////////////////////////////////////////////

#include "drive_config.h"
#include "ReelTwo.h"
// Needed unconditionally: globals.cpp/controller.cpp/servo.cpp reference the
// ServoDispatch base class for the physical PWM servo outputs regardless of
// DRIVE_SYSTEM. TankDrivePWM.h and TankDriveRoboteq.h happen to pull this in
// transitively, but TankDriveSabertooth.h (packet-serial, no PWM) does not —
// include it directly so DRIVE_SYSTEM_SABER builds don't lose it.
#include "ServoDispatch.h"
#if DRIVE_SYSTEM == DRIVE_SYSTEM_PWM
#include "drive/TankDrivePWM.h"
#endif
#if DRIVE_SYSTEM >= DRIVE_SYSTEM_ROBOTEQ_PWM &&                                \
    DRIVE_SYSTEM <= DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
#include "drive/TankDriveRoboteq.h"
#endif
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER
#include "drive/TankDriveSabertooth.h"
#endif
#if DOME_DRIVE == DOME_DRIVE_PWM
#include "drive/DomeDrivePWM.h"
#elif DOME_DRIVE == DOME_DRIVE_SABER
#include "drive/DomeDriveSabertooth.h"
#elif DOME_DRIVE == DOME_DRIVE_ROBOCLAW
#include "dome_drive_roboclaw.h"
#endif
#include "core/MedianSampleBuffer.h"
#include "core/DelayCall.h"
#include <Wire.h>
#ifndef VMUSIC_SERIAL
#include <hcr.h>
#endif
#include "core.h"
#include "version.h"
#include "pin_config.h"
#include <EEPROM.h>
#include "ppm_decoder.h"
#include "i2c_utils.h"
#include "wcb_client_controller.h"

// Forward-declare AmidalaController before the headers that use it as a pointer.
class AmidalaController;

#include "button_actions.h"
#include "audio.h"
#include "config.h"
#include "console.h"
#include "jevois_console.h"
#include "rdh_serial.h"
#include "xbee_remote.h"
#include "params.h"
#include "wifi_ap.h"

#include "serial_output.h"

class AmidalaController : public SetupEvent, public AnimatedEvent {
public:
  // Defined in src/controller.cpp — initialiser list references
  // servoDispatch (extern declared in that translation unit).
  AmidalaController();

  inline void processGesture(const char *gesture) {
    fConsole.processGesture(gesture);
  }

  inline void processButton(unsigned num) { fConsole.processButton(num); }

  inline void processLongButton(unsigned num) {
    fConsole.processLongButton(num);
  }

  inline void processAltButton(unsigned num) { fConsole.processAltButton(num); }

  inline void processDoubleButton(unsigned num) { fConsole.processDoubleButton(num); }

  // Called on button_up for normal (non-alt, non-long) presses.
  // If a double-press action is configured for this button, starts/completes
  // detection; otherwise fires the single-press immediately with no delay.
  inline void noteButtonUp(unsigned num) {
    if (num < 1 || num > params.getButtonCount()) return;
    unsigned idx = num - 1;
    uint16_t timeout = params.dbtimeout;
    if (params.DB[idx].action == ButtonAction::kNone || timeout == 0) {
      fConsole.processButton(num);
      return;
    }
    uint32_t now = millis();
    if (fDblPressActive[idx] && now - fDblPressTime[idx] <= timeout) {
      fDblPressActive[idx] = false;
      fConsole.processDoubleButton(num);
    } else {
      fDblPressActive[idx] = true;
      fDblPressTime[idx] = now;
    }
  }

  // Called every animate() frame to fire pending single-presses after timeout.
  inline void checkDoublePressPending() {
    uint16_t timeout = params.dbtimeout;
    if (timeout == 0) return;
    uint32_t now = millis();
    for (unsigned i = 0; i < params.getButtonCount(); i++) {
      if (fDblPressActive[i] && now - fDblPressTime[i] > timeout) {
        fDblPressActive[i] = false;
        fConsole.processButton(i + 1);
      }
    }
  }

  inline bool isAltHeld() const { return fAltHeld; }
  inline void setAltHeld(bool held) { fAltHeld = held; }

  inline void setVolumeNoResponse(unsigned volume) {
    fAudio.setVolumeNoResponse(volume);
  }
  inline void setAltVolumeNoResponse(unsigned volume) {
    fAudio.setAltVolumeNoResponse(volume);
  }

  inline void toggleMute() { fAudio.toggleMute(); }

  // Maximum gap between two button-up events to count as a double-press.
  static const uint32_t DOUBLE_PRESS_MS = 400;

  // Call on every button-up event for the configured mutebutton.
  // Fires toggleMute() and resets the timer when a double-press is detected.
  inline void noteMuteBtnUp() {
    uint32_t now = millis();
    if (fLastMuteBtnUpTime != 0 &&
        now - fLastMuteBtnUpTime <= DOUBLE_PRESS_MS) {
      fLastMuteBtnUpTime = 0;
      toggleMute();
    } else {
      fLastMuteBtnUpTime = now;
    }
  }

  // Reset the double-press timer (call on controller disconnect).
  inline void resetMutePressTimer() { fLastMuteBtnUpTime = 0; }

  AmidalaConsole fConsole;
  AmidalaAudio fAudio;
  AmidalaConfig fConfig;
  AmidalaWiFiAP fWiFiAP;
  WCBClientController fWCB;
#ifdef VMUSIC_SERIAL
  VMusic fVMusic;
#else
  HCRVocalizer fHCR;
#endif
#ifdef EXPERIMENTAL_JEVOIS_STEERING
  JevoisConsole fJevois;
#endif
  DriveController fDriveStick;
  DomeController fDomeStick;
  XBeePocketRemote *remote[2] = {&fDriveStick, &fDomeStick};
  AmidalaParameters params;
#ifdef RDH_SERIAL
  RDHSerial fAutoDome;
#endif

// fTankDrive/fDomeDrive are constructed lazily in AmidalaController::setup()
// (issue #147), not here in the class body's own default member initializer,
// because their constructors need a HardwareSerial& resolved from params —
// and params isn't loaded from flash until setup() runs. AmidalaController
// itself is a global (see AmidalaFirmware.ino), so plain value members here
// would be bound to a fixed serial port at C++ static-init time, before any
// config value exists to choose one. Nothing dereferences these between
// global construction and setup()'s "new" calls -- see setup()'s comment at
// the construction site for the invariant this relies on.
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER
  TankDriveSabertooth* fTankDrive = nullptr;
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_PWM
  TankDrivePWM* fTankDrive = nullptr;
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM
  TankDriveRoboteq* fTankDrive = nullptr;
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL
  TankDriveRoboteq* fTankDrive = nullptr;
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
  TankDriveRoboteq* fTankDrive = nullptr;
#elif defined(DRIVE_SYSTEM)
#error Unsupported DRIVE_SYSTEM
#endif

#if DOME_DRIVE == DOME_DRIVE_SABER
  DomeDriveSabertooth* fDomeDrive = nullptr;
#elif DOME_DRIVE == DOME_DRIVE_PWM
  DomeDrivePWM* fDomeDrive = nullptr;
#elif DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  DomeDriveRoboClaw* fDomeDrive = nullptr;
#endif

  bool checkRCMode() {
#ifdef RCSEL_PIN
    static bool sRCMode;
    if (digitalRead(RCSEL_PIN) == LOW) {
      if (!sRCMode) {
        fConsole.println("RC Enabled (" +
                         String(params.getRadioChannelCount()) + " Channels)");
#ifdef STATUS_RC_PIN
        digitalWrite(STATUS_RC_PIN, HIGH);
#endif
        sRCMode = true;
      }
      return true;
    }
    if (sRCMode) {
#ifdef STATUS_RC_PIN
      digitalWrite(STATUS_RC_PIN, LOW);
#endif
      sRCMode = false;
    }
    return false;
#else
    return false;
#endif
  }

  bool checkSel2Mode() {
#ifdef SEL2_PIN
    static bool sSel2Mode;
    if (digitalRead(SEL2_PIN) == LOW) {
      if (!sSel2Mode) {
        sSel2Mode = true;
      }
      return true;
    }
    if (sSel2Mode) {
      sSel2Mode = false;
    }
    return false;
#else
    return false;
#endif
  }

  unsigned getDomeMode() {
#ifdef RDH_SERIAL
    return fAutoDome.getMode();
#else
    return 0;
#endif
  }

  unsigned getDomeHome() {
#ifdef RDH_SERIAL
    return fAutoDome.getHome();
#else
    return 0;
#endif
  }

  unsigned getDomePosition() {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    return fDomeDrive->getCurrentDegrees();
#elif defined(RDH_SERIAL)
    return fAutoDome.getAngle();
#else
    return 0;
#endif
  }

  /**
   * Process a "dome=<cmd>" console command for the RoboClaw dome drive.
   * Commands: home, calibrate, stop, front, rand, status, <N>, +<N>, -<N>
   * Defined in src/controller.cpp.
   */
  void processDomeCommand(const char* cmd);

  /**
   * Execute a dome command from a button or gesture action.
   * subcmd is a ButtonAction::DomeCmdType value; arg is the angle or delta
   * in degrees (only used for kDomeGotoAbs, kDomeRelPos, kDomeRelNeg).
   * Defined in src/controller.cpp.
   */
  void processDomeCmd(uint8_t subcmd, uint8_t arg);

  bool getDomeIMU() { return params.domeimu; }

  void setDomeHome(unsigned pos) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    // No-op: the RoboClaw drive derives home from the hall sensor trigger.
    (void)pos;
#elif defined(RDH_SERIAL)
    fAutoDome.setDomeHomePosition(pos);
#else
    (void)pos;
#endif
  }

  unsigned getVolume() { return params.volume; }

  void setTargetSteering(TargetSteering *steering) {
#ifdef DRIVE_SYSTEM
    fTankDrive->setTargetSteering(steering);
#endif
  }

  void enableController() {
#ifdef DRIVE_SYSTEM
    fTankDrive->setEnable(true);
#endif
  }

  void disableController() {
    emergencyStop();
#ifdef DRIVE_SYSTEM
    fTankDrive->setEnable(false);
#endif
  }

  // Defined in src/controller.cpp — references servoDispatch global.
  void emergencyStop();

  void enableDomeController() {
#ifdef DOME_DRIVE
    fDomeDrive->setEnable(true);
#endif
  }

  void disableDomeController() {
#ifdef DOME_DRIVE
    domeEmergencyStop();
    fDomeDrive->setEnable(false);
#endif
  }

  // Defined in src/controller.cpp — references servoDispatch global.
  void domeEmergencyStop();

  void setDigitalPin(int pin, bool state) {
    // pin is 1-based, indexing however many of the 11 pool pins currently
    // have role Dout (issue #133) -- 0-11, not a fixed 4. Out-of-range pins
    // are silently ignored (mirrors the old fixed-table's "entries beyond
    // the wired count are 0" behavior).
    uint8_t doutCount = countPinsWithRole(params.pinRole, PinRoleType::kDout);
    if (pin >= 1 && (uint8_t)pin <= doutCount) {
      params.D[pin - 1].state = state;
      uint8_t gpio = nthPinWithRole(params.pinRole, PinRoleType::kDout, pin - 1);
      digitalWrite(gpio, state ? HIGH : LOW);
    }
  }

  bool getDigitalPin(int pin) {
    uint8_t doutCount = countPinsWithRole(params.pinRole, PinRoleType::kDout);
    if (pin >= 1 && (uint8_t)pin <= doutCount) {
      return params.D[pin - 1].state;
    }
    return false;
  }

  bool loadConfig() {
#ifdef VMUSIC_SERIAL
    fConsole.println(F("Waiting for VMusic"));
    if (!fVMusic.init()) {
      fConsole.println(F("VMusic unavailable"));
      return false;
    }
    fConsole.println(F("Reading Config File"));
    return readConfig(fVMusic, fConsole);
#else
    return readConfig(fConsole);
#endif
  }

  // Second arg: true if the command actually went out over the mesh, false
  // if it went out UART0 (either because that's the selected destination,
  // or because WCB was selected but the fail-safe fallback kicked in) — the
  // monitor tap uses this to tag the line "MESH: " vs "S0: " correctly.
  void (*fSerialTxLog)(const char*, bool) = nullptr;

  void writeEol() { writeEolTo(SERIAL, params.serialeol); }

  void sendSerialString(const char *str) {
    bool wentToMesh = fWCB.routeOutbound(str, params.outboundserial == 1, params.serialdelim);
    if (!wentToMesh)
      sendSerialStringTo(SERIAL, str, params.serialdelim, params.serialeol);
    if (fSerialTxLog) fSerialTxLog(str, wentToMesh);
  }

  // Defined in src/controller.cpp — references servoDispatch, panservo,
  // and tiltservo (extern declared in that translation unit).
  virtual void setup() override;

  // Defined in src/controller.cpp — references servoDispatch global.
  virtual void animate() override;

private:
  // DriveController and DomeController call setDriveThrottle/setDomeThrottle,
  // which are intentionally private to all other callers.
  void executeDomeAction(uint8_t subcmd, int arg);
  friend class DriveController;
  friend class DomeController;

  PPMDecoder fPPMDecoder;
  bool fMinimal = true;
  bool fAltHeld = false;
  uint32_t fLastMuteBtnUpTime = 0;
  bool fDblPressActive[9] = {};
  uint32_t fDblPressTime[9] = {};
  uint32_t fDriveStateMillis = 0;
  uint32_t fDomeStateMillis = 0;
  // Internal drive/dome "recently active" indicators used by animate()'s
  // timing logic. Previously piggybacked on setDigitalPin(7)/(8) against
  // two always-unwired DOUT slots (DRIVE_ACTIVE/DOME_ACTIVE, see
  // pin_config.h) -- now that DOUT-typed pin COUNT is dynamic (issue #133,
  // 0-11 rather than a fixed 4), there's no fixed "slot 7/8" to piggyback
  // on without risking collision with a real user-configured DOUT pin, so
  // these get their own dedicated state instead.
  bool fDriveActiveIndicator = false;
  bool fDomeActiveIndicator = false;
  float fDomeThrottle = 0;
  float fDriveThrottle = 0;

  inline float getDomeThrottle() { return fDomeThrottle; }

  inline void setDomeThrottle(float throttle) { fDomeThrottle = throttle; }

  inline float getDriveThrottle() { return fDriveThrottle; }

  inline void setDriveThrottle(float throttle) { fDriveThrottle = throttle; }

  void setDomeHomePosition() {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    fDomeDrive->startHoming();
#elif defined(RDH_SERIAL)
    fAutoDome.setDomeHomePosition();
#endif
  }

  void toggleRandomDome() {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    if (fDomeDrive->isHomed())
        fDomeDrive->enableRandomMode();
    else
        fDomeDrive->disableAutoMode();
#elif defined(RDH_SERIAL)
    fAutoDome.toggleRandomDome();
#endif
  }

};
