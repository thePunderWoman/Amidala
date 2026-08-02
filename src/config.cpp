#include "controller.h"

void AmidalaConfig::init(AmidalaController *controller) {
  fController = controller;
  fOutput = &controller->fConsole;
}

// ---------------------------------------------------------------------------
// Reassignable GPIO pin roles (issue #133) -- see include/pin_assignment.h
// ---------------------------------------------------------------------------

void AmidalaConfig::validatePinAssignments() {
  AmidalaParameters &params = fController->params;
  PinRoleType before[11];
  memcpy(before, params.pinRole, sizeof(before));

#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  bool requireHall = true;
#else
  bool requireHall = false;
#endif
  sanitizePinRoles(params.pinRole, requireHall);

  if (fOutput) {
    for (uint8_t i = 0; i < 11; i++) {
      if (before[i] == params.pinRole[i]) continue;
      fOutput->print(F("WARNING: pin config for GPIO"));
      fOutput->print(kAssignablePins[i]);
      fOutput->print(F(" was invalid or left the board without a required "
                       "role -- reset to "));
      fOutput->println(pinRoleToString(params.pinRole[i]));
    }
  }
}

// Parses "<match><type>" (e.g. "pin1role=analog"), validates the requested
// role against the current pinRole[] array (hardware ceilings, ADC1
// narrowing), and only writes params.pinRole[pinIndexOf(pin)] on success.
static bool applyPinRoleParam(AmidalaParameters &params, const char *cmd,
                              const char *match, uint8_t pin, Print *out) {
  if (!startswith(cmd, match)) return false;
  PinRoleType newRole;
  if (!pinRoleFromString(cmd, &newRole)) {
    if (out) out->println(F("pin role rejected: unrecognized role"));
    return false;
  }
  PinRoleValidationResult r = validateRoleChange(pin, newRole, params.pinRole);
  if (!r.ok) {
    if (out) {
      out->print(F("pin role rejected: "));
      out->println(r.reason);
    }
    return false;
  }
  params.pinRole[pinIndexOf(pin)] = newRole;
  return true;
}

// ---------------------------------------------------------------------------
// Reassignable dome/drive serial ports (issue #147) -- see
// include/serial_assignment.h
// ---------------------------------------------------------------------------

// Whether the compiled-in dome/drive subsystem consumes a serial port at
// all in this build. Shared by applySerialPortParam() and
// AmidalaConfig::validateSerialPortAssignments() below so the two "does this
// consumer even exist" checks can't drift apart.
static constexpr bool domeNeedsSerialPort() {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW || DOME_DRIVE == DOME_DRIVE_SABER
  return true;
#else
  return false;
#endif
}

static constexpr bool driveNeedsSerialPort() {
#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER || \
    DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL || \
    DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
  return true;
#else
  return false;
#endif
}

// Parses "<match><port>" (e.g. "domeserialport=serial2"), validates the
// requested port against whichever OTHER consumer (dome vs. drive) is
// active in this build, and only writes params.<...>SerialPort on success.
// Parsed unconditionally regardless of whether THIS consumer is active in
// the compiled build -- same reasoning as the RoboClaw-specific params
// above: keeps config.txt portable across builds. Whether the stored value
// means anything is decided at the construction site in controller.cpp's
// setup() (only consumed if this build's DOME_DRIVE/DRIVE_SYSTEM actually
// needs a serial port) and in the web UI's visibility rules.
static bool applySerialPortParam(AmidalaParameters &params, const char *cmd,
                                  const char *match, SerialConsumer consumer,
                                  Print *out) {
  if (!startswith(cmd, match)) return false;
  SerialPortId newPort;
  if (!serialPortFromString(cmd, &newPort)) {
    if (out) out->println(F("serial port rejected: unrecognized port"));
    return false;
  }
  bool otherActive = (consumer == SerialConsumer::kDome) ? driveNeedsSerialPort()
                                                          : domeNeedsSerialPort();
  SerialPortId otherPort = (consumer == SerialConsumer::kDome) ? params.driveSerialPort
                                                                : params.domeSerialPort;
  SerialPortValidationResult r =
      validateSerialPortChange(consumer, newPort, otherActive, otherPort);
  if (!r.ok) {
    if (out) {
      out->print(F("serial port rejected: "));
      out->println(r.reason);
    }
    return false;
  }
  if (consumer == SerialConsumer::kDome) params.domeSerialPort = newPort;
  else params.driveSerialPort = newPort;
  return true;
}

void AmidalaConfig::validateSerialPortAssignments() {
  AmidalaParameters &params = fController->params;
  if (domeNeedsSerialPort() && driveNeedsSerialPort() &&
      params.domeSerialPort == params.driveSerialPort) {
    // config.txt lines parse independently -- a conflicting pair could each
    // have individually looked valid at parse time (re-selecting their own
    // current port) before the other line landed. Reset dome back to its
    // default and keep drive's most-recently-parsed value, same "reset the
    // invalid one, log it" pattern as sanitizePinRoles().
    SerialPortId defDome, defDrive;
    defaultSerialPorts(defDome, defDrive);
    if (fOutput)
      fOutput->println(F("WARNING: dome/drive serial port conflict -- dome reset to default"));
    params.domeSerialPort = defDome;
  }
}

void AmidalaConfig::applyDomePositionParams() {
#ifdef DOME_DRIVE
  AmidalaParameters &params = fController->params;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  // fDomeDrive isn't constructed yet when config.txt is parsed at boot
  // (AmidalaController::setup() loads config before constructing it) --
  // params is already updated by the intparam()/boolparam() match above, and
  // setup() re-applies it to the drive once constructed, so skipping the
  // live-apply here is safe. Only a live (post-boot) config change needs it.
  if (fController->fDomeDrive)
    fController->fDomeDrive->applyDomePositionParams(
        params.domeseekmin, params.domeseekmax,
        params.domeseekl,   params.domeseekr,
        params.domefudge,
        DEFAULT_DOME_SPEED_TARGET, params.domespeedmin,
        params.domedecelzone);
#endif
#endif
}

void AmidalaConfig::showLoadEEPROM(bool load) {
  AmidalaParameters &params = fController->params;
  if (load) {
    fOutput->println(F("Loading config from EEPROM"));
    fController->params.init(true);
  } else {
    fOutput->println(F("EEPROM content"));
  }
  fOutput->print(F("J1="));
  fOutput->println(params.xbr, HEX);
  fOutput->print(F("J2="));
  fOutput->println(params.xbl, HEX);
  fOutput->print(F("J1VREF:"));
  fOutput->print(params.rvrmin);
  fOutput->print('-');
  fOutput->print(params.rvrmax);
  fOutput->print(F(" J2VREF:"));
  fOutput->print(params.rvlmin);
  fOutput->print('-');
  fOutput->println(params.rvlmax);
  for (unsigned i = 0; i < params.getServoCount(); i++) {
    fOutput->print('S');
    fOutput->print(i + 1);
    fOutput->print(F(": min="));
    fOutput->print(params.S[i].min);
    fOutput->print(F(", max="));
    fOutput->print(params.S[i].max);
    fOutput->print(F(", n="));
    fOutput->print(params.S[i].n);
    fOutput->print(F(", d="));
    fOutput->print(params.S[i].d);
    fOutput->print(F(", t="));
    fOutput->print(params.S[i].t);
    fOutput->print(F(", s="));
    fOutput->print(params.S[i].s);
    fOutput->print(F(", minpulse="));
    fOutput->print(params.S[i].minpulse);
    fOutput->print(F(", maxpulse="));
    fOutput->print(params.S[i].maxpulse);
    if (params.S[i].r)
      fOutput->print(F(" rev"));
    fOutput->println();
  }
  fOutput->print(F("Failsafe:"));
  fOutput->println(params.fst);
}

