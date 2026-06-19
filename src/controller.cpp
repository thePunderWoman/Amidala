#include "controller.h"
#include "dome_position_math.h"
#include "xbee_spi.h"

// Hardware globals defined in src/globals.cpp.
// ServoDispatch& avoids including ServoDispatchDirect.h here, which would
// pull in ServoDispatchPrivate.h and duplicate ISR definitions.
extern ServoDispatch& servoDispatch;

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
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER
      fTankDrive(128, DRIVE_SERIAL, fDriveStick),
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_PWM
      fTankDrive(servoDispatch, 1, 0, 2, fDriveStick),
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM
      fTankDrive(servoDispatch, 1, 0, 2, fDriveStick),
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL
      fTankDrive(DRIVE_SERIAL, fDriveStick),
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
      fTankDrive(DRIVE_SERIAL, servoDispatch, 1, 0, 4, fDriveStick),
#elif defined(DRIVE_SYSTEM)
#error Unsupported DRIVE_SYSTEM
#endif
#if DOME_DRIVE == DOME_DRIVE_SABER
      fDomeDrive(129, DOME_DRIVE_SERIAL, fDomeStick),
#elif DOME_DRIVE == DOME_DRIVE_PWM
      fDomeDrive(servoDispatch, 3, fDomeStick),
#elif DOME_DRIVE == DOME_DRIVE_ROBOCLAW
      fDomeDrive(ROBOCLAW_SERIAL,
                 DEFAULT_DOME_ROBOCLAW_ADDRESS,
                 DEFAULT_DOME_ROBOCLAW_CHANNEL,
                 DOME_HALL_PIN,
                 fDomeStick),
#endif
      fPPMDecoder(PPMIN_PIN, params.getRadioChannelCount()) {
}

void AmidalaController::emergencyStop() {
#ifdef DRIVE_SYSTEM
  fTankDrive.stop();
  // Force neutral PWM signals as a safety backup
  servoDispatch.moveTo(1, 0.5); // Drive Left
  servoDispatch.moveTo(0, 0.5); // Drive Right
  // If using serial, the Roboteq script handles its own RWD 100 watchdog
#endif
}

void AmidalaController::domeEmergencyStop() {
#ifdef DOME_DRIVE
  fDomeDrive.stop();
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
  fConsole.setMinimal(!loadConfig());
  fConfig.showCurrentConfiguration();

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

  pinMode(DOUT1_PIN, OUTPUT);
  pinMode(DOUT2_PIN, OUTPUT);
  pinMode(DOUT3_PIN, OUTPUT);
  pinMode(DOUT4_PIN, OUTPUT);
#ifdef DOUT7_PIN
  pinMode(DOUT7_PIN, OUTPUT);
#endif
#ifdef DOUT8_PIN
  pinMode(DOUT8_PIN, OUTPUT);
#endif

  pinMode(ANALOG2_PIN, INPUT);

  pinMode(PPMIN_PIN, INPUT);

#ifdef SEL2_PIN
  pinMode(SEL2_PIN, INPUT_PULLUP);
#endif
  setDigitalPin(1, false);
  setDigitalPin(2, false);
  setDigitalPin(3, false);
  setDigitalPin(4, false);
  setDigitalPin(5, false);
  setDigitalPin(6, false);
  setDigitalPin(7, false);
  setDigitalPin(8, false);

  fTankDrive.setMaxSpeed(MAXIMUM_SPEED);
  fTankDrive.setThrottleAccelerationScale(ACCELERATION_SCALE);
  fTankDrive.setThrottleDecelerationScale(DECELRATION_SCALE);
  fTankDrive.setTurnAccelerationScale(ACCELERATION_SCALE * 2);
  fTankDrive.setTurnDecelerationScale(DECELRATION_SCALE);
  fTankDrive.setGuestSpeedModifier(MAXIMUM_GUEST_SPEED);
  fTankDrive.setScaling(SCALING);
  fTankDrive.setChannelMixing(CHANNEL_MIXING);

#ifdef DOME_DRIVE
  fDomeDrive.setMaxSpeed(DOME_MAXIMUM_SPEED);
  fDomeDrive.setInverted(params.domeflip);
  fDomeDrive.setScaling(false);
  fDomeDrive.setThrottleAccelerationScale(DEFAULT_DOME_ACCELERATION_SCALE);
  fDomeDrive.setThrottleDecelerationScale(DEFAULT_DOME_DECELERATION_SCALE);
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  // Apply runtime-configurable RoboClaw parameters from the loaded config.
  fDomeDrive.setFrontOffset(params.domefront);
  fDomeDrive.setQPPS(params.domercqpps);
  fDomeDrive.setStallTimeout(params.domestall);
  fDomeDrive.setMaxSpeedPct(float(params.domespeed) / 100.0f);
  fDomeDrive.applyDomePositionParams(
      params.domeseekmin, params.domeseekmax,
      params.domeseekl,   params.domeseekr,
      params.domefudge,
      DEFAULT_DOME_SPEED_TARGET, params.domespeedmin);
  // setup() is called explicitly here (after config is applied) rather than
  // via the ReelTwo SetupEvent pool, so homing starts with correct parameters.
  fDomeDrive.setup();
#endif
#endif

  static const uint8_t kServoPins[] = {
      SERVO1_PIN, SERVO2_PIN, SERVO3_PIN,
  };
  for (unsigned i = 0; i < sizeof(kServoPins) / sizeof(kServoPins[0]); i++) {
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

    servoDispatch.setServo(i, kServoPins[i], minpulse, maxpulse,
                           neutralpulse, 0);
  }
}

