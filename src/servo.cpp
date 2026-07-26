#include "controller.h"
#include "config.h"

// servoDispatch is defined in src/globals.cpp as a ServoDispatch& reference to
// the concrete ServoDispatchDirect instance.  Using the base-class reference
// avoids including ServoDispatchDirect.h here, which would duplicate the ISR
// handlers defined in ServoDispatchPrivate.h.
extern ServoDispatch& servoDispatch;

void AmidalaConsole::printServoPos(uint16_t num) {
  if (servoDispatch.isActive(num)) {
    printNum(servoDispatch.currentPos(num));
  } else {
    print("----");
  }
}

void AmidalaConsole::setServo() {
  // Not supported
  println(F("Invalid"));
}

void AmidalaConfig::applyServoConfig(unsigned num, uint16_t minpulse,
                                     uint16_t maxpulse, float neutral) {
  // Pins aren't contiguous once independently reassignable (issue #133) --
  // look up the actual Servo-typed pin for this channel from params.pinRole
  // instead of computing it from SERVO1_PIN. num beyond the live servo
  // count (e.g. a channel the user hasn't traded a pin into) has no pin;
  // nthPinWithRole() returns kNoPin and setServo() disables that channel,
  // same as the "channel beyond live count" case in controller.cpp setup().
  AmidalaParameters &params = fController->params;
  uint8_t pin = nthPinWithRole(params.pinRole, PinRoleType::kServo, num);
  if (pin == kNoPin) pin = 0;
  servoDispatch.setServo(num, pin, minpulse, maxpulse,
                         neutral * maxpulse, 0);
}