void AmidalaConfig::showCurrentConfiguration() {
  char gesture[MAX_GESTURE_LENGTH + 1];
  AmidalaParameters &params = fController->params;
  if (fController->fConsole.isMinimal())
    fOutput->println(F("Config (Minimal EEPROM Mode)"));
  else
    fOutput->println(F("Config"));
  fOutput->println();
  fOutput->print(F("J1:"));
  fOutput->print(params.xbr, HEX);
  fOutput->print(F(" J2:"));
  fOutput->println(params.xbl, HEX);
  fOutput->print(F("J1VREF:"));
  fOutput->print(params.rvrmin);
  fOutput->print('-');
  fOutput->print(params.rvrmax);
  fOutput->print(F(" J2VREF:"));
  fOutput->print(params.rvlmin);
  fOutput->print('-');
  fOutput->println(params.rvlmax);
  fOutput->print(F("J1 V/H Adjust:")); // NOT SUPPORTED
  fOutput->print(45);
  fOutput->print(',');
  fOutput->println(45);
  if (!fController->fConsole.isMinimal()) {
    AmidalaParameters::SoundBank *sb = &params.SB[0];

    fOutput->print(F("Vol: "));
    fOutput->println(params.volume);
    fOutput->print(F("Rnd Sound: "));
    fOutput->println(params.rndon ? F("On") : F("Off"));
    fOutput->print(F("Random Sound Delay: "));
    fOutput->print(params.mindelay);
    fOutput->print(F(" - "));
    fOutput->println(params.maxdelay);
    fOutput->print(F("Ack Sound: "));
    fOutput->println(params.ackon ? F("On") : F("Off"));
    fOutput->print(F("Ack Types: "));
    fOutput->println("AutoDome,Disable,Slow-Mode");
    fOutput->println();
    fOutput->print(F("Sound Banks: "));
    fOutput->print(params.sbcount);
    fOutput->print(F(" ("));
    fOutput->print(params.getSoundBankCount());
    fOutput->println(F(" Max)"));
    for (unsigned i = 0; i < params.sbcount; i++, sb++) {
      fOutput->print(i + 1);
      fOutput->print(F(": "));
      fOutput->print(sb->dir);
      fOutput->print(F(" ("));
      fOutput->print(sb->numfiles);
      fOutput->print(F(")"));
      if (sb->numfiles > 1 && sb->random)
        fOutput->print(F(" (rand)"));
      fOutput->println();
    }
    fOutput->println();
    fOutput->print(F("My i2c Address: "));
    fOutput->println(params.myi2c);
    fOutput->print(F("Configured i2c devices: "));
    fOutput->print(1);
    fOutput->println(F(" (99)"));
    fOutput->println();
    fOutput->println(F("Buttons:"));
    for (unsigned i = 0; i < params.getButtonCount() - 1; i++) {
      if (params.B[i].action != 0) {
        fOutput->print(i + 1);
        fOutput->print(F(": "));
        params.B[i].printDescription(fOutput);
      }
    }
    fOutput->println();
    fOutput->println(F("Long Buttons:"));
    for (unsigned i = 0; i < params.getButtonCount() - 1; i++) {
      if (params.LB[i].action != 0) {
        fOutput->print(i + 1);
        fOutput->print(F(": "));
        params.LB[i].printDescription(fOutput);
      }
    }
    fOutput->println();
    fOutput->print(F("altbtn: "));
    fOutput->println(params.altbtn);
    fOutput->print(F("altdomestick: "));
    fOutput->println(params.altdomestick);
    if (params.altbtn != 0) {
      fOutput->println(F("Alt Buttons:"));
      for (unsigned i = 0; i < params.getButtonCount() - 1; i++) {
        if (params.AB[i].action != 0) {
          fOutput->print(i + 1);
          fOutput->print(F(": "));
          params.AB[i].printDescription(fOutput);
        }
      }
    }
    switch (params.b9) {
    case 'y':
    case 'b':
      fOutput->print(9);
      fOutput->print(F(": "));
      params.B[8].printDescription(fOutput);
      break;
    case 'n':
    case 'k':
      fOutput->println(F("Enable/Disable Remotes"));
      break;
    case 's':
      fOutput->println(F("Slow/Fast Mode"));
      break;
    case 'd':
      fOutput->println(F("Enable/Disable Drives"));
      break;
    }
    fOutput->println();
    fOutput->println(F("Gestures:"));
    if (!params.slowgest.isEmpty()) {
      fOutput->print(params.slowgest.getGestureString(gesture));
      fOutput->println(F(" Go Slow On/Off"));
    }
    if (!params.ackgest.isEmpty()) {
      fOutput->print(params.ackgest.getGestureString(gesture));
      fOutput->println(F(" Ack On/Off"));
    }
    if (!params.rnd.isEmpty()) {
      fOutput->print(params.rnd.getGestureString(gesture));
      fOutput->println(F(" Rnd Sound On/Off"));
    }
    for (unsigned i = 0; i < params.getGestureCount(); i++) {
      params.G[i].printDescription(fOutput);
    }

    fOutput->println();
    if (params.autocorrect)
      fOutput->println(F("Auto Correct Gestures Enabled"));

    fOutput->print(F("Serial Baud: "));
    fOutput->println(params.serialbaud);
    fOutput->print(F("Serial Delimiter: "));
    fOutput->println((char)params.serialdelim);
    fOutput->print(F("Serial EOL: "));
    fOutput->println(params.serialeol);
    fOutput->print(F("Serial Init: "));
    fOutput->println();
    fOutput->println(F("Serial String Commands:"));
    fOutput->println();
    fOutput->print(F("Bluetooth Controller: "));
    fOutput->println(params.btcontrolleron ? F("On") : F("Off"));
    fOutput->print(F("WCB Client: "));
    fOutput->println(params.wcbenable ? F("On") : F("Off"));
    fOutput->print(F("WCB Outbound: "));
    fOutput->println(params.outboundserial == 1 ? F("WCB Mesh") : F("UART0"));
    fOutput->print(F("WiFi AP: "));
    fOutput->println(params.wifion ? F("On") : F("Off"));
    fOutput->print(F("WiFi SSID: "));
    fOutput->println(params.wifiSSID);
    fOutput->print(F("WiFi Password: "));
    fOutput->println(params.wifiPassword);
    for (unsigned i = 0; i < params.serialcount; i++) {
      fOutput->print(i + 1);
      fOutput->print(F(": "));
      fOutput->print(params.Str[i].name);
      fOutput->print(F(" | "));
      fOutput->println(params.Str[i].str);
    }
    fOutput->println();
    fOutput->print(F("domemode: "));
    fOutput->println(params.domemode);
    fOutput->print(F("domehome: "));
    fOutput->println(params.domehome);
    fOutput->print(F("domeflip: "));
    fOutput->println(params.domeflip ? F("true") : F("false"));
    fOutput->print(F("domegest: "));
    fOutput->println(params.domegest.getGestureString(gesture));
    fOutput->print(F("domeseekmin: "));
    fOutput->println(params.domeseekmin);
    fOutput->print(F("domeseekmax: "));
    fOutput->println(params.domeseekmax);
    fOutput->print(F("domefudge: "));
    fOutput->println(params.domefudge);
    fOutput->print(F("domeseekl: "));
    fOutput->println(params.domeseekl);
    fOutput->print(F("domeseekr: "));
    fOutput->println(params.domeseekr);
    fOutput->print(F("domespeed: "));
    fOutput->println(params.domespeed);
    fOutput->print(F("domespeedhome: "));
    fOutput->println(params.domespeedhome);
    fOutput->print(F("domespeedseek: "));
    fOutput->println(params.domespeedseek);
    fOutput->print(F("domespeedmin: "));
    fOutput->println(params.domespeedmin);
    fOutput->print(F("domedecelzone: "));
    fOutput->println(params.domedecelzone);
    fOutput->print(F("domech6: "));
    fOutput->println(params.domech6 ? F("true") : F("false"));
    fOutput->println();
  }
  fOutput->println();
  fOutput->println(F("Channel/Servos:"));
  for (unsigned i = 0; i < params.getServoCount(); i++) {
    fOutput->print('S');
    fOutput->print(i + 1);
    fOutput->print(F(": min="));
    fOutput->print(params.S[i].min);
    fOutput->print(F(", max="));
    fOutput->print(params.S[i].max);
    fOutput->print(F(", n="));
    fOutput->print(params.S[i].n);
    fOutput->print(F(", d="));
    fOutput->print(params.S[i].d);
    fOutput->print(F(", t="));
    fOutput->print(params.S[i].t);
    fOutput->print(F(", s="));
    fOutput->print(params.S[i].s);
    fOutput->print(F(", minpulse="));
    fOutput->print(params.S[i].minpulse);
    fOutput->print(F(", maxpulse="));
    fOutput->print(params.S[i].maxpulse);
    if (params.S[i].r)
      fOutput->print(F(" rev"));
    fOutput->println();
  }
  fOutput->print(F("Failsafe:"));
  fOutput->println(params.fst);
  // Not supported
  fOutput->println(F("Remote Console Cmd On"));
}

