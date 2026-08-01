#include "controller.h"
#include "dome_position_math.h"
#include "xbee_spi.h"
#include "bt_gamepad.h"
#include "config_file.h"

// Hardware globals defined in src/globals.cpp.
// ServoDispatch& avoids including ServoDispatchDirect.h here, which would
// pull in ServoDispatchPrivate.h and duplicate ISR definitions.
extern ServoDispatch& servoDispatch;

// ---------------------------------------------------------------------------
// Reassignable dome/drive serial ports (issue #147) -- see
// include/serial_assignment.h for the port-conflict validation this relies
// on. Only Serial1 and Serial2/AUX_SERIAL are ever reassignable; Serial0
// (WCB/body-controller out) is fixed and handled separately below.
// ---------------------------------------------------------------------------

static HardwareSerial& resolveSerialPort(SerialPortId id) {
  return (id == SerialPortId::kSerial2) ? AUX_SERIAL : Serial1;
}

// Explicit pins required for both ports: ESP32-S3 UART1/UART2 have no
// hardware-fixed default pins and won't route to the right GPIOs without
// them (see pin_config.h's SERIAL1_*_PIN/AUX_SERIAL_*_PIN).
static void beginSerialPort(SerialPortId id, uint32_t baud) {
  if (id == SerialPortId::kSerial2) {
    AUX_SERIAL.begin(baud, SERIAL_8N1, AUX_SERIAL_RX_PIN, AUX_SERIAL_TX_PIN);
  } else {
    Serial1.begin(baud, SERIAL_8N1, SERIAL1_RX_PIN, SERIAL1_TX_PIN);
  }
}

AmidalaController::AmidalaController()
    : fConsole(),
      fHCR(&SERIAL, HCR_BAUD_RATE),
#ifdef VMUSIC_SERIAL
      fVMusic(VMUSIC_SERIAL),
#endif
      fDriveStick(this), fDomeStick(this),
#ifdef RDH_SERIAL
      fAutoDome(RDH_SERIAL),
#endif
      fPPMDecoder(PPMIN_PIN, params.getRadioChannelCount()) {
  // fTankDrive/fDomeDrive are NOT constructed here (issue #147) -- see their
  // declaration in controller.h and the "new" calls in setup() below. This
  // constructor runs at C++ global static-init time (AmidalaController is a
  // global -- see AmidalaFirmware.ino), before params is loaded from flash,
  // so nothing here can depend on a config-driven serial port choice yet.
}

void AmidalaController::emergencyStop() {
#ifdef DRIVE_SYSTEM
  fTankDrive->stop();
  fTankDrive->setEnable(false);
  // Force neutral PWM signals as a safety backup
  servoDispatch.moveTo(1, 0.5); // Drive Left
  servoDispatch.moveTo(0, 0.5); // Drive Right
  // If using serial, the Roboteq script handles its own RWD 100 watchdog
#endif
}

void AmidalaController::domeEmergencyStop() {
#ifdef DOME_DRIVE
  fDomeDrive->stop();
  fDomeDrive->setEnable(false);
#if DOME_DRIVE != DOME_DRIVE_ROBOCLAW
  // Force neutral PWM signal as a safety backup (PWM/Sabertooth only).
  servoDispatch.moveTo(3, 0.5); // Dome
#endif
#endif
}

