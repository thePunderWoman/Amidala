#include "controller.h"

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
  case button.kI2CStr:
    if (button.i2cstr.cmd != 0 &&
        button.i2cstr.cmd <= params.getSerialStringCount()) {
      const char *str = params.Str[button.i2cstr.cmd - 1].str;
      DEBUG_PRINT("I2C STR addr=");
      DEBUG_PRINT(button.i2cstr.target);
      DEBUG_PRINT(" str=");
      DEBUG_PRINTLN(str);
      sendI2CStr(button.i2cstr.target, str);
    }
    break;
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
  if (button.serial.serialstr != 0 &&
      button.serial.serialstr <= params.getSerialStringCount()) {
    DEBUG_PRINT("SERIAL #");
    DEBUG_PRINTLN(params.Str[button.serial.serialstr - 1].str);
    fController->sendSerialString(params.Str[button.serial.serialstr - 1].str);
  }
}

void AmidalaConsole::processGesture(const char *gesture) {
  AmidalaParameters &params = fController->params;
  print("GESTURE: ");
  println(gesture);
  for (unsigned i = 0; i < params.getGestureCount(); i++) {
    if (params.G[i].gesture.matches(gesture)) {
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