void AmidalaConfig::writeCurrentConfiguration() {
  AmidalaParameters &params = fController->params;
  size_t offs = 0xea;
  for (unsigned i = 0; i < sizeof(params.S) / sizeof(params.S[0]); i++) {
    /*EEPROM.put(offs, S[i].min);*/ offs += sizeof(params.S[i].min);
    /*EEPROM.put(offs, S[i].max);*/ offs += sizeof(params.S[i].max);
    /*EEPROM.put(offs, S[i].n);*/ offs += sizeof(params.S[i].n);
    /*EEPROM.put(offs, S[i].d);*/ offs += sizeof(params.S[i].d);
    /*EEPROM.put(offs, S[i].t);*/ offs += sizeof(params.S[i].t);
    /*EEPROM.put(offs, S[i].r);*/ offs += sizeof(params.S[i].r);
    // S[i].s = (uint8_t)ceil((float)EEPROM.read(offs++) / 255.0f * 10) * 10;
    offs += sizeof(params.S[i].s);
    EEPROM.put(offs, params.S[i].minpulse);
    offs += sizeof(params.S[i].minpulse);
    EEPROM.put(offs, params.S[i].maxpulse);
    offs += sizeof(params.S[i].maxpulse);
  }
  EEPROM.commit();  // required on ESP32 — flushes RAM buffer to NVS flash
  fOutput->println(F("Updated"));
}

// Parse one button action line (cmd already past the "b="/"lb="/"ab="/"db=" prefix).
// Format: ButtonNum,ActionType[,Arg1[,Arg2[,SerialID]]]
// SerialID is the stable numeric ID of a serial string.  For kSerialStr it is the
// primary reference (position 3); for kI2CStr it is position 4; for all other
// actions it is an optional side-effect appended as the last numeric field.
static bool parseButtonLine(const char *cmd, AmidalaParameters &params,
                            ButtonAction *arr) {
  uint16_t args[6] = {};
  uint8_t argcount = parseUintArgs(cmd, args, 6);
  if (argcount < 2 || args[0] < 1 || args[0] > params.getButtonCount())
    return false;
  ButtonAction *b = &arr[args[0] - 1];
  memset(b, '\0', sizeof(*b));
  b->action = (uint8_t)args[1];
  switch (args[1]) {
  case ButtonAction::kSound:
    b->sound.soundbank = (uint8_t)max(1, (int)min((int)args[2], (int)params.sbcount));
    b->sound.sound = (argcount >= 4) ? (uint8_t)args[3] : 0;
    if (b->sound.soundbank > 0)
      b->sound.sound = (uint8_t)min((int)b->sound.sound, (int)params.SB[b->sound.soundbank - 1].numfiles);
    b->serialid = (argcount >= 5) ? args[4] : 0;
    break;
  case ButtonAction::kServo:
    b->servo.num = (uint8_t)max(1, min((int)args[2], 8));
    b->servo.pos = (argcount >= 4) ? (uint8_t)min(max((int)args[3], 0), 180) : 0;
    b->serialid = (argcount >= 5) ? args[4] : 0;
    break;
  case ButtonAction::kDigitalOut:
    b->dout.num   = (uint8_t)max(1, min((int)args[2], 8));
    b->dout.state = (argcount >= 4) ? (uint8_t)min(2, (int)args[3]) : 0;
    b->serialid = (argcount >= 5) ? args[4] : 0;
    break;
  case ButtonAction::kI2CCmd:
    b->i2ccmd.target = (uint8_t)min((int)args[2], 100);
    b->i2ccmd.cmd    = (argcount >= 4) ? (uint8_t)args[3] : 0;
    b->serialid = (argcount >= 5) ? args[4] : 0;
    break;
  case ButtonAction::kSerialStr:
    b->serialid = args[2];
    DEBUG_PRINT("BUTTON SERIAL ID: ");
    DEBUG_PRINTLN(b->serialid);
    break;
  case ButtonAction::kI2CStr:
    b->i2cstr.target = (uint8_t)min((int)args[2], 100);
    b->serialid = args[3];
    break;
  case ButtonAction::kHCREmote:
    b->emote.emotion = (uint8_t)min((int)args[2], 4);
    b->emote.level   = (argcount >= 4) ? (uint8_t)min((int)args[3], 1) : 0;
    b->serialid = (argcount >= 5) ? args[4] : 0;
    break;
  case ButtonAction::kHCRMuse:
    b->serialid = (argcount >= 3) ? args[2] : 0;
    break;
  case ButtonAction::kDomeCmd:
    b->dome.subcmd = (uint8_t)args[2];
    b->dome.arg    = (argcount >= 4) ? (uint8_t)args[3] : 0;
    b->serialid = (argcount >= 5) ? args[4] : 0;
    break;
  default:
    b->action = 0;
    break;
  }
  return true;
}

// ---------------------------------------------------------------------------
// config.txt key dispatch (issue #171)
//
// processConfig() used to be one large if/else-if chain matching every key,
// which grew large enough on the ESP32-S3/Xtensa toolchain to trip a
// link-time "dangerous relocation: windowed longcall crosses 1GB boundary"
// error -- a hard limit on a single function's compiled body size, not a
// logic bug (compiler-flag workarounds were tried and ruled out; only the
// function's own size matters). #170 bought headroom by splitting
// dome/RoboClaw keys into a separate processDomeConfig() sub-dispatch, but
// that was always a stopgap -- nothing stopped the same error from
// resurfacing once either function grew again.
//
// The real fix: one handler function per key (or per key-variant, e.g.
// domepos='s two comma-count overloads), referenced from kConfigHandlers
// below. processConfig() itself is now a fixed-size loop over that table --
// its compiled size no longer grows with the number of config keys, so this
// class of link error can't recur no matter how many settings get added
// later. processDomeConfig() as a separate concept is retired: its keys are
// just more entries in the same table now.
//
// Each handler is a mechanical extraction of what used to be one branch (or
// one `||`-chained arm) of the old chain -- same key string, same helper
// call, same bounds, same side effects. A few keys' original branches had no
// explicit `return true;` on their success path and relied on falling
// through to the chain's single shared `return false;` at the very end --
// preserved exactly below (noted per-function) rather than "fixed", since
// wifi_ap.cpp's `POST /api/config` genuinely branches on this return value
// today and changing it would be an observable behavior change out of scope
// for this refactor.
// ---------------------------------------------------------------------------

// ---- Sound banks / servos / buttons ----------------------------------------

bool AmidalaConfig::cfg_sb(const char *cmd) {
  if (!startswith(cmd, "sb="))
    return false;
  AmidalaParameters &params = fController->params;
  if (params.sbcount < params.getSoundBankCount()) {
    AmidalaParameters::SoundBank *sb = &params.SB[params.sbcount];
    char *dirname = sb->dir;
    memset(sb, '\0', sizeof(*sb));
    for (unsigned i = 0;
         *cmd != '\0' && *cmd != ',' && i < sizeof(sb->dir) - 1; i++) {
      dirname[i] = *cmd++;
      dirname[i + 1] = '\0';
    }
    if (*cmd == ',') {
      sb->numfiles = strtolu(++cmd, &cmd);
      if (*cmd == ',') {
        if (cmd[1] == 's')
          sb->random = false;
        else if (cmd[1] == 'r')
          sb->random = true;
        else
          return false;
      } else {
        sb->random = true;
      }
      if (*cmd == '\0') {
        params.sbcount++;
        return true;
      }
    }
  }
  // Falls through to false on a full table or malformed input -- matches
  // the original chain's shared trailing `return false;`.
  return false;
}