void AmidalaController::setup() {
  fConsole.println(F("Loading config from EEPROM"));
  params.init();

  fConsole.init(this);
  fConfig.init(this);

#ifdef EXPERIMENTAL_JEVOIS_STEERING
  fJevois.init(this);
#endif

  fConsole.println(F("Reading Config File"));
  bool configLoaded = loadConfig();
  fConsole.setMinimal(!configLoaded);
#ifndef VMUSIC_SERIAL
  // Self-heal config.txt: append defaults for any scalar setting that's
  // missing, so newly-added settings never end up in an undocumented,
  // invisible-to-the-user in-memory-only state.
  if (configLoaded)
    ensureConfigDefaults(params);
#endif
  // Guarantee the 11 reassignable pins (issue #133) are mutually consistent
  // -- config.txt lines parse in file order, so a line's own conflict check
  // only sees whatever's been parsed so far -- before anything below reads
  // them into a pinMode()/constructor call.
  fConfig.validatePinAssignments();
  // Guarantee domeSerialPort/driveSerialPort (issue #147) don't both claim
  // the same physical port before either is read below -- same "config.txt
  // parses in file order" reasoning as validatePinAssignments() above.
  fConfig.validateSerialPortAssignments();
  fConfig.showCurrentConfiguration();

  // fTankDrive/fDomeDrive construction is deferred to here (issue #147),
  // rather than living in AmidalaController's own constructor -- see
  // controller.h's member declarations. This is the earliest point after
  // config has loaded, and strictly before the first live use of either
  // (fTankDrive->setGuestStick() below). Each branch also opens its serial
  // port -- resolved from params.driveSerialPort/domeSerialPort -- on the
  // correct pins/baud before constructing. Previously (before this and the
  // reassignment feature both landed) this begin() happened in
  // AmidalaFirmware.ino's setup(), before config was even loaded, always on
  // AUX_SERIAL, and on the WRONG pins there (SERIAL0_RX_PIN/TX_PIN, i.e.
  // GPIO43/44, meant for Serial0/WCB-out) since neither
  // TankDriveRoboteq/TankDriveSabertooth/DomeDriveSabertooth ever begin()
  // their own serial port themselves.
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER
  beginSerialPort(params.driveSerialPort, DRIVE_BAUD_RATE);
  fTankDrive = new TankDriveSabertooth(128, resolveSerialPort(params.driveSerialPort), fDriveStick);
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_PWM
  fTankDrive = new TankDrivePWM(servoDispatch, 1, 0, 2, fDriveStick);
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM
  fTankDrive = new TankDriveRoboteq(servoDispatch, 1, 0, 2, fDriveStick);
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL
  beginSerialPort(params.driveSerialPort, DRIVE_BAUD_RATE);
  fTankDrive = new TankDriveRoboteq(resolveSerialPort(params.driveSerialPort), fDriveStick);
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
  beginSerialPort(params.driveSerialPort, DRIVE_BAUD_RATE);
  fTankDrive = new TankDriveRoboteq(resolveSerialPort(params.driveSerialPort), servoDispatch, 1, 0, 4, fDriveStick);
#endif

#if DOME_DRIVE == DOME_DRIVE_SABER
  beginSerialPort(params.domeSerialPort, DRIVE_BAUD_RATE);
  fDomeDrive = new DomeDriveSabertooth(129, resolveSerialPort(params.domeSerialPort), fDomeStick);
#elif DOME_DRIVE == DOME_DRIVE_PWM
  fDomeDrive = new DomeDrivePWM(servoDispatch, 3, fDomeStick);
#elif DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  // DomeDriveRoboClaw::setup() re-begin()s this same port later
  // (dome_drive_roboclaw.cpp) without pin args, which is fine -- the ESP32
  // Arduino core keeps a UART's already-attached pins across a re-begin()
  // that doesn't specify new ones.
  beginSerialPort(params.domeSerialPort, ROBOCLAW_BAUD_RATE);
  fDomeDrive = new DomeDriveRoboClaw(resolveSerialPort(params.domeSerialPort),
                                     DEFAULT_DOME_ROBOCLAW_ADDRESS,
                                     DEFAULT_DOME_ROBOCLAW_CHANNEL,
                                     DOME_HALL_PIN,
                                     fDomeStick);
#endif

  // WiFi AP must come up BEFORE WCB Client's begin() -- WCB_Client detects
  // an already-active SoftAP and switches to WIFI_AP_STA instead of forcing
  // WIFI_STA, so the mesh and the AP can coexist on the same radio channel.
  // Calling it in the other order only "also works" per the library's own
  // docs (Arduino's WiFi.mode() ORs in AP without dropping STA) -- AP-first
  // is its clearly-primary documented pattern, so that's what this follows.
  if (params.wifion)
    fWiFiAP.begin(params.wifiSSID, params.wifiPassword, this);

#ifndef VMUSIC_SERIAL
  fWCB.begin(params, fHCR, fConsole);
#endif

  // BT gamepad left stick drives when the primary XBee stick is absent.
  fTankDrive->setGuestStick(gBTGamepad);
  // BT gamepad right stick steers the dome when the XBee dome stick is absent.
  // setAltDomeStick() only exists on our DomeDriveRoboclaw class — Reeltwo's
  // DomeDrive base (used by DomeDrivePWM/DomeDriveSabertooth) has no
  // alt-stick concept, so this degrades gracefully to unavailable there.
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  fDomeDrive->setAltDomeStick(&gBTGamepad);
#endif
  if (params.btcontrolleron) {
    gBTGamepad.setup();
    gBTGamepad.setTargetAddr(params.btaddr);
  }

  fConsole.println(F("Activating Servos"));
  fConsole.println(F("Activating Digital Outputs"));
  fConsole.println(F("Init i2c Bus"));

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000L);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);
#endif

  if (params.autocorrect)
    fConsole.println(F("Auto Correct Gestures Enabled"));

