// snips_bindings_apply.cpp
// AmidalaController's side of the Snips analog bindings: turns the pure
// SnipsBindings::Outputs (include/snips_bindings.h) into drive/dome speed,
// dome position, servo moves and virtual button presses.

#include "controller.h"
#include "button_dispatch.h"

// See src/servo.cpp: the base-class reference avoids pulling in
// ServoDispatchDirect.h's ISR handlers a second time.
extern ServoDispatch& servoDispatch;

namespace {

// goToAngle() is a RoboClaw command, so don't resend it for less than this
// change, or more often than this.
constexpr int kDomeStepDeg = 2;
constexpr uint32_t kDomeCommandMs = 100;

float effectiveScale(const float sides[2]) {
  float best = -1;
  for (int i = 0; i < 2; i++) {
    if (sides[i] > best) best = sides[i];
  }
  return best < 0 ? 1.0f : best;
}

float wrapDegrees(float deg) {
  while (deg >= 360.0f) deg -= 360.0f;
  while (deg < 0.0f) deg += 360.0f;
  return deg;
}

}  // namespace

void AmidalaController::applySnipsBindings(uint8_t side, const Snips::UplinkPacket &pkt) {
  const uint32_t now = millis();
  SnipsBindings::Outputs out;
  fBindingEval.evaluate(side, pkt, now, params.sbind, &out);

  fSideDriveScale[side] = out.driveScale;
  fSideDomeScale[side] = out.domeScale;
  applyBindingScales();

  if (out.hasDomeAngle) steerDomeTo(out.domeAngle, now);
  if (out.domeTurn != 0) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    // A rotation-rate binding turns the dome by as much as the controller
    // turned. Start from wherever the dome is now if it's been a while.
    if (!fBindingHeadingValid || now - fBindingHeadingLastMs > 1000) {
      fBindingHeading = fDomeDrive ? fDomeDrive->getCurrentDegrees() : 0;
      fBindingHeadingValid = true;
    }
    // Controller rates are + counterclockwise seen from above; goToAngle()
    // is + clockwise.
    fBindingHeading = wrapDegrees(fBindingHeading - out.domeTurn);
    steerDomeTo(fBindingHeading, now);
#endif
  }

  for (int i = 0; i < out.servoCount; i++) {
    const SnipsBindings::ServoMove &m = out.servos[i];
    if (m.channel >= 1 && m.channel <= params.getServoCount())
      servoDispatch.moveTo(m.channel - 1, m.pos);
  }

  for (int i = 0; i < out.buttonCount; i++) {
    const SnipsBindings::ButtonEvent &b = out.buttons[i];
    dispatchButtonPress(*this, b.num, b.up, b.longUp, params.altbtn, isAltHeld());
  }
}

void AmidalaController::snipsSideDisconnected(uint8_t side) {
  fBindingEval.reset(side, params.sbind);
  // A bound accelerator or dome speed lets go: the droid shouldn't keep
  // moving at the last trigger position.
  if (fSideDriveScale[side] >= 0) fSideDriveScale[side] = 0;
  if (fSideDomeScale[side] >= 0) fSideDomeScale[side] = 0;
  applyBindingScales();
  fBindingHeadingValid = false;
}

void AmidalaController::clearSnipsBindingScales() {
  for (int i = 0; i < 2; i++) {
    fSideDriveScale[i] = -1;
    fSideDomeScale[i] = -1;
  }
  applyBindingScales();
}

void AmidalaController::applyBindingScales() {
  const float drive = effectiveScale(fSideDriveScale);
  if (drive != fDriveAccelScale) {
    fDriveAccelScale = drive;
    applyDriveSpeedPct();
  }
  const float dome = effectiveScale(fSideDomeScale);
  if (dome != fDomeAccelScale) {
    fDomeAccelScale = dome;
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    if (fDomeDrive) fDomeDrive->setMaxSpeedPct(float(params.domespeed) / 100.0f * dome);
#elif defined(DOME_DRIVE)
    if (fDomeDrive) fDomeDrive->setMaxSpeed(DOME_MAXIMUM_SPEED * dome);
#endif
  }
}

void AmidalaController::steerDomeTo(float degrees, uint32_t now) {
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  fBindingHeadingLastMs = now;
  if (!fDomeDrive || !fDomeDrive->isHomed() || !fDomeDrive->isCalibrated()) return;
  const int target = (int)wrapDegrees(degrees + 0.5f) % 360;
  if (fBindingHeadingSent >= 0) {
    int diff = abs(target - fBindingHeadingSent);
    if (diff > 180) diff = 360 - diff;
    if (diff < kDomeStepDeg || now - fBindingHeadingSentMs < kDomeCommandMs) return;
  }
  fDomeDrive->goToAngle(target);
  fBindingHeadingSent = target;
  fBindingHeadingSentMs = now;
#else
  // Only the RoboClaw dome drive knows where the dome is pointing.
  (void)degrees;
  (void)now;
#endif
}