bool AmidalaConfig::cfg_s(const char *cmd) {
  if (!startswith(cmd, "s="))
    return false;
  AmidalaParameters &params = fController->params;
  uint8_t argcount;
  int args[10];
  memset(args, '\0', sizeof(args));
  AmidalaParameters::Channel *s = params.S;
  if (numberparams(cmd, argcount, args, sizeof(args)) && argcount >= 3 &&
      args[0] >= 1 && args[0] <= int(params.getServoCount())) {
    unsigned num = args[0] - 1;
    s += num;
    s->min = min(max(args[1], 0), 180);
    s->max = max(min(args[2], 180), 0);
    s->n = (argcount >= 4) ? max(min(args[3], 180), 0)
                           : (s->min + (s->max - s->min) / 2);
    s->d = (argcount >= 5) ? max(min(args[4], 180), 0) : 0;
    s->t = (argcount >= 6) ? args[5] : 0;
    s->s = (argcount >= 7) ? max(min(args[6], 100), 0) : 100;
    s->r = (argcount >= 8) ? max(min(args[7], 1), 0) : 0;
    s->minpulse =
        (argcount >= 9) ? min(max(args[8], 800), 2400) : params.minpulse;
    s->maxpulse =
        (argcount >= 10) ? min(max(args[9], 800), 2400) : params.maxpulse;

    float neutral = float(s->n) / float(s->max);
    uint16_t minpulse = s->minpulse;
    uint16_t maxpulse = s->maxpulse;
    if (s->r) {
      maxpulse = s->minpulse;
      minpulse = s->maxpulse;
    }
    applyServoConfig(num, minpulse, maxpulse, neutral);
    return true;
  }
  // Falls through to false on malformed input -- matches the original
  // chain's shared trailing `return false;`.
  return false;
}

bool AmidalaConfig::cfg_b(const char *cmd) {
  if (!startswith(cmd, "b="))
    return false;
  return parseButtonLine(cmd, fController->params, fController->params.B);
}

bool AmidalaConfig::cfg_lb(const char *cmd) {
  if (!startswith(cmd, "lb="))
    return false;
  return parseButtonLine(cmd, fController->params, fController->params.LB);
}

bool AmidalaConfig::cfg_ab(const char *cmd) {
  if (!startswith(cmd, "ab="))
    return false;
  return parseButtonLine(cmd, fController->params, fController->params.AB);
}

bool AmidalaConfig::cfg_db(const char *cmd) {
  if (!startswith(cmd, "db="))
    return false;
  return parseButtonLine(cmd, fController->params, fController->params.DB);
}

// ---- Serial strings / favorites / categories / safety commands ------------

bool AmidalaConfig::cfg_sstr(const char *cmd) {
  if (!startswith(cmd, "sstr="))
    return false;
  AmidalaParameters &params = fController->params;
  if (params.serialcount >= params.getSerialStringCount())
    return true; // silently ignore when full
  SerialString *a = &params.Str[params.serialcount];
  const char* val = cmd; // startswith already advanced past "sstr="
  // Format: ID|Name|command
  // Parse the leading numeric ID
  const char *end;
  uint16_t id = (uint16_t)strtolu(val, &end);
  if (end > val && *end == '|') {
    // New format: ID|Name|command
    a->id = id;
    val = end + 1; // skip past first '|'
  } else {
    a->id = 0; // will be assigned below after all strings are loaded
  }
  const char* pipe = strchr(val, '|');
  if (pipe) {
    size_t nlen = (size_t)(pipe - val);
    if (nlen >= sizeof(a->name)) nlen = sizeof(a->name) - 1;
    memcpy(a->name, val, nlen);
    a->name[nlen] = '\0';
    strncpy(a->str, pipe + 1, sizeof(a->str) - 1);
    a->str[sizeof(a->str) - 1] = '\0';
  } else {
    a->name[0] = '\0';
    strncpy(a->str, val, sizeof(a->str) - 1);
    a->str[sizeof(a->str) - 1] = '\0';
  }
  params.serialcount++;
  // Track the highest ID seen so nextSstrId stays above it
  if (a->id >= params.nextSstrId)
    params.nextSstrId = a->id + 1;
  // No explicit return on the success path in the original branch -- falls
  // through to the chain's shared `return false;`. Preserved exactly:
  // wifi_ap.cpp's POST /api/config genuinely returns an error response for
  // a successful sstr= today, and changing that is out of scope here.
  return false;
}

bool AmidalaConfig::cfg_fav(const char *cmd) {
  if (!startswith(cmd, "f="))
    return false;
  AmidalaParameters &params = fController->params;
  // Favorites: f=1,3,5 (comma-separated 1-based sstr indices)
  const char* p = cmd; // past "f="
  params.sstr_fav_cnt = 0;
  while (*p && params.sstr_fav_cnt < MAX_SSTR_FAVS) {
    uint16_t v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    if (v > 0) params.sstr_favs[params.sstr_fav_cnt++] = v;
    if (*p == ',') p++;
  }
  // No explicit return in the original branch -- preserved fall-through to
  // false (see cfg_sstr()'s comment).
  return false;
}

bool AmidalaConfig::cfg_hidden(const char *cmd) {
  if (!startswith(cmd, "hidden="))
    return false;
  AmidalaParameters &params = fController->params;
  // Hidden: hidden=2,4 (comma-separated 1-based sstr indices hidden from Droid Control)
  const char* p = cmd; // past "hidden="
  params.sstr_hidden_cnt = 0;
  while (*p && params.sstr_hidden_cnt < MAX_SSTR_HIDDEN) {
    uint16_t v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    if (v > 0) params.sstr_hidden[params.sstr_hidden_cnt++] = v;
    if (*p == ',') p++;
  }
  // No explicit return in the original branch -- preserved fall-through to
  // false (see cfg_sstr()'s comment).
  return false;
}

bool AmidalaConfig::cfg_cat(const char *cmd) {
  if (!startswith(cmd, "cat="))
    return false;
  AmidalaParameters &params = fController->params;
  // Category: cat=Name|1,3,5
  if (params.sstr_cat_count >= MAX_SSTR_CATS) return true;
  const char* val = cmd; // past "cat="
  const char* pipe = strchr(val, '|');
  if (!pipe) return true;
  AmidalaParameters::SstrCat* cat = &params.sstr_cats[params.sstr_cat_count];
  size_t nlen = (size_t)(pipe - val);
  if (nlen >= sizeof(cat->name)) nlen = sizeof(cat->name) - 1;
  memcpy(cat->name, val, nlen);
  cat->name[nlen] = '\0';
  cat->cnt = 0;
  const char* p = pipe + 1;
  while (*p && cat->cnt < MAX_SSTR_CAT_ENTRIES) {
    uint16_t v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    if (v > 0) cat->idx[cat->cnt++] = v;
    if (*p == ',') p++;
  }
  params.sstr_cat_count++;
  // The original branch returns true on its two early-exit paths above
  // (table full, malformed/no '|') but falls through to false here on a
  // genuinely successful parse -- preserved exactly, not "fixed" (see
  // cfg_sstr()'s comment).
  return false;
}

bool AmidalaConfig::cfg_estopstr(const char *cmd) {
  if (!startswith(cmd, "estopstr="))
    return false;
  AmidalaParameters &params = fController->params;
  if (params.estopCmdCount < MAX_SAFETY_CMDS && *cmd) {
    strncpy(params.EstopCmds[params.estopCmdCount].str, cmd, sizeof(AmidalaParameters::SafetyCmd::str) - 1);
    params.EstopCmds[params.estopCmdCount].str[sizeof(AmidalaParameters::SafetyCmd::str) - 1] = '\0';
    params.estopCmdCount++;
  }
  // No explicit return in the original branch -- preserved fall-through to
  // false (see cfg_sstr()'s comment).
  return false;
}