#ifdef SERIAL
  SERIAL.begin(params.serialbaud, SERIAL_8N1, SERIAL0_RX_PIN, SERIAL0_TX_PIN);
  sendSerialString(params.serialinit);
  fAudio.init(this);
#endif
  if (params.auxserial3)
    AUX_SERIAL.begin(params.serialbaud, SERIAL_8N1, AUX_SERIAL_RX_PIN, AUX_SERIAL_TX_PIN);

  remote[0]->addr = params.xbr;
  remote[1]->addr = params.xbl;
  remote[0]->type = remote[0]->kFailsafe;
  remote[1]->type = remote[1]->kFailsafe;

#ifdef STATUS_J1_PIN
  pinMode(STATUS_J1_PIN, OUTPUT);
  pinMode(STATUS_J2_PIN, OUTPUT);
  pinMode(STATUS_RC_PIN, OUTPUT);
  pinMode(STATUS_S1_PIN, OUTPUT);
  pinMode(STATUS_S2_PIN, OUTPUT);
  pinMode(STATUS_S3_PIN, OUTPUT);
  pinMode(STATUS_S4_PIN, OUTPUT);
#endif

  // Pins are derived from params.pinRole (issue #133) -- validatePinAssignments()
  // above already guaranteed these are mutually consistent and within the
  // assignable pool. Counts per role type are however many pool pins
  // currently have that role, not a fixed number.
  uint8_t doutCount = countPinsWithRole(params.pinRole, PinRoleType::kDout);
  for (uint8_t i = 0; i < doutCount; i++) {
    pinMode(nthPinWithRole(params.pinRole, PinRoleType::kDout, i), OUTPUT);
  }

  uint8_t analogCount = countPinsWithRole(params.pinRole, PinRoleType::kAnalog);
  for (uint8_t i = 0; i < analogCount; i++) {
    pinMode(nthPinWithRole(params.pinRole, PinRoleType::kAnalog, i), INPUT);
  }

  uint8_t ppmPin = nthPinWithRole(params.pinRole, PinRoleType::kPpm, 0);
  if (ppmPin != kNoPin) {
    fPPMDecoder.setPin(ppmPin);
    pinMode(ppmPin, INPUT);
  }

#ifdef SEL2_PIN
  pinMode(SEL2_PIN, INPUT_PULLUP);
#endif
  for (uint8_t i = 1; i <= doutCount; i++) {
    setDigitalPin(i, false);
  }

  fTankDrive->setMaxSpeed(MAXIMUM_SPEED);
  fTankDrive->setThrottleAccelerationScale(ACCELERATION_SCALE);
  fTankDrive->setThrottleDecelerationScale(DECELRATION_SCALE);
  fTankDrive->setTurnAccelerationScale(ACCELERATION_SCALE * 2);
  fTankDrive->setTurnDecelerationScale(DECELRATION_SCALE);
  fTankDrive->setGuestSpeedModifier(MAXIMUM_GUEST_SPEED);
  fTankDrive->setScaling(SCALING);
  fTankDrive->setChannelMixing(CHANNEL_MIXING);