void AmidalaController::animate() {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  fDomeDrive.animate();
#endif
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
    // digital out 7
    if (fDriveStateMillis + 1000 < millis()) {
      setDigitalPin(7, true);
      fDriveStateMillis = millis();
    }
  } else if (getDigitalPin(7) && fDriveStateMillis + 1000 < millis()) {
    setDigitalPin(7, false);
    fDriveStateMillis = millis();
  }
#endif

#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fabsf(fDomeDrive.getLastCommandedSpeed()) > 0.01f) {
    if (fDomeStateMillis + 1000 < millis()) {
      setDigitalPin(8, true);
      fDomeStateMillis = millis();
    }
  } else if (getDigitalPin(8)) {
    setDigitalPin(8, false);
    fDomeStateMillis = millis();
  }
#else
  if (fDomeDrive.isMoving()) {
    if (fDomeStateMillis + 1000 < millis()) {
      setDigitalPin(8, true);
      fDomeStateMillis = millis();
    }
  } else if (getDigitalPin(8)) {
    setDigitalPin(8, false);
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
    fDomeDrive.toggleRandomMode();
    break;
  case ButtonAction::kDomeStop:
    fDomeDrive.stop();
    break;
  case ButtonAction::kDomeFront:
    fDomeDrive.goToAngle(0);
    break;
  case ButtonAction::kDomeHome:
    fDomeDrive.startHoming();
    break;
  case ButtonAction::kDomeCalibrate:
    fDomeDrive.startCalibration();
    break;
  case ButtonAction::kDomeGotoAbs:
    fDomeDrive.goToAngle(arg);
    break;
  case ButtonAction::kDomeRelPos:
    fDomeDrive.goToRelative(arg);
    break;
  case ButtonAction::kDomeRelNeg:
    fDomeDrive.goToRelative(-arg);
    break;
  case ButtonAction::kDomeAbsStick:
    fDomeDrive.toggleAbsoluteStickMode();
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
    fDomeDrive.printStatus(fConsole);
  } else if (strcmp(cmd, "seqon") == 0 || strncmp(cmd, "seqon,", 6) == 0) {
    // Suspend auto dome behaviors while an external controller runs a sequence.
    // Optional comma-argument gives the pause duration in seconds; omitted or
    // invalid = DEFAULT_DOME_SEQUENCE_PAUSE_MS.  Capped at DOME_SEQUENCE_PAUSE_MAX_MS.
    int argSec = (cmd[5] == ',') ? atoi(cmd + 6) : 0;
    uint32_t durationMs = dome_sequence_pause_duration_ms(
        argSec, DEFAULT_DOME_SEQUENCE_PAUSE_MS, DOME_SEQUENCE_PAUSE_MAX_MS);
    fDomeDrive.startSequencePause(durationMs);
  } else if (strcmp(cmd, "seqoff") == 0) {
    fDomeDrive.endSequencePause();
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