bool AmidalaConfig::cfg_resumestr(const char *cmd) {
  if (!startswith(cmd, "resumestr="))
    return false;
  AmidalaParameters &params = fController->params;
  if (params.resumeCmdCount < MAX_SAFETY_CMDS && *cmd) {
    strncpy(params.ResumeCmds[params.resumeCmdCount].str, cmd, sizeof(AmidalaParameters::SafetyCmd::str) - 1);
    params.ResumeCmds[params.resumeCmdCount].str[sizeof(AmidalaParameters::SafetyCmd::str) - 1] = '\0';
    params.resumeCmdCount++;
  }
  // No explicit return in the original branch -- preserved fall-through to
  // false (see cfg_sstr()'s comment).
  return false;
}

// ---- Gestures ---------------------------------------------------------------

bool AmidalaConfig::cfg_gesture(const char *cmd) {
  if (!startswith(cmd, "g="))
    return false;
  AmidalaParameters &params = fController->params;
  char gesture[MAX_GESTURE_LENGTH + 1];
  char *gesture_end = &gesture[sizeof(gesture) - 1];
  char *gest = gesture;
  while (*cmd != ',' && *cmd != '\0') {
    if (gest <= gesture_end) {
      *gest++ = *cmd;
      *gest = '\0';
    }
    cmd++;
  }
  if (*cmd == ',')
    cmd++;
  GestureAction *g =
      &params.G[min((unsigned)params.gcount, params.getGestureCount() - 1)];
  ButtonAction *b = &g->action;
  g->gesture.setGesture(gesture);
  if (!g->gesture.isEmpty()) {
    uint16_t args[5] = {};
    uint8_t argcount = parseUintArgs(cmd, args, 5);
    if (argcount >= 1) {
      memset(b, '\0', sizeof(*b));
      b->action = (uint8_t)args[0];
      switch (args[0]) {
      case ButtonAction::kSound:
        b->sound.soundbank = (uint8_t)max(1, (int)min((int)args[1], (int)params.sbcount));
        b->sound.sound = (argcount >= 3) ? (uint8_t)args[2] : 0;
        b->serialid = (argcount >= 4) ? args[3] : 0;
        break;
      case ButtonAction::kServo:
        b->servo.num = (uint8_t)max(1, min((int)args[1], 8));
        b->servo.pos = (argcount >= 3) ? (uint8_t)min(max((int)args[2], 0), 180) : 0;
        b->serialid = (argcount >= 4) ? args[3] : 0;
        break;
      case ButtonAction::kDigitalOut:
        b->dout.num   = (uint8_t)max(1, min((int)args[1], 8));
        b->dout.state = (argcount >= 3) ? (uint8_t)min(2, (int)args[2]) : 0;
        b->serialid = (argcount >= 4) ? args[3] : 0;
        break;
      case ButtonAction::kI2CCmd:
        b->i2ccmd.target = (uint8_t)min((int)args[1], 100);
        b->i2ccmd.cmd    = (argcount >= 3) ? (uint8_t)args[2] : 0;
        b->serialid = (argcount >= 4) ? args[3] : 0;
        break;
      case ButtonAction::kSerialStr:
        b->serialid = args[1];
        DEBUG_PRINT("GESTURE SERIAL ID: ");
        DEBUG_PRINTLN(b->serialid);
        break;
      case ButtonAction::kI2CStr:
        b->i2cstr.target = (uint8_t)min((int)args[1], 100);
        b->serialid = args[2];
        break;
      case ButtonAction::kHCREmote:
        b->emote.emotion = (uint8_t)min((int)args[1], 4);
        b->emote.level   = (argcount >= 3) ? (uint8_t)min((int)args[2], 1) : 0;
        b->serialid = (argcount >= 4) ? args[3] : 0;
        break;
      case ButtonAction::kHCRMuse:
        b->serialid = (argcount >= 2) ? args[1] : 0;
        break;
      case ButtonAction::kDomeCmd:
        b->dome.subcmd = (uint8_t)args[1];
        b->dome.arg    = (argcount >= 3) ? (uint8_t)args[2] : 0;
        b->serialid = (argcount >= 4) ? args[3] : 0;
        break;
      default:
        b->action = 0;
        break;
      }
      if (params.gcount < params.getGestureCount())
        params.gcount++;
      return true;
    }
  }
  return false;
}

// ---- Audio hardware selection ------------------------------------------------

bool AmidalaConfig::cfg_audiohw(const char *cmd) {
  if (!startswith(cmd, "audiohw="))
    return false;
  AmidalaParameters &params = fController->params;
  if (startswith(cmd, "hcr"))    params.audiohw = AUDIO_HW_HCR;
  else if (startswith(cmd, "vmusic")) params.audiohw = AUDIO_HW_VMUSIC;
  return true;
}

// ---- Simple scalar settings (single key -> single params field) ------------