#ifdef DOME_DRIVE
  fDomeDrive->setMaxSpeed(DOME_MAXIMUM_SPEED);
  fDomeDrive->setInverted(params.domeflip);
  fDomeDrive->setScaling(false);
  fDomeDrive->setThrottleAccelerationScale(DEFAULT_DOME_ACCELERATION_SCALE);
  fDomeDrive->setThrottleDecelerationScale(DEFAULT_DOME_DECELERATION_SCALE);
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  // Apply runtime-configurable RoboClaw parameters from the loaded config.
  fDomeDrive->setAddress(params.domercaddr);
  fDomeDrive->setChannel(params.domercchan);
  fDomeDrive->setFrontOffset(params.domefront);
  fDomeDrive->setQPPS(params.domercqpps);
  fDomeDrive->setStallTimeout(params.domestall);
  fDomeDrive->setMaxSpeedPct(float(params.domespeed) / 100.0f);
  fDomeDrive->applyDomePositionParams(
      params.domeseekmin, params.domeseekmax,
      params.domeseekl,   params.domeseekr,
      params.domefudge,
      DEFAULT_DOME_SPEED_TARGET, params.domespeedmin);
  // Hall is a selectable role type (issue #133), defaulting to GPIO40 (see
  // pin_assignment.h) but reassignable if the sensor's been physically
  // rewired to a different pool pin. Must be called before setup() -- that's
  // where pinMode()/attachInterrupt() on the hall pin actually happen.
  // fConfig.validatePinAssignments() (called earlier in setup(), above)
  // already guarantees exactly one pin has role Hall whenever RoboClaw dome
  // drive is compiled in, so this is never kNoPin here.
  fDomeDrive->setHallPin(nthPinWithRole(params.pinRole, PinRoleType::kHall, 0));
  // setup() is called explicitly here (after config is applied) rather than
  // via the ReelTwo SetupEvent pool, so homing starts with correct parameters.
  fDomeDrive->setup();
#endif
#endif

  // Pins derived from params.pinRole (issue #133) -- servoSettings[] in
  // globals.cpp seeds compiled-in defaults for all 8 LEDC channels at
  // global-init time (before config loads); this re-applies the
  // config-derived pin for each live channel, and disables (pin=0) any
  // channel at or beyond the current live servo count, same double-init
  // pattern used throughout setup().
  uint8_t servoCount = params.getServoCount();
  for (uint8_t i = 0; i < kMaxServoChannels; i++) {
    if (i >= servoCount) {
      servoDispatch.setServo(i, 0, params.minpulse, params.maxpulse,
                             params.minpulse, 0);
      continue;
    }
    uint8_t pin = nthPinWithRole(params.pinRole, PinRoleType::kServo, i);
    uint16_t minpulse =
        params.S[i].minpulse ? params.S[i].minpulse : params.minpulse;
    uint16_t maxpulse =
        params.S[i].maxpulse ? params.S[i].maxpulse : params.maxpulse;

    float neutral_percent = float(params.S[i].n) / 180.0f;
    bool reversed = params.S[i].r;

    if (reversed) {
      uint16_t temp = maxpulse;
      maxpulse = minpulse;
      minpulse = temp;
    }

    uint16_t neutralpulse =
        (uint16_t)((int32_t)minpulse +
                   (int32_t)(neutral_percent *
                             ((int32_t)maxpulse - (int32_t)minpulse)));

    servoDispatch.setServo(i, pin, minpulse, maxpulse, neutralpulse, 0);
  }
}

void AmidalaController::animate() {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  fDomeDrive->animate();
#endif
  if (params.btcontrolleron) gBTGamepad.animate();
  fWCB.poll(params);
  if (params.wifion)
    fWiFiAP.handle();
  checkDoublePressPending();
  fConsole.process();
#ifdef EXPERIMENTAL_JEVOIS_STEERING
  fJevois.process();
#endif
#ifdef RDH_SERIAL
  fAutoDome.process();
#endif
  fAudio.process();

#if DRIVE_SYSTEM == DRIVE_SYSTEM_PWM || DRIVE_SYSTEM == DRIVE_SYSTEM_SABER
  if (servoDispatch.currentPos(0) != 1450 ||
      servoDispatch.currentPos(1) != 1500) {
    if (fDriveStateMillis + 1000 < millis()) {
      fDriveActiveIndicator = true;
      fDriveStateMillis = millis();
    }
  } else if (fDriveActiveIndicator && fDriveStateMillis + 1000 < millis()) {
    fDriveActiveIndicator = false;
    fDriveStateMillis = millis();
  }
#endif

#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fabsf(fDomeDrive->getLastCommandedSpeed()) > 0.01f) {
    if (fDomeStateMillis + 1000 < millis()) {
      fDomeActiveIndicator = true;
      fDomeStateMillis = millis();
    }
  } else if (fDomeActiveIndicator) {
    fDomeActiveIndicator = false;
    fDomeStateMillis = millis();
  }
