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
#include "step_value.h"
#include "ReelTwo.h"
#include "debug_monitor_tee.h"  // must follow ReelTwo.h -- see its own header comment
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
#include "hcr_link_transport.h"
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

#ifndef VMUSIC_SERIAL
// Firmware side of hcr_link.h: HCR lines go to the mesh via WCBClientController,
// wired bytes to UART0 (the same port HCRVocalizer itself writes to).
class HcrLinkSinkImpl : public HcrLinkSink {
public:
  // txLog: the controller's fSerialTxLog slot (set later by the web UI), taken
  // by reference so HCR TX lands in the monitor exactly like serial strings do.
  HcrLinkSinkImpl(WCBClientController &wcb, void (*&txLog)(const char *, bool))
      : fWCB(wcb), fTxLog(txLog) {}
  bool sendMesh(const char *line) override { return fWCB.broadcastHcr(line); }
  bool sendMeshToHost(const char *line) override { return fWCB.sendHcrToHost(line); }
  void writeSerial(const char *bytes) override {
    SERIAL.write((const uint8_t *)bytes, strlen(bytes));
  }
  void logTx(const char *text, bool viaMesh) override {
    if (fTxLog) fTxLog(text, viaMesh);
  }

private:
  WCBClientController &fWCB;
  void (*&fTxLog)(const char *, bool);
};
#endif

#include "button_actions.h"
#include "audio.h"
#include "config.h"
#include "console.h"
#include "jevois_console.h"
#include "rdh_serial.h"
#include "xbee_remote.h"
#include "motor_lock.h"
#include "snips_downlink.h"
#include "snips_remote.h"
#include "xbee_at_session.h"
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

  // ButtonAction::kVolumeStep handler (issue #204 phase 2) -- target 0 =
  // plain (routes via params.volumewheel), 1 = alt (params.altvolumewheel,
  // falling through to params.volumewheel when altvolumewheel==0 -- must
  // match AmidalaAudio::setAltVolumeNoResponse()'s own fallthrough exactly,
  // or this reads back the wrong channel's current value before stepping
  // it). "Alt" isn't a second value space -- see
  // AmidalaAudio::getEffectiveVolume() -- so this just reads back whichever
  // channel that wheel selector currently points at, steps it, and writes
  // it back through the same routing.
  inline void stepVolume(uint8_t dir, uint8_t target) {
    uint8_t wheel = (target && params.altvolumewheel != 0) ? params.altvolumewheel
                                                            : params.volumewheel;
    uint8_t current = fAudio.getEffectiveVolume(wheel);
    uint8_t next = stepValue(current, params.snipsVolumeStep, dir != 0);
    if (target) setAltVolumeNoResponse(next);
    else setVolumeNoResponse(next);
  }

  // Applies params.driveSpeedPct (percentage of MAXIMUM_SPEED) to
  // fTankDrive -- the single formula shared by setup() (boot-time), this
  // class's stepDriveSpeed() (live button adjustment), and
  // AmidalaConfig::cfg_drivespeedpct() (live config-key set), so a future
  // change to the formula (clamping, a non-linear curve, etc.) can't land
  // on only one of the three call sites.
  //
  // fDriveAccelScale (0..1) is a Snips drive-accelerator binding's share of
  // that cap (see snips_bindings.h); 1 with none.
  inline void applyDriveSpeedPct() {
#ifdef DRIVE_SYSTEM
    if (fTankDrive)
      fTankDrive->setMaxSpeed(MAXIMUM_SPEED * (params.driveSpeedPct / 100.0f) * fDriveAccelScale);
#endif
  }

  // ButtonAction::kThrottleStep handler (issue #204 phase 2). Drive-only --
  // there is no dome throttle. params.driveSpeedPct is the persisted,
  // live-adjustable cap (percentage of MAXIMUM_SPEED); fTankDrive's actual
  // fSpeedModifier (see Reeltwo's TankDrive::setMaxSpeed()) has no live
  // getter of its own, so params.driveSpeedPct is the single source of
  // truth for both applying and echoing this value.
  inline void stepDriveSpeed(uint8_t dir) {
    params.driveSpeedPct = stepValue(params.driveSpeedPct, params.snipsThrottleStep, dir != 0);
    applyDriveSpeedPct();
  }

  inline void toggleMute() { fAudio.toggleMute(); }

  // E-stop: the web UI's E-STOP, the OTA upload's automatic one, or a
  // ButtonAction::kEstop button. Stops and disables the drive and dome,
  // broadcasts the estopstr= commands, and blocks re-enabling (see
  // motor_lock.h) until resume(). The lag-based safety stop and
  // controller-disconnect stops also call emergencyStop() but recover on
  // their own, so they don't count. Reported to Snips controllers' status
  // screens.
  void estop() {
    fMotorLock.setEstop(true);
    emergencyStop();
    domeEmergencyStop();
    for (uint8_t i = 0; i < params.estopCmdCount; i++)
      sendSerialString(params.EstopCmds[i].str);
  }

  // Clears the e-stop, broadcasts the resumestr= commands, and re-enables
  // the drive and dome unless motors are also switched off.
  void resume() {
    fMotorLock.setEstop(false);
    enableController();
    enableDomeController();
    for (uint8_t i = 0; i < params.resumeCmdCount; i++)
      sendSerialString(params.ResumeCmds[i].str);
  }

  void toggleEstop() {
    if (isEstopActive()) resume();
    else estop();
  }

  bool isEstopActive() const { return fMotorLock.estopActive(); }

  // "Motors on/off" (ButtonAction::kMotorsToggle): stops and disables the
  // drive and dome without an e-stop broadcast, and keeps them off until
  // switched back on. Reported to Snips controllers as motors disabled.
  void setMotorsDisabled(bool disabled) {
    fMotorLock.setMotorsDisabled(disabled);
    if (disabled) {
      emergencyStop();
      domeEmergencyStop();
    } else {
      enableController();
      enableDomeController();
    }
  }

  void toggleMotors() { setMotorsDisabled(!areMotorsDisabled()); }

  bool areMotorsDisabled() const { return fMotorLock.motorsDisabled(); }

  // Snips analog bindings (snips_bindings.h): applies what one uplink from
  // `side` asks for, and lets go of `side`'s bindings when it disconnects
  // (a bound accelerator or dome speed drops to 0). Defined in
  // src/snips_bindings_apply.cpp.
  void applySnipsBindings(uint8_t side, const Snips::UplinkPacket &pkt);
  void snipsSideDisconnected(uint8_t side);
  // Back to the unscaled speed cap and dome speed, e.g. after switching
  // away from Snips controllers.
  void clearSnipsBindingScales();

  // Whether the dome is wandering on its own. Only the RoboClaw dome drive
  // tracks this; the RDH auto dome serial link just toggles it blind, so
  // it reads false there.
  bool isAutoDomeOn() const {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    return fDomeDrive && fDomeDrive->isRandomMode();
#else
    return false;
#endif
  }

  // The last serial string a button or gesture sent, shown on Snips
  // controllers as the running sequence (see snips_downlink.h) -- unless
  // it's marked as a utility command (utility=, the Serial Commands page's
  // "Utility" checkbox).
  SnipsDownlink::SequenceTracker fSequences;

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
  // Snips Controllers (issue #204) -- standalone siblings, not part of
  // remote[] above. See include/snips_remote.h for why.
  SnipsRemote fSnipsRight;
  SnipsRemote fSnipsLeft;
  // XBee PAN ID / coordinator-role read+write over local AT commands
  // (issue #213) -- one shared session regardless of controllertype, same
  // as fDomeStick's web-capture flags above are one shared session for
  // gestures. Registered with xbee_spi.cpp via xbeeSPISetATSession() in
  // setup() so the 0x88 response frame pump can reach it; ticked once per
  // animate() cycle for its own timeout check.
  XBeeATSession fXBeeAT;
  // Heap-allocated from PSRAM, not a plain member -- Str[MAX_SERIAL_STRINGS]
  // alone is ~67KB, and as static internal-SRAM storage that was ~35% of
  // this firmware's total RAM budget (issue #172). See allocParamsInPSRAM()
  // in controller.cpp. Safe as a reference: declared here, before
  // fAutoDome/fPPMDecoder below, so member construction order (which follows
  // declaration order, not initializer-list order) binds it before anything
  // later in the constructor's initializer list reads from it.
  AmidalaParameters &params;