bool AmidalaConfig::cfg_acktype(const char *cmd) {
  return charparam(cmd, "acktype=", "gadsr", fController->params.acktype);
}
bool AmidalaConfig::cfg_b9(const char *cmd) {
  return charparam(cmd, "b9=", "ynksdb", fController->params.b9);
}
bool AmidalaConfig::cfg_volume(const char *cmd) {
  return intparam(cmd, "volume=", fController->params.volume, 0, 100);
}
bool AmidalaConfig::cfg_volumeChA(const char *cmd) {
  return intparam(cmd, "volumeChA=", fController->params.volumeChA, 0, 100);
}
bool AmidalaConfig::cfg_volumeChB(const char *cmd) {
  return intparam(cmd, "volumeChB=", fController->params.volumeChB, 0, 100);
}
bool AmidalaConfig::cfg_volumewheel(const char *cmd) {
  return intparam(cmd, "volumewheel=", fController->params.volumewheel, 0, 4);
}
bool AmidalaConfig::cfg_altvolumewheel(const char *cmd) {
  return intparam(cmd, "altvolumewheel=", fController->params.altvolumewheel, 0, 4);
}
bool AmidalaConfig::cfg_startupem(const char *cmd) {
  return intparam(cmd, "startupem=", fController->params.startupem, 0, 4);
}
bool AmidalaConfig::cfg_startuplvl(const char *cmd) {
  return intparam(cmd, "startuplvl=", fController->params.startuplvl, 0, 1);
}
bool AmidalaConfig::cfg_ackem(const char *cmd) {
  return intparam(cmd, "ackem=", fController->params.ackem, 0, 4);
}
bool AmidalaConfig::cfg_acklvl(const char *cmd) {
  return intparam(cmd, "acklvl=", fController->params.acklvl, 0, 1);
}
bool AmidalaConfig::cfg_mindelay(const char *cmd) {
  return intparam(cmd, "mindelay=", fController->params.mindelay, 0, 1000);
}
bool AmidalaConfig::cfg_maxdelay(const char *cmd) {
  return intparam(cmd, "maxdelay=", fController->params.maxdelay, 0, 1000);
}
bool AmidalaConfig::cfg_rvrmin(const char *cmd) {
  return intparam(cmd, "rvrmin=", fController->params.rvrmin, 0, 100);
}
bool AmidalaConfig::cfg_rvrmax(const char *cmd) {
  return intparam(cmd, "rvrmax=", fController->params.rvrmax, 900, 1023);
}
bool AmidalaConfig::cfg_rvlmin(const char *cmd) {
  return intparam(cmd, "rvlmin=", fController->params.rvlmin, 0, 100);
}
bool AmidalaConfig::cfg_rvlmax(const char *cmd) {
  return intparam(cmd, "rvlmax=", fController->params.rvlmax, 900, 1023);
}
bool AmidalaConfig::cfg_minpulse(const char *cmd) {
  return intparam(cmd, "minpulse=", fController->params.minpulse, 0, 2500);
}
bool AmidalaConfig::cfg_maxpulse(const char *cmd) {
  return intparam(cmd, "maxpulse=", fController->params.maxpulse, 0, 2500);
}
bool AmidalaConfig::cfg_rcchn(const char *cmd) {
  return intparam(cmd, "rcchn=", fController->params.rcchn, 6, 8);
}
bool AmidalaConfig::cfg_rcd(const char *cmd) {
  return intparam(cmd, "rcd=", fController->params.rcd, 1, 50);
}
bool AmidalaConfig::cfg_rcj(const char *cmd) {
  return intparam(cmd, "rcj=", fController->params.rcj, 1, 40);
}
bool AmidalaConfig::cfg_myi2c(const char *cmd) {
  return intparam(cmd, "myi2c=", fController->params.myi2c, 0, 100);
}
bool AmidalaConfig::cfg_serialbaud(const char *cmd) {
  return intparam(cmd, "serialbaud=", fController->params.serialbaud, 300, 115200);
}
bool AmidalaConfig::cfg_serialdelim(const char *cmd) {
  return intparam(cmd, "serialdelim=", fController->params.serialdelim, 0, 255);
}
bool AmidalaConfig::cfg_serialeol(const char *cmd) {
  return intparam(cmd, "serialeol=", fController->params.serialeol, 0, 255);
}
bool AmidalaConfig::cfg_fst(const char *cmd) {
  return intparam(cmd, "fst=", fController->params.fst, 1000, 3000);
}
bool AmidalaConfig::cfg_j1adjv(const char *cmd) {
  return intparam(cmd, "j1adjv=", fController->params.j1adjv, 0, 80);
}
bool AmidalaConfig::cfg_j1adjh(const char *cmd) {
  return intparam(cmd, "j1adjh=", fController->params.j1adjh, 0, 80);
}
bool AmidalaConfig::cfg_rnd(const char *cmd) {
  return gestureparam(cmd, "rnd=", fController->params.rnd);
}
bool AmidalaConfig::cfg_ackgest(const char *cmd) {
  return gestureparam(cmd, "ackgest=", fController->params.ackgest);
}
bool AmidalaConfig::cfg_slowgest(const char *cmd) {
  return gestureparam(cmd, "slowgest=", fController->params.slowgest);
}
bool AmidalaConfig::cfg_domegest(const char *cmd) {
  return gestureparam(cmd, "domegest=", fController->params.domegest);
}
bool AmidalaConfig::cfg_startup(const char *cmd) {
  return boolparam(cmd, "startup=", fController->params.startup);
}
bool AmidalaConfig::cfg_rndon(const char *cmd) {
  return boolparam(cmd, "rndon=", fController->params.rndon);
}
bool AmidalaConfig::cfg_ackon(const char *cmd) {
  return boolparam(cmd, "ackon=", fController->params.ackon);
}
bool AmidalaConfig::cfg_mix12(const char *cmd) {
  return boolparam(cmd, "mix12=", fController->params.mix12);
}
bool AmidalaConfig::cfg_autocorrect(const char *cmd) {
  return boolparam(cmd, "auto=", fController->params.autocorrect);
}
bool AmidalaConfig::cfg_goslow(const char *cmd) {
  return boolparam(cmd, "goslow=", fController->params.goslow);
}
bool AmidalaConfig::cfg_domech6(const char *cmd) {
  return boolparam(cmd, "domech6=", fController->params.domech6);
}

// ---- XBee addresses -----------------------------------------------------

bool AmidalaConfig::cfg_xbr(const char *cmd) {
  if (!startswith(cmd, "xbr="))
    return false;
  fController->params.xbr = strtoul(cmd, NULL, 16);
  return true;
}

bool AmidalaConfig::cfg_xbl(const char *cmd) {
  if (!startswith(cmd, "xbl="))
    return false;
  fController->params.xbl = strtoul(cmd, NULL, 16);
  return true;
}

// ---- Alt/mute buttons, double-press, BT, WCB, WiFi --------------------------

bool AmidalaConfig::cfg_altbtn(const char *cmd) {
  return intparam(cmd, "altbtn=", fController->params.altbtn, 0, 9);
}
bool AmidalaConfig::cfg_altdomestick(const char *cmd) {
  return intparam(cmd, "altdomestick=", fController->params.altdomestick, 0, 1);
}
bool AmidalaConfig::cfg_mutebutton(const char *cmd) {
  return intparam(cmd, "mutebutton=", fController->params.mutebutton, 0, 9);
}
bool AmidalaConfig::cfg_dbtimeout(const char *cmd) {
  return intparam(cmd, "dbtimeout=", fController->params.dbtimeout, 0, 5000);
}
bool AmidalaConfig::cfg_gesturetimeout(const char *cmd) {
  return intparam(cmd, "gesturetimeout=", fController->params.gesturetimeout, 200, 5000);
}
bool AmidalaConfig::cfg_auxserial3(const char *cmd) {
  return boolparam(cmd, "auxserial3=", fController->params.auxserial3);
}
bool AmidalaConfig::cfg_btcontrolleron(const char *cmd) {
  return boolparam(cmd, "btcontrolleron=", fController->params.btcontrolleron);
}
bool AmidalaConfig::cfg_btaddr(const char *cmd) {
  if (!startswith(cmd, "btaddr="))
    return false;
  AmidalaParameters &params = fController->params;
  strncpy(params.btaddr, cmd, sizeof(params.btaddr) - 1);
  params.btaddr[sizeof(params.btaddr) - 1] = '\0';
  return true;
}
bool AmidalaConfig::cfg_wcbenable(const char *cmd) {
  return boolparam(cmd, "wcbenable=", fController->params.wcbenable);
}
bool AmidalaConfig::cfg_wcboct2(const char *cmd) {
  if (!startswith(cmd, "wcboct2="))
    return false;
  fController->params.wcboct2 = (uint8_t)strtoul(cmd, NULL, 16);
  return true;
}
bool AmidalaConfig::cfg_wcboct3(const char *cmd) {
  if (!startswith(cmd, "wcboct3="))
    return false;
  fController->params.wcboct3 = (uint8_t)strtoul(cmd, NULL, 16);
  return true;
}
bool AmidalaConfig::cfg_wcbpassword(const char *cmd) {
  if (!startswith(cmd, "wcbpassword="))
    return false;
  AmidalaParameters &params = fController->params;
  strncpy(params.wcbpassword, cmd, sizeof(params.wcbpassword) - 1);
  params.wcbpassword[sizeof(params.wcbpassword) - 1] = '\0';
  return true;
}
bool AmidalaConfig::cfg_wcbquantity(const char *cmd) {
  return intparam(cmd, "wcbquantity=", fController->params.wcbquantity, 0, 19);
}
bool AmidalaConfig::cfg_wcbid(const char *cmd) {
  return intparam(cmd, "wcbid=", fController->params.wcbid, 0, 20);
}
bool AmidalaConfig::cfg_outboundserial(const char *cmd) {
  return intparam(cmd, "outboundserial=", fController->params.outboundserial, 0, 1);
}
bool AmidalaConfig::cfg_wifion(const char *cmd) {
  return boolparam(cmd, "wifion=", fController->params.wifion);
}
bool AmidalaConfig::cfg_wifissid(const char *cmd) {
  if (!startswith(cmd, "wifissid="))
    return false;
  AmidalaParameters &params = fController->params;
  strncpy(params.wifiSSID, cmd, sizeof(params.wifiSSID) - 1);
  params.wifiSSID[sizeof(params.wifiSSID) - 1] = '\0';
  return true;
}
bool AmidalaConfig::cfg_wifipassword(const char *cmd) {
  if (!startswith(cmd, "wifipassword="))
    return false;
  AmidalaParameters &params = fController->params;
  strncpy(params.wifiPassword, cmd, sizeof(params.wifiPassword) - 1);
  params.wifiPassword[sizeof(params.wifiPassword) - 1] = '\0';
  return true;
}
bool AmidalaConfig::cfg_wifichannel(const char *cmd) {
  return intparam(cmd, "wifichannel=", fController->params.wifichannel, 1, 13);
}