#else
  if (fDomeDrive->isMoving()) {
    if (fDomeStateMillis + 1000 < millis()) {
      fDomeActiveIndicator = true;
      fDomeStateMillis = millis();
    }
  } else if (fDomeActiveIndicator) {
    fDomeActiveIndicator = false;
    fDomeStateMillis = millis();
  }
#endif

  if (checkRCMode() && remote[0]->failsafe() && remote[0]->failsafeNotice &&
      remote[1]->failsafe() && remote[1]->failsafeNotice) {
    // Both Pocket Remotes are disabled enable RC controller
    fPPMDecoder.init();
    remote[0]->type = remote[0]->kRC;
    remote[1]->type = remote[1]->kRC;
  }
  xbeeSPIReceiveAll(remote, sizeof(remote) / sizeof(remote[0]));

  if (checkRCMode() && remote[0]->type == remote[0]->kRC &&
      digitalRead(XBEE_ATTN_PIN) == HIGH) {
    if (fPPMDecoder.decode()) {
      remote[0]->x = fPPMDecoder.channel(0, 0, 1024, 512);
      remote[0]->y = fPPMDecoder.channel(1, 0, 1024, 512);
      remote[0]->w1 = fPPMDecoder.channel(4, 0, 1024, 0);
      remote[0]->update();

      remote[1]->x = fPPMDecoder.channel(2, 0, 1024, 512);
      remote[1]->y = fPPMDecoder.channel(3, 0, 1024, 512);
      remote[1]->w1 = fPPMDecoder.channel(5, 0, 1024, 0);
      remote[1]->update();
    }
  } else {
    bool stickActive = false;
    for (unsigned i = 0; i < sizeof(remote) / sizeof(remote[0]); i++) {
      auto r = remote[i];
      if (r->type == r->kXBee) {
        stickActive = true;
#ifdef USE_POCKET_REMOTE_DEBUG
        if (i == 0) {
          DEBUG_PRINT(F("J1 x="));
          DEBUG_PRINT(r->x);
          DEBUG_PRINT(F(" y="));
          DEBUG_PRINTLN(r->y);
        }
#endif
        if (r->lastPacket + params.fst < millis())
          r->type = r->kFailsafe;
        r->update();
      }
    }
    (void)stickActive;
    for (unsigned i = 0; i < sizeof(remote) / sizeof(remote[0]); i++) {
      auto r = remote[i];
      if (r->failsafe() != r->failsafeNotice) {
        if (stickActive)
          fConsole.println();
        fConsole.print('J');
        fConsole.print(i + 1);
        fConsole.print(F(" FS "));
        fConsole.println(r->failsafe() ? F("ON") : F("OFF"));
        if (i == 0) {
#ifdef STATUS_J1_PIN
          digitalWrite(STATUS_J1_PIN, r->failsafe() ? LOW : HIGH);
#endif
        } else if (i == 1) {
#ifdef STATUS_J2_PIN
          digitalWrite(STATUS_J2_PIN, r->failsafe() ? LOW : HIGH);
#endif
        }
        r->failsafeNotice = r->failsafe();
      }
    }
  }
}

// ---------------------------------------------------------------------------
// executeDomeAction()
// Shared dispatcher for both processDomeCommand() and processDomeCmd().
// subcmd must be a ButtonAction::DomeCmdType value.
// ---------------------------------------------------------------------------

void AmidalaController::executeDomeAction(uint8_t subcmd, int arg) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  switch (subcmd) {
  case ButtonAction::kDomeRand:
    fDomeDrive->toggleRandomMode();
    break;
  case ButtonAction::kDomeStop:
    fDomeDrive->stop();
    break;
  case ButtonAction::kDomeFront:
    fDomeDrive->goToAngle(0);
    break;
  case ButtonAction::kDomeHome:
    fDomeDrive->startHoming();
    break;
  case ButtonAction::kDomeCalibrate:
    fDomeDrive->startCalibration();
    break;
  case ButtonAction::kDomeGotoAbs:
    fDomeDrive->goToAngle(arg);
    break;
  case ButtonAction::kDomeRelPos:
    fDomeDrive->goToRelative(arg);
    break;
  case ButtonAction::kDomeRelNeg:
    fDomeDrive->goToRelative(-arg);
    break;
  case ButtonAction::kDomeAbsStick:
    fDomeDrive->toggleAbsoluteStickMode();
    break;
  default:
    break;
  }
