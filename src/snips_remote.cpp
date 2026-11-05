// snips_remote.cpp
// SnipsRemote method bodies that call AmidalaController -- see
// include/snips_remote.h for the class and its design rationale.

#include <stdio.h>
#include "controller.h"
#include "snips_drive.h"
#include "snips_remote.h"

void SnipsRemote::onUplinkReceived(const Snips::UplinkPacket &pkt, uint16_t senderAddr16) {
  uint32_t now = millis();
  addr16 = senderAddr16;

  if (fDriver) {
    int altbtn = fDriver->params.altbtn;
    bool altbtnInRange = altbtn > 0 && ownsButtonNumber((unsigned)altbtn) &&
                         !isGestureButton(fSide, bitIndexForButtonNumber((unsigned)altbtn));
    if (altbtnInRange) {
      unsigned altBit = bitIndexForButtonNumber((unsigned)altbtn);
      fDriver->setAltHeld((pkt.buttonMask & (1u << altBit)) != 0);
    }
    bool altHeld = fDriver->isAltHeld();

    int muteBtn = fDriver->params.mutebutton;
    bool muteBtnInRange = muteBtn > 0 && ownsButtonNumber((unsigned)muteBtn) &&
                          !isGestureButton(fSide, bitIndexForButtonNumber((unsigned)muteBtn));
    unsigned muteBit = muteBtnInRange ? bitIndexForButtonNumber((unsigned)muteBtn) : SnipsButtons::kCount;

    for (unsigned i = 0; i < SnipsButtons::kCount; i++) {
      // Goes to the dome remote below instead, as gesture start/end.
      if (isGestureButton(fSide, i)) continue;
      bool prevPressed = (fPrevButtonMask & (1u << i)) != 0;
      bool pressed = (pkt.buttonMask & (1u << i)) != 0;
      bool down = !prevPressed && pressed;
      bool up = prevPressed && !pressed;
      auto r = fLongPress[i].update(down, up, pressed, now, DEFAULT_LONG_PRESS_MS);
      unsigned num = buttonNumber(i);
      dispatchButtonPress(*fDriver, num, r.up, r.longUp, altbtn, altHeld);
      if (muteBtnInRange && i == muteBit && r.up) {
        fDriver->noteMuteBtnUp();
      }
    }

    // Right drives, left turns the dome -- see snips_drive.h.
    SnipsDrive::applyUplink(pkt, fSide == kLeft, now, &stickRemote());
  }

  fPrevButtonMask = pkt.buttonMask;
  triggerPercent = pkt.triggerPercent;
  stickXPercent = pkt.stickXPercent;
  stickYPercent = pkt.stickYPercent;
  batteryPercent = pkt.batteryPercent;
  chargeState = pkt.chargeState;
  fConnected = true;
  fLastPacket = now;

  // The controller's last few uplinks before it powers off: let go of the
  // drive/dome now instead of waiting out the failsafe timeout.
  if (pkt.flags & Snips::UplinkPacket::kFlagShuttingDown) {
    if (fDriver) SnipsDrive::markShutDown(&stickRemote());
    disconnect();
  }
}

XBeePocketRemote &SnipsRemote::stickRemote() const {
  return fSide == kRight ? static_cast<XBeePocketRemote &>(fDriver->fDriveStick)
                         : static_cast<XBeePocketRemote &>(fDriver->fDomeStick);
}

void SnipsRemote::checkFailsafe(uint32_t fst) {
  if (!fConnected) return;
  if (fLastPacket + fst >= millis()) return;
  disconnect();
}

void SnipsRemote::disconnect() {
  fConnected = false;
  // Forget held buttons, so a button still down when the link dropped
  // doesn't fire on the first uplink after reconnecting.
  fPrevButtonMask = 0;
  for (ButtonLongPress &lp : fLongPress) lp = ButtonLongPress();
  if (!fDriver) return;

  // Clear alt/mute state if this side owns them, mirroring
  // DriveController/DomeController::onDisconnect() in drive_controllers.cpp.
  int altbtn = fDriver->params.altbtn;
  if (altbtn > 0 && ownsButtonNumber((unsigned)altbtn))
    fDriver->setAltHeld(false);
  int muteBtn = fDriver->params.mutebutton;
  if (muteBtn > 0 && ownsButtonNumber((unsigned)muteBtn))
    fDriver->resetMutePressTimer();
}

Snips::DownlinkPacket SnipsRemote::buildDownlinkPacket() const {
  SnipsDownlink::DroidStatus status;
  status.rightHand = fSide == kRight;
  if (fDriver) {
    const AmidalaParameters &params = fDriver->params;
    const uint8_t wheel = SnipsDownlink::volumeWheelFor(
        params.B[buttonNumber(SnipsButtons::kLeftUp) - 1],
        params.B[buttonNumber(SnipsButtons::kRightUp) - 1],
        params.volumewheel, params.altvolumewheel);
    status.volumePercent =
        fDriver->fAudio.isMuted() ? 0 : fDriver->fAudio.getEffectiveVolume(wheel);
    status.throttlePercent = params.driveSpeedPct;
    status.estop = fDriver->isEstopActive();
    status.motorsDisabled = fDriver->areMotorsDisabled();
    status.muse = fDriver->fAudio.isMusing();
    status.autoDome = fDriver->isAutoDomeOn();
    // No droid battery reading exists yet, so it goes out as unknown.
    const uint32_t now = millis();
    status.sequenceId = fDriver->fSequences.id();
    status.sequenceName = fDriver->fSequences.nameAt(now);
  }
  return SnipsDownlink::build(status);
}