// ---- Reassignable GPIO pin roles (issue #133) -------------------------------

bool AmidalaConfig::cfg_pin1role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin1role=", 1, fOutput);
}
bool AmidalaConfig::cfg_pin2role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin2role=", 2, fOutput);
}
bool AmidalaConfig::cfg_pin3role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin3role=", 3, fOutput);
}
bool AmidalaConfig::cfg_pin4role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin4role=", 4, fOutput);
}
bool AmidalaConfig::cfg_pin5role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin5role=", 5, fOutput);
}
bool AmidalaConfig::cfg_pin6role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin6role=", 6, fOutput);
}
bool AmidalaConfig::cfg_pin39role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin39role=", 39, fOutput);
}
bool AmidalaConfig::cfg_pin40role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin40role=", 40, fOutput);
}
bool AmidalaConfig::cfg_pin41role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin41role=", 41, fOutput);
}
bool AmidalaConfig::cfg_pin42role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin42role=", 42, fOutput);
}
bool AmidalaConfig::cfg_pin47role(const char *cmd) {
  return applyPinRoleParam(fController->params, cmd, "pin47role=", 47, fOutput);
}

// ---- Reassignable dome/drive serial ports (issue #147) ----------------------

bool AmidalaConfig::cfg_domeserialport(const char *cmd) {
  return applySerialPortParam(fController->params, cmd, "domeserialport=", SerialConsumer::kDome, fOutput);
}
bool AmidalaConfig::cfg_driveserialport(const char *cmd) {
  return applySerialPortParam(fController->params, cmd, "driveserialport=", SerialConsumer::kDrive, fOutput);
}

// ---- Reboot -------------------------------------------------------------

bool AmidalaConfig::cfg_reboot(const char *cmd) {
  if (strcmp(cmd, "reboot") != 0)
    return false;
  void (*resetArduino)() = NULL;
  resetArduino();
  return true; // unreachable -- resetArduino() never returns
}

// ---- Dome/RoboClaw settings (formerly processDomeConfig()) -----------------

bool AmidalaConfig::cfg_domeimu(const char *cmd) {
  return boolparam(cmd, "domeimu=", fController->params.domeimu);
}

bool AmidalaConfig::cfg_domeflip(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!boolparam(cmd, "domeflip=", params.domeflip))
    return false;
  // domeDrive is still null when config.txt is parsed at boot (it's
  // constructed later in AmidalaController::setup(), which re-applies
  // params.domeflip to it once it exists) -- only a live post-boot change
  // needs to apply it here immediately.
  if (fController->fDomeDrive)
    fController->fDomeDrive->setInverted(params.domeflip);
  return true;
}

bool AmidalaConfig::cfg_domespeed(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!intparam(cmd, "domespeed=", params.domespeed, 0, 100))
    return false;
  // setMaxSpeed()/setMaxSpeedPct() expect a 0.0-1.0 fraction, not the raw
  // 0-100 UI value -- matches what boot-time setup() already does
  // (src/controller.cpp). setMaxSpeedPct() is a RoboClaw-only convenience
  // wrapper; other dome-drive variants only have the base setMaxSpeed().
  // domeDrive is null when config.txt is parsed at boot -- setup() applies
  // params.domespeed to the drive once it's constructed, so skip here.
  if (fController->fDomeDrive) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    static_cast<DomeDriveRoboClaw*>(fController->fDomeDrive)->setMaxSpeedPct(float(params.domespeed) / 100.0f);
#else
    fController->fDomeDrive->setMaxSpeed(float(params.domespeed) / 100.0f);
#endif
  }
  return true;
}

bool AmidalaConfig::cfg_domepos(const char *cmd) {
  int32_t sintarg;
  if (!sintparam(cmd, "domepos=", sintarg))
    return false;
#ifdef RDH_SERIAL
  Serial.print("NEWPOS: ");
  Serial.println(sintarg);
  fController->fAutoDome.setAbsolutePosition(sintarg);
  return true;
#else
  return false;
#endif
}

bool AmidalaConfig::cfg_domepos2(const char *cmd) {
  int32_t sintarg, sintarg2;
  if (!sintparam2(cmd, "domepos=", sintarg, sintarg2))
    return false;
#ifdef RDH_SERIAL
  Serial.print("NEWPOS: ");
  Serial.println(sintarg);
  fController->fAutoDome.setAbsolutePosition(sintarg, sintarg2);
  return true;
#else
  return false;
#endif
}

bool AmidalaConfig::cfg_domerpos(const char *cmd) {
  int32_t sintarg;
  if (!sintparam(cmd, "domerpos=", sintarg))
    return false;
#ifdef RDH_SERIAL
  Serial.print("NEWPOS: ");
  Serial.println(sintarg);
  fController->fAutoDome.setRelativePosition(sintarg);
  return true;
#else
  return false;
#endif
}

bool AmidalaConfig::cfg_domerpos2(const char *cmd) {
  int32_t sintarg, sintarg2;
  if (!sintparam2(cmd, "domerpos=", sintarg, sintarg2))
    return false;
#ifdef RDH_SERIAL
  Serial.print("NEWPOS: ");
  Serial.println(sintarg);
  fController->fAutoDome.setRelativePosition(sintarg, sintarg2);
  return true;
#else
  return false;
#endif
}

bool AmidalaConfig::cfg_domehome(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!intparam(cmd, "domehome=", params.domehome, 0, 360))
    return false;
#ifdef RDH_SERIAL
  fController->fAutoDome.setDomeHomePosition(params.domehome);
#endif
  return true;
}

bool AmidalaConfig::cfg_domemode(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!intparam(cmd, "domemode=", params.domemode, 1, 5))
    return false;
#ifdef RDH_SERIAL
  fController->fAutoDome.setDomeDefaultMode(params.domemode);
#endif
  return true;
}

bool AmidalaConfig::cfg_domeseekr(const char *cmd) {
  if (!intparam(cmd, "domeseekr=", fController->params.domeseekr, 1, 180))
    return false;
  applyDomePositionParams();
  return true;
}
bool AmidalaConfig::cfg_domeseekl(const char *cmd) {
  if (!intparam(cmd, "domeseekl=", fController->params.domeseekl, 1, 180))
    return false;
  applyDomePositionParams();
  return true;
}
bool AmidalaConfig::cfg_domefudge(const char *cmd) {
  if (!intparam(cmd, "domefudge=", fController->params.domefudge, 1, 45))
    return false;
  applyDomePositionParams();
  return true;
}
bool AmidalaConfig::cfg_domespeedhome(const char *cmd) {
  if (!intparam(cmd, "domespeedhome=", fController->params.domespeedhome, 1, 100))
    return false;
  applyDomePositionParams();
  return true;
}
bool AmidalaConfig::cfg_domespeedseek(const char *cmd) {
  if (!intparam(cmd, "domespeedseek=", fController->params.domespeedseek, 1, 100))
    return false;
  applyDomePositionParams();
  return true;
}
bool AmidalaConfig::cfg_domespeedmin(const char *cmd) {
  if (!intparam(cmd, "domespeedmin=", fController->params.domespeedmin, 0, 30))
    return false;
  applyDomePositionParams();
  return true;
}
bool AmidalaConfig::cfg_domedecelzone(const char *cmd) {
  if (!intparam(cmd, "domedecelzone=", fController->params.domedecelzone, 5, 90))
    return false;
  applyDomePositionParams();
  return true;
}

// ---- RoboClaw dome drive parameters (parsed regardless of active dome
//      drive so config.txt is portable between builds) ----------------------
// domeDrive is null when config.txt is parsed at boot (constructed later in
// AmidalaController::setup(), which re-applies each of these params to it
// once it exists) -- only a live post-boot change needs to apply here.