#else
  (void)subcmd;
  (void)arg;
#endif
}

// ---------------------------------------------------------------------------
// processDomeCommand()
// Handles "dome=<cmd>" from the console. The <cmd> string is everything after
// the "dome=" prefix and the trailing newline has already been stripped.
//
// Supported commands:
//   home          — re-run the homing sweep
//   calibrate     — run the 10-revolution calibration
//   stop          — emergency stop
//   front         — go to 0° (dome front)
//   rand          — toggle random wander mode
//   abstick       — toggle absolute-stick mode
//   status        — print position and state
//   <N>           — go to absolute angle N (degrees, relative to front)
//   +<N>          — relative move +N degrees
//   -<N>          — relative move -N degrees
//   seqon[,<sec>] — suspend auto dome behaviors for <sec> seconds (watchdog)
//   seqoff        — clear any active sequence pause immediately
// ---------------------------------------------------------------------------

void AmidalaController::processDomeCommand(const char* cmd) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (strcmp(cmd, "home") == 0) {
    executeDomeAction(ButtonAction::kDomeHome, 0);
  } else if (strcmp(cmd, "calibrate") == 0) {
    executeDomeAction(ButtonAction::kDomeCalibrate, 0);
  } else if (strcmp(cmd, "stop") == 0) {
    executeDomeAction(ButtonAction::kDomeStop, 0);
  } else if (strcmp(cmd, "front") == 0) {
    executeDomeAction(ButtonAction::kDomeFront, 0);
  } else if (strcmp(cmd, "rand") == 0) {
    executeDomeAction(ButtonAction::kDomeRand, 0);
  } else if (strcmp(cmd, "abstick") == 0) {
    executeDomeAction(ButtonAction::kDomeAbsStick, 0);
  } else if (strcmp(cmd, "status") == 0) {
    fDomeDrive->printStatus(fConsole);
  } else if (strcmp(cmd, "seqon") == 0 || strncmp(cmd, "seqon,", 6) == 0) {
    // Suspend auto dome behaviors while an external controller runs a sequence.
    // Optional comma-argument gives the pause duration in seconds; omitted or
    // invalid = DEFAULT_DOME_SEQUENCE_PAUSE_MS.  Capped at DOME_SEQUENCE_PAUSE_MAX_MS.
    int argSec = (cmd[5] == ',') ? atoi(cmd + 6) : 0;
    uint32_t durationMs = dome_sequence_pause_duration_ms(
        argSec, DEFAULT_DOME_SEQUENCE_PAUSE_MS, DOME_SEQUENCE_PAUSE_MAX_MS);
    fDomeDrive->startSequencePause(durationMs);
  } else if (strcmp(cmd, "seqoff") == 0) {
    fDomeDrive->endSequencePause();
  } else if (cmd[0] == '+' && cmd[1] != '\0') {
    executeDomeAction(ButtonAction::kDomeRelPos, atoi(cmd + 1));
  } else if (cmd[0] == '-' && cmd[1] != '\0') {
    executeDomeAction(ButtonAction::kDomeRelNeg, atoi(cmd + 1));
  } else if (cmd[0] >= '0' && cmd[0] <= '9') {
    executeDomeAction(ButtonAction::kDomeGotoAbs, atoi(cmd));
  } else {
    fConsole.println(F("Dome: unknown command"));
    fConsole.println(F("  dome=home|calibrate|stop|front|rand|abstick|status|<N>|+<N>|-<N>"));
    fConsole.println(F("       |seqon[,<sec>]|seqoff"));
  }
#else
  (void)cmd;
  fConsole.println(F("Dome: RoboClaw drive not enabled in this build"));
#endif
}

// ---------------------------------------------------------------------------
// processDomeCmd()
// Executes a dome action from a button or gesture mapping.  subcmd is a
// ButtonAction::DomeCmdType value; arg carries the angle or delta in degrees
// for the three positional sub-commands (kDomeGotoAbs, kDomeRelPos,
// kDomeRelNeg) and is unused for the rest.
// ---------------------------------------------------------------------------

void AmidalaController::processDomeCmd(uint8_t subcmd, uint8_t arg) {
  executeDomeAction(subcmd, arg);
}