#ifndef VMUSIC_SERIAL
  // After params (it holds a reference to it) and fWCB (the sink's mesh leg).
  // Installed on fHCR by AmidalaAudio::init().
  HcrLinkSinkImpl  fHcrLinkSink{fWCB, fSerialTxLog};
  HcrLinkTransport fHcrLink{params, fHcrLinkSink};
#endif
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

  // No-op while an e-stop or motors-off is in effect (see motor_lock.h), so
  // a reconnecting remote or a recovered safety stop can't bring the drive
  // back behind it.
  void enableController() {
    if (!fMotorLock.allowsEnable()) return;
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

  // Gated the same way as enableController().
  void enableDomeController() {
    if (!fMotorLock.allowsEnable()) return;
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
    fConfig.setLoadingConfigFile(true);
    bool loaded = readConfig(fVMusic, fConsole);
#else
    fConfig.setLoadingConfigFile(true);
    bool loaded = readConfig(fConsole);
#endif
    fConfig.setLoadingConfigFile(false);
    return loaded;
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
  MotorLock fMotorLock;
  // Last failsafe state reported for each Snips controller (right, left);
  // starts true, like XBeePocketRemote::failsafeNotice.
  bool fSnipsFailsafeNotice[2] = {true, true};
  void reportRemoteFailsafe(unsigned index, bool failsafe, bool *notice, bool newlineFirst);
  SnipsBindings::Evaluator fBindingEval;
  // Per side (SnipsRemote::Side): a bound accelerator/dome speed's latest
  // value, or -1 with no such binding on that side.
  float fSideDriveScale[2] = {-1, -1};
  float fSideDomeScale[2] = {-1, -1};
  float fDriveAccelScale = 1.0f;
  float fDomeAccelScale = 1.0f;
  // Dome heading a rotation-rate binding steers toward, when a binding last
  // steered the dome, and the last goToAngle() sent (it's rate-limited).
  bool fBindingHeadingValid = false;
  float fBindingHeading = 0;
  uint32_t fBindingHeadingLastMs = 0;
  int fBindingHeadingSent = -1;
  uint32_t fBindingHeadingSentMs = 0;
  void applyBindingScales();
  void steerDomeTo(float degrees, uint32_t now);
  bool fDblPressActive[MAX_BUTTONS] = {};
  uint32_t fDblPressTime[MAX_BUTTONS] = {};
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