bool AmidalaConfig::cfg_domercaddr(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!intparam(cmd, "domercaddr=", params.domercaddr, 128, 135))
    return false;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fController->fDomeDrive)
    static_cast<DomeDriveRoboClaw*>(fController->fDomeDrive)->setAddress(params.domercaddr);
#endif
  return true;
}

bool AmidalaConfig::cfg_domercchan(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!intparam(cmd, "domercchan=", params.domercchan, 1, 2))
    return false;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fController->fDomeDrive)
    static_cast<DomeDriveRoboClaw*>(fController->fDomeDrive)->setChannel(params.domercchan);
#endif
  return true;
}

bool AmidalaConfig::cfg_domercqpps(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!intparam(cmd, "domercqpps=", params.domercqpps, 1, 65535))
    return false;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fController->fDomeDrive)
    static_cast<DomeDriveRoboClaw*>(fController->fDomeDrive)->setQPPS(params.domercqpps);
#endif
  return true;
}

bool AmidalaConfig::cfg_domefront(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!intparam(cmd, "domefront=", params.domefront, 0, 359))
    return false;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fController->fDomeDrive)
    static_cast<DomeDriveRoboClaw*>(fController->fDomeDrive)->setFrontOffset(params.domefront);
#endif
  return true;
}

bool AmidalaConfig::cfg_domestall(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!intparam(cmd, "domestall=", params.domestall, 100, 5000))
    return false;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fController->fDomeDrive)
    static_cast<DomeDriveRoboClaw*>(fController->fDomeDrive)->setStallTimeout(params.domestall);
#endif
  return true;
}

bool AmidalaConfig::cfg_domeerrlog(const char *cmd) {
  AmidalaParameters &params = fController->params;
  if (!boolparam(cmd, "domeerrlog=", params.domeerrlog))
    return false;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  if (fController->fDomeDrive)
    static_cast<DomeDriveRoboClaw*>(fController->fDomeDrive)->setErrorLogging(params.domeerrlog);
#endif
  return true;
}

// ---- Dispatch table -----------------------------------------------------

const AmidalaConfig::ConfigHandler AmidalaConfig::kConfigHandlers[] = {
    &AmidalaConfig::cfg_sb,
    &AmidalaConfig::cfg_s,
    &AmidalaConfig::cfg_b,
    &AmidalaConfig::cfg_lb,
    &AmidalaConfig::cfg_ab,
    &AmidalaConfig::cfg_db,
    &AmidalaConfig::cfg_sstr,
    &AmidalaConfig::cfg_fav,
    &AmidalaConfig::cfg_hidden,
    &AmidalaConfig::cfg_cat,
    &AmidalaConfig::cfg_estopstr,
    &AmidalaConfig::cfg_resumestr,
    &AmidalaConfig::cfg_gesture,
    &AmidalaConfig::cfg_audiohw,
    &AmidalaConfig::cfg_acktype,
    &AmidalaConfig::cfg_b9,
    &AmidalaConfig::cfg_volume,
    &AmidalaConfig::cfg_volumeChA,
    &AmidalaConfig::cfg_volumeChB,
    &AmidalaConfig::cfg_volumewheel,
    &AmidalaConfig::cfg_altvolumewheel,
    &AmidalaConfig::cfg_startupem,
    &AmidalaConfig::cfg_startuplvl,
    &AmidalaConfig::cfg_ackem,
    &AmidalaConfig::cfg_acklvl,
    &AmidalaConfig::cfg_mindelay,
    &AmidalaConfig::cfg_maxdelay,
    &AmidalaConfig::cfg_rvrmin,
    &AmidalaConfig::cfg_rvrmax,
    &AmidalaConfig::cfg_rvlmin,
    &AmidalaConfig::cfg_rvlmax,
    &AmidalaConfig::cfg_minpulse,
    &AmidalaConfig::cfg_maxpulse,
    &AmidalaConfig::cfg_rcchn,
    &AmidalaConfig::cfg_rcd,
    &AmidalaConfig::cfg_rcj,
    &AmidalaConfig::cfg_myi2c,
    &AmidalaConfig::cfg_serialbaud,
    &AmidalaConfig::cfg_serialdelim,
    &AmidalaConfig::cfg_serialeol,
    &AmidalaConfig::cfg_fst,
    &AmidalaConfig::cfg_j1adjv,
    &AmidalaConfig::cfg_j1adjh,
    &AmidalaConfig::cfg_rnd,
    &AmidalaConfig::cfg_ackgest,
    &AmidalaConfig::cfg_slowgest,
    &AmidalaConfig::cfg_domegest,
    &AmidalaConfig::cfg_startup,
    &AmidalaConfig::cfg_rndon,
    &AmidalaConfig::cfg_ackon,
    &AmidalaConfig::cfg_mix12,
    &AmidalaConfig::cfg_autocorrect,
    &AmidalaConfig::cfg_goslow,
    &AmidalaConfig::cfg_domech6,
    &AmidalaConfig::cfg_xbr,
    &AmidalaConfig::cfg_xbl,
    &AmidalaConfig::cfg_domeimu,
    &AmidalaConfig::cfg_domeflip,
    &AmidalaConfig::cfg_domespeed,
    &AmidalaConfig::cfg_domepos,
    &AmidalaConfig::cfg_domepos2,
    &AmidalaConfig::cfg_domerpos,
    &AmidalaConfig::cfg_domerpos2,
    &AmidalaConfig::cfg_domehome,
    &AmidalaConfig::cfg_domemode,
    &AmidalaConfig::cfg_domeseekr,
    &AmidalaConfig::cfg_domeseekl,
    &AmidalaConfig::cfg_domefudge,
    &AmidalaConfig::cfg_domespeedhome,
    &AmidalaConfig::cfg_domespeedseek,
    &AmidalaConfig::cfg_domespeedmin,
    &AmidalaConfig::cfg_domedecelzone,
    &AmidalaConfig::cfg_domercaddr,
    &AmidalaConfig::cfg_domercchan,
    &AmidalaConfig::cfg_domercqpps,
    &AmidalaConfig::cfg_domefront,
    &AmidalaConfig::cfg_domestall,
    &AmidalaConfig::cfg_domeerrlog,
    &AmidalaConfig::cfg_altbtn,
    &AmidalaConfig::cfg_altdomestick,
    &AmidalaConfig::cfg_mutebutton,
    &AmidalaConfig::cfg_dbtimeout,
    &AmidalaConfig::cfg_gesturetimeout,
    &AmidalaConfig::cfg_auxserial3,
    &AmidalaConfig::cfg_btcontrolleron,
    &AmidalaConfig::cfg_btaddr,
    &AmidalaConfig::cfg_wcbenable,
    &AmidalaConfig::cfg_wcboct2,
    &AmidalaConfig::cfg_wcboct3,
    &AmidalaConfig::cfg_wcbpassword,
    &AmidalaConfig::cfg_wcbquantity,
    &AmidalaConfig::cfg_wcbid,
    &AmidalaConfig::cfg_outboundserial,
    &AmidalaConfig::cfg_wifion,
    &AmidalaConfig::cfg_wifissid,
    &AmidalaConfig::cfg_wifipassword,
    &AmidalaConfig::cfg_wifichannel,
    &AmidalaConfig::cfg_pin1role,
    &AmidalaConfig::cfg_pin2role,
    &AmidalaConfig::cfg_pin3role,
    &AmidalaConfig::cfg_pin4role,
    &AmidalaConfig::cfg_pin5role,
    &AmidalaConfig::cfg_pin6role,
    &AmidalaConfig::cfg_pin39role,
    &AmidalaConfig::cfg_pin40role,
    &AmidalaConfig::cfg_pin41role,
    &AmidalaConfig::cfg_pin42role,
    &AmidalaConfig::cfg_pin47role,
    &AmidalaConfig::cfg_domeserialport,
    &AmidalaConfig::cfg_driveserialport,
    &AmidalaConfig::cfg_reboot,
};

bool AmidalaConfig::processConfig(const char *cmd) {
  for (auto handler : kConfigHandlers) {
    if ((this->*handler)(cmd))
      return true;
  }
  return false;
}
