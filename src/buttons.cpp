#include "controller.h"

static int findSerialStringById(AmidalaParameters &params, uint16_t id) {
  if (id == 0) return -1;
  for (int i = 0; i < (int)params.serialcount; i++) {
    if (params.Str[i].id == id) return i;
  }
  return -1;
}

void AmidalaConsole::process(ButtonAction &button) {
  AmidalaParameters &params = fController->params;
  switch (button.action) {
  case button.kSound:
    DEBUG_PRINT("PLAY SOUND BANK ");
    DEBUG_PRINT(button.sound.soundbank);
    DEBUG_PRINT(" # ");
    DEBUG_PRINTLN(button.sound.sound);
    if (button.sound.sound != 0) {
      fController->fAudio.playSound(button.sound.soundbank, button.sound.sound);
    } else {
      fController->fAudio.playSound(button.sound.soundbank);
    }
    break;
  case button.kI2CCmd:
    DEBUG_PRINT("I2C CMD addr=");
    DEBUG_PRINT(button.i2ccmd.target);
    DEBUG_PRINT(" cmd=");
    DEBUG_PRINTLN(button.i2ccmd.cmd);
    sendI2CCmd(button.i2ccmd.target, button.i2ccmd.cmd);
    break;
  case button.kI2CStr: {
    int idx = findSerialStringById(params, button.serialid);
    if (idx >= 0) {
      const char *str = params.Str[idx].str;
      DEBUG_PRINT("I2C STR addr=");
      DEBUG_PRINT(button.i2cstr.target);
      DEBUG_PRINT(" str=");
      DEBUG_PRINTLN(str);
      sendI2CStr(button.i2cstr.target, str);
    }
    break;
  }
  case button.kSerialStr: {
    int idx = findSerialStringById(params, button.serialid);
    if (idx >= 0) {
      DEBUG_PRINT("SERIAL: ");
      DEBUG_PRINTLN(params.Str[idx].str);
      fController->sendSerialString(params.Str[idx].str);
    }
    break;
  }
  case button.kHCREmote:
    fController->fAudio.playEmote(button.emote.emotion, button.emote.level);
    break;
  case button.kHCRMuse:
    fController->fAudio.toggleMuse();
    break;
  case button.kDomeCmd:
    fController->processDomeCmd(button.dome.subcmd, button.dome.arg);
    break;
  }
  // Play ack emote for non-audio actions when ackon is enabled
  if (button.action != ButtonAction::kNone &&
      button.action != ButtonAction::kHCREmote &&
      button.action != ButtonAction::kHCRMuse) {
    fController->fAudio.playAck();
  }
  // Side-effect serial string (only for actions where serialid is not already the primary ref)
  if (button.action != ButtonAction::kSerialStr &&
      button.action != ButtonAction::kI2CStr &&
      button.serialid != 0) {
    int idx = findSerialStringById(params, button.serialid);
    if (idx >= 0) {
      DEBUG_PRINT("SERIAL (side-effect): ");
      DEBUG_PRINTLN(params.Str[idx].str);
      fController->sendSerialString(params.Str[idx].str);
    }
  }
}

void AmidalaConsole::processGesture(const char *gesture) {
  AmidalaParameters &params = fController->params;
  print("GESTURE: ");
  println(gesture);
  for (unsigned i = 0; i < params.getGestureCount(); i++) {
    if (params.G[i].gesture.matches(gesture, params.autocorrect)) {
      process(params.G[i].action);
    }
  }
}

void AmidalaConsole::processButtonLayer(unsigned num, const char *label,
                                        ButtonAction *actions) {
  print(label);
  println(num);
  AmidalaParameters &params = fController->params;
  if (num >= 1 && num <= params.getButtonCount())
    process(actions[num - 1]);
}

void AmidalaConsole::processButton(unsigned num) {
  processButtonLayer(num, "Processing Button ", fController->params.B);
}

void AmidalaConsole::processLongButton(unsigned num) {
  processButtonLayer(num, "Processing Long Button ", fController->params.LB);
}

void AmidalaConsole::processAltButton(unsigned num) {
  processButtonLayer(num, "Processing Alt Button ", fController->params.AB);
}

void AmidalaConsole::processDoubleButton(unsigned num) {
  processButtonLayer(num, "Processing Double Button ", fController->params.DB);
}
