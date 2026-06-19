#include "controller.h"

void AmidalaConfig::init(AmidalaController *controller) {
  fController = controller;
  fOutput = &controller->fConsole;
}

void AmidalaConfig::applyDomePositionParams() {
#ifdef DOME_DRIVE
  AmidalaParameters &params = fController->params;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  fController->fDomeDrive.applyDomePositionParams(
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
    fOutput->print(F("WiFi AP: "));
    fOutput->println(params.wifion ? F("On") : F("Off"));
    fOutput->print(F("WiFi SSID: "));
    fOutput->println(params.wifiSSID);
    fOutput->print(F("WiFi Password: "));
    fOutput->println(params.wifiPassword);
    for (unsigned i = 0; i < params.getSerialStringCount(); i++) {
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

bool AmidalaConfig::processConfig(const char *cmd) {
  bool boolarg;
  int32_t sintarg;
  int32_t sintarg2;
  AmidalaParameters &params = fController->params;
#ifdef RDH_SERIAL
  RDHSerial &autoDome = fController->fAutoDome;
#endif
  auto *domeDrive = &fController->fDomeDrive;
  if (startswith(cmd, "sb=")) {
    if (params.sbcount < params.getSoundBankCount()) {
      AmidalaParameters::SoundBank *sb =
          &params.SB[params.sbcount];
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
  } else if (startswith(cmd, "s=")) {
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
  } else if (startswith(cmd, "b=")) {
    uint8_t argcount;
    uint8_t args[5];
    memset(args, '\0', sizeof(args));
    ButtonAction *b = params.B;
    if (numberparams(cmd, argcount, args, sizeof(args)) && argcount >= 2 &&
        args[0] >= 1 && args[0] <= params.getButtonCount()) {
      b += args[0] - 1;
      memset(b, '\0', sizeof(*b));
      b->action = args[1];
      b->sound.serialstr = 0;
      switch (args[1]) {
      case ButtonAction::kSound:
        b->sound.soundbank = max(1, (int)min(args[2], params.sbcount));
        b->sound.sound = (argcount >= 4) ? args[3] : 0;
        b->sound.sound =
            min(b->sound.sound, params.SB[b->sound.soundbank].numfiles);
        b->action = args[1];
        break;
      case ButtonAction::kServo:
        b->servo.num = max(1, min((int)args[2], 8));
        b->servo.pos = (argcount >= 4) ? args[3] : 0;
        b->servo.pos = min(max((int)b->servo.pos, 0), 180);
        b->action = args[1];
        break;
      case ButtonAction::kDigitalOut:
        b->dout.num = max(1, min((int)args[2], 8));
        b->dout.state = (argcount >= 4) ? args[3] : 0;
        b->dout.state = min(2, (int)b->dout.state);
        b->action = args[1];
        break;
      case ButtonAction::kI2CCmd:
        b->i2ccmd.target = min((int)args[2], 100);
        b->i2ccmd.cmd = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      case ButtonAction::kSerialStr:
        b->serial.serialstr = min((int)args[2], MAX_SERIAL_STRINGS);
        DEBUG_PRINT("BUTTON SERIAL #");
        DEBUG_PRINTLN(b->serial.serialstr);
        if (b->action == 0)
          b->action = args[1];
        break;
      case ButtonAction::kI2CStr:
        b->i2cstr.target = min((int)args[2], 100);
        b->i2cstr.cmd = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      case ButtonAction::kHCREmote:
        b->emote.emotion = min(args[2], (uint8_t)4);
        b->emote.level = (argcount >= 4) ? min(args[3], (uint8_t)1) : 0;
        b->action = args[1];
        break;
      case ButtonAction::kHCRMuse:
        b->action = args[1];
        break;
      case ButtonAction::kDomeCmd:
        b->dome.subcmd = args[2];
        b->dome.arg    = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      default:
        b->action = 0;
        break;
      }
      if (b->action != ButtonAction::kSerialStr &&
          b->action != ButtonAction::kHCREmote &&
          b->action != ButtonAction::kHCRMuse && argcount >= 4)
        b->sound.serialstr = (argcount >= 5) ? args[4] : 0;
      return true;
    }
    return false;
  } else if (startswith(cmd, "lb=")) {
    uint8_t argcount;
    uint8_t args[5];
    memset(args, '\0', sizeof(args));
    ButtonAction *b = params.LB;
    if (numberparams(cmd, argcount, args, sizeof(args)) && argcount >= 2 &&
        args[0] >= 1 && args[0] <= params.getButtonCount()) {
      b += args[0] - 1;
      memset(b, '\0', sizeof(*b));
      b->action = args[1];
      b->sound.serialstr = 0;
      switch (args[1]) {
      case ButtonAction::kSound:
        b->sound.soundbank = max(1, (int)min(args[2], params.sbcount));
        b->sound.sound = (argcount >= 4) ? args[3] : 0;
        b->sound.sound =
            min(b->sound.sound, params.SB[b->sound.soundbank].numfiles);
        b->action = args[1];
        break;
      case ButtonAction::kServo:
        b->servo.num = max(1, min((int)args[2], 8));
        b->servo.pos = (argcount >= 4) ? args[3] : 0;
        b->servo.pos = min(max((int)b->servo.pos, 0), 180);
        b->action = args[1];
        break;
      case ButtonAction::kDigitalOut:
        b->dout.num = max(1, min((int)args[2], 8));
        b->dout.state = (argcount >= 4) ? args[3] : 0;
        b->dout.state = min(2, (int)b->dout.state);
        b->action = args[1];
        break;
      case ButtonAction::kI2CCmd:
        b->i2ccmd.target = min((int)args[2], 100);
        b->i2ccmd.cmd = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      case ButtonAction::kSerialStr:
        b->serial.serialstr = min((int)args[2], MAX_SERIAL_STRINGS);
        if (b->action == 0)
          b->action = args[1];
        break;
      case ButtonAction::kI2CStr:
        b->i2cstr.target = min((int)args[2], 100);
        b->i2cstr.cmd = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      case ButtonAction::kHCREmote:
        b->emote.emotion = min(args[2], (uint8_t)4);
        b->emote.level = (argcount >= 4) ? min(args[3], (uint8_t)1) : 0;
        b->action = args[1];
        break;
      case ButtonAction::kHCRMuse:
        b->action = args[1];
        break;
      case ButtonAction::kDomeCmd:
        b->dome.subcmd = args[2];
        b->dome.arg    = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      default:
        b->action = 0;
        break;
      }
      if (b->action != ButtonAction::kSerialStr &&
          b->action != ButtonAction::kHCREmote &&
          b->action != ButtonAction::kHCRMuse && argcount >= 4)
        b->sound.serialstr = args[3];
      return true;
    }
    return false;
  } else if (startswith(cmd, "ab=")) {
    uint8_t argcount;
    uint8_t args[5];
    memset(args, '\0', sizeof(args));
    ButtonAction *b = params.AB;
    if (numberparams(cmd, argcount, args, sizeof(args)) && argcount >= 2 &&
        args[0] >= 1 && args[0] <= params.getButtonCount()) {
      b += args[0] - 1;
      memset(b, '\0', sizeof(*b));
      b->action = args[1];
      b->sound.serialstr = 0;
      switch (args[1]) {
      case ButtonAction::kSound:
        b->sound.soundbank = max(1, (int)min(args[2], params.sbcount));
        b->sound.sound = (argcount >= 4) ? args[3] : 0;
        b->sound.sound =
            min(b->sound.sound, params.SB[b->sound.soundbank].numfiles);
        b->action = args[1];
        break;
      case ButtonAction::kServo:
        b->servo.num = max(1, min((int)args[2], 8));
        b->servo.pos = (argcount >= 4) ? args[3] : 0;
        b->servo.pos = min(max((int)b->servo.pos, 0), 180);
        b->action = args[1];
        break;
      case ButtonAction::kDigitalOut:
        b->dout.num = max(1, min((int)args[2], 8));
        b->dout.state = (argcount >= 4) ? args[3] : 0;
        b->dout.state = min(2, (int)b->dout.state);
        b->action = args[1];
        break;
      case ButtonAction::kI2CCmd:
        b->i2ccmd.target = min((int)args[2], 100);
        b->i2ccmd.cmd = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      case ButtonAction::kSerialStr:
        b->serial.serialstr = min((int)args[2], MAX_SERIAL_STRINGS);
        if (b->action == 0)
          b->action = args[1];
        break;
      case ButtonAction::kI2CStr:
        b->i2cstr.target = min((int)args[2], 100);
        b->i2cstr.cmd = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      case ButtonAction::kHCREmote:
        b->emote.emotion = min(args[2], (uint8_t)4);
        b->emote.level = (argcount >= 4) ? min(args[3], (uint8_t)1) : 0;
        b->action = args[1];
        break;
      case ButtonAction::kHCRMuse:
        b->action = args[1];
        break;
      case ButtonAction::kDomeCmd:
        b->dome.subcmd = args[2];
        b->dome.arg    = (argcount >= 4) ? args[3] : 0;
        b->action = args[1];
        break;
      default:
        b->action = 0;
        break;
      }
      if (b->action != ButtonAction::kSerialStr &&
          b->action != ButtonAction::kHCREmote &&
          b->action != ButtonAction::kHCRMuse && argcount >= 5)
        b->sound.serialstr = args[4];
      return true;
    }
    return false;
  } else if (startswith(cmd, "sstr=")) {
    SerialString *a =
        &params.Str[min((unsigned)params.serialcount, params.getSerialStringCount() - 1)];
    strncpy(a->str, cmd, sizeof(a->str) - 1);
    a->str[sizeof(a->str) - 1] = '\0';
    if (params.serialcount < params.getSerialStringCount())
      params.serialcount++;
  } else if (startswith(cmd, "g=")) {
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
      uint8_t argcount;
      uint8_t args[5];
      memset(args, '\0', sizeof(args));
      if (numberparams(cmd, argcount, args, sizeof(args)) && argcount >= 1) {
        memset(b, '\0', sizeof(*b));
        b->action = args[0];
        b->sound.serialstr = 0;
        switch (b->action = args[0]) {
        case ButtonAction::kSound:
          b->sound.soundbank = max(1, (int)min(args[1], params.sbcount));
          b->sound.sound = (argcount >= 3) ? args[2] : 0;
          b->sound.sound =
              min(b->sound.sound, params.SB[b->sound.soundbank].numfiles);
          break;
        case ButtonAction::kServo:
          b->servo.num = max(1, min((int)args[1], 8));
          b->servo.pos = (argcount >= 3) ? args[2] : 0;
          b->servo.pos = min(max((int)b->servo.pos, 0), 180);
          break;
        case ButtonAction::kDigitalOut:
          b->dout.num = max(1, min((int)args[1], 8));
          b->dout.state = (argcount >= 3) ? args[2] : 0;
          b->dout.state = min(2, (int)b->dout.state);
          break;
        case ButtonAction::kI2CCmd:
          b->i2ccmd.target = min((int)args[1], 100);
          b->i2ccmd.cmd = (argcount >= 3) ? args[2] : 0;
          break;
        case ButtonAction::kSerialStr:
          b->serial.serialstr = min((int)args[1], MAX_SERIAL_STRINGS);
          DEBUG_PRINT("GESTURE SERIAL #");
          DEBUG_PRINTLN(b->serial.serialstr);
          break;
        case ButtonAction::kI2CStr:
          b->i2cstr.target = min((int)args[1], 100);
          b->i2cstr.cmd = (argcount >= 3) ? args[2] : 0;
          break;
        case ButtonAction::kHCREmote:
          b->emote.emotion = min(args[1], (uint8_t)4);
          b->emote.level = (argcount >= 3) ? min(args[2], (uint8_t)1) : 0;
          break;
        case ButtonAction::kHCRMuse:
          break;
        case ButtonAction::kDomeCmd:
          b->dome.subcmd = args[1];
          b->dome.arg    = (argcount >= 3) ? args[2] : 0;
          break;
        default:
          b->action = 0;
          break;
        }
        if (b->action != ButtonAction::kSerialStr &&
            b->action != ButtonAction::kHCREmote &&
            b->action != ButtonAction::kHCRMuse &&
            b->action != ButtonAction::kDomeCmd && argcount >= 3)
          b->sound.serialstr = args[2];
        if (params.gcount < params.getGestureCount())
          params.gcount++;
        return true;
      }
    }
    return false;
  } else if (startswith(cmd, "audiohw=")) {
    if (startswith(cmd, "hcr"))    params.audiohw = AUDIO_HW_HCR;
    else if (startswith(cmd, "vmusic")) params.audiohw = AUDIO_HW_VMUSIC;
    return true;
  } else if (charparam(cmd, "acktype=", "gadsr", params.acktype) ||
             charparam(cmd, "b9=", "ynksdb", params.b9) ||
             intparam(cmd, "volume=", params.volume, 0, 100) ||
             intparam(cmd, "volumeChA=", params.volumeChA, 0, 100) ||
             intparam(cmd, "volumeChB=", params.volumeChB, 0, 100) ||
             intparam(cmd, "volumewheel=", params.volumewheel, 0, 4) ||
             intparam(cmd, "altvolumewheel=", params.altvolumewheel, 0, 4) ||
             intparam(cmd, "startupem=", params.startupem, 0, 4) ||
             intparam(cmd, "startuplvl=", params.startuplvl, 0, 1) ||
             intparam(cmd, "ackem=", params.ackem, 0, 4) ||
             intparam(cmd, "acklvl=", params.acklvl, 0, 1) ||
             intparam(cmd, "mindelay=", params.mindelay, 0, 1000) ||
             intparam(cmd, "maxdelay=", params.maxdelay, 0, 1000) ||
             intparam(cmd, "rvrmin=", params.rvrmin, 0, 100) ||
             intparam(cmd, "rvrmax=", params.rvrmax, 900, 1023) ||
             intparam(cmd, "rvlmin=", params.rvlmin, 0, 100) ||
             intparam(cmd, "rvlmax=", params.rvlmax, 900, 1023) ||
             intparam(cmd, "minpulse=", params.minpulse, 0, 2500) ||
             intparam(cmd, "maxpulse=", params.maxpulse, 0, 2500) ||
             intparam(cmd, "rcchn=", params.rcchn, 6, 8) ||
             intparam(cmd, "rcd=", params.rcd, 1, 50) ||
             intparam(cmd, "rcj=", params.rcj, 1, 40) ||
             intparam(cmd, "myi2c=", params.myi2c, 0, 100) ||
             intparam(cmd, "serialbaud=", params.serialbaud, 300, 115200) ||
             intparam(cmd, "serialdelim=", params.serialdelim, 0, 255) ||
             intparam(cmd, "serialeol=", params.serialeol, 0, 255) ||
             intparam(cmd, "fst=", params.fst, 1000, 3000) ||
             intparam(cmd, "j1adjv=", params.j1adjv, 0, 80) ||
             intparam(cmd, "j1adjh=", params.j1adjh, 0, 80) ||
             gestureparam(cmd, "rnd=", params.rnd) ||
             gestureparam(cmd, "ackgest=", params.ackgest) ||
             gestureparam(cmd, "slowgest=", params.slowgest) ||
             gestureparam(cmd, "domegest=", params.domegest) ||
             boolparam(cmd, "startup=", params.startup) ||
             boolparam(cmd, "rndon=", params.rndon) ||
             boolparam(cmd, "ackon=", params.ackon) ||
             boolparam(cmd, "mix12=", params.mix12) ||
             boolparam(cmd, "auto=", params.autocorrect) ||
             boolparam(cmd, "goslow=", params.goslow) ||
             boolparam(cmd, "domech6=", params.domech6)) {
    return true;
  } else if (startswith(cmd, "xbr=")) {
    params.xbr = strtoul(cmd, NULL, 16);
    return true;
  } else if (startswith(cmd, "xbl=")) {
    params.xbl = strtoul(cmd, NULL, 16);
    return true;
  } else if (boolparam(cmd, "domeimu=", boolarg)) {
    params.domeimu = boolarg;
    return true;
  } else if (boolparam(cmd, "domeflip=", params.domeflip)) {
    domeDrive->setInverted(params.domeflip);
    return true;
  } else if (intparam(cmd, "domespeed=", params.domespeed, 0, 100)) {
    domeDrive->setMaxSpeed(params.domespeed);
    return true;
  } else if (sintparam(cmd, "domepos=", sintarg)) {
#ifdef RDH_SERIAL
    Serial.print("NEWPOS: ");
    Serial.println(sintarg);
    autoDome.setAbsolutePosition(sintarg);
    return true;
#else
    return false;
#endif
  } else if (sintparam2(cmd, "domepos=", sintarg, sintarg2)) {
#ifdef RDH_SERIAL
    Serial.print("NEWPOS: ");
    Serial.println(sintarg);
    autoDome.setAbsolutePosition(sintarg, sintarg2);
    return true;
#else
    return false;
#endif
  } else if (sintparam(cmd, "domerpos=", sintarg)) {
#ifdef RDH_SERIAL
    Serial.print("NEWPOS: ");
    Serial.println(sintarg);
    autoDome.setRelativePosition(sintarg);
    return true;
#else
    return false;
#endif
  } else if (sintparam2(cmd, "domerpos=", sintarg, sintarg2)) {
#ifdef RDH_SERIAL
    Serial.print("NEWPOS: ");
    Serial.println(sintarg);
    autoDome.setRelativePosition(sintarg, sintarg2);
    return true;
#else
    return false;
#endif
  } else if (intparam(cmd, "domehome=", params.domehome, 0, 360)) {
#ifdef RDH_SERIAL
    autoDome.setDomeHomePosition(params.domehome);
    return true;
#else
    return false;
#endif
  } else if (intparam(cmd, "domemode=", params.domemode, 1, 5)) {
#ifdef RDH_SERIAL
    autoDome.setDomeDefaultMode(params.domemode);
    return true;
#else
    return false;
#endif
  } else if (intparam(cmd, "domeseekr=",    params.domeseekr,    1,   180) ||
             intparam(cmd, "domeseekl=",    params.domeseekl,    1,   180) ||
             intparam(cmd, "domefudge=",    params.domefudge,    1,    45) ||
             intparam(cmd, "domespeedhome=",params.domespeedhome,1,   100) ||
             intparam(cmd, "domespeedseek=",params.domespeedseek,1,   100) ||
             intparam(cmd, "domespeedmin=", params.domespeedmin, 0,    30) ||
             intparam(cmd, "domedecelzone=",params.domedecelzone,5,    90)) {
    applyDomePositionParams();
    return true;
  // ---- RoboClaw dome drive parameters (parsed regardless of active dome
  //      drive so config.txt is portable between builds) ---------------------
  } else if (intparam(cmd, "domercaddr=", params.domercaddr, 128, 135)) {
    return true;
  } else if (intparam(cmd, "domercchan=", params.domercchan, 1, 2)) {
    return true;
  } else if (intparam(cmd, "domercqpps=", params.domercqpps, 1, 65535)) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    static_cast<DomeDriveRoboClaw*>(domeDrive)->setQPPS(params.domercqpps);
#endif
    return true;
  } else if (intparam(cmd, "domefront=", params.domefront, 0, 359)) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    static_cast<DomeDriveRoboClaw*>(domeDrive)->setFrontOffset(params.domefront);
#endif
    return true;
  } else if (intparam(cmd, "domestall=", params.domestall, 100, 5000)) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    static_cast<DomeDriveRoboClaw*>(domeDrive)->setStallTimeout(params.domestall);
#endif
    return true;
  } else if (intparam(cmd, "altbtn=", params.altbtn, 0, 9)) {
    return true;
  } else if (intparam(cmd, "altdomestick=", params.altdomestick, 0, 1)) {
    return true;
  } else if (intparam(cmd, "mutebutton=", params.mutebutton, 0, 9)) {
    return true;
  } else if (boolparam(cmd, "wifion=", params.wifion)) {
    return true;
  } else if (startswith(cmd, "wifissid=")) {
    strncpy(params.wifiSSID, cmd, sizeof(params.wifiSSID) - 1);
    params.wifiSSID[sizeof(params.wifiSSID) - 1] = '\0';
    return true;
  } else if (startswith(cmd, "wifipassword=")) {
    strncpy(params.wifiPassword, cmd, sizeof(params.wifiPassword) - 1);
    params.wifiPassword[sizeof(params.wifiPassword) - 1] = '\0';
    return true;
  } else if (strcmp(cmd, "reboot") == 0) {
    void (*resetArduino)() = NULL;
    resetArduino();
  }
  // else if (intparam(cmd, "domespmin=", params.domespmin, 0, 100))
  // {
  // }
  // else if (intparam(cmd, "domespmax=", params.domespmin, 900, 1023))
  // {
  // }
  return false;
}
