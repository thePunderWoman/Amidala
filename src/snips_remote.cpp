// snips_remote.cpp
// SnipsRemote method bodies that call AmidalaController -- see
// include/snips_remote.h for the class and its design rationale.

#include <stdio.h>
#include "controller.h"
#include "snips_remote.h"

void SnipsRemote::onUplinkReceived(const Snips::UplinkPacket &pkt, uint16_t senderAddr16) {
  uint32_t now = millis();
  addr16 = senderAddr16;

  if (fDriver) {
    int altbtn = fDriver->params.altbtn;
    bool altbtnInRange = altbtn > 0 && ownsButtonNumber((unsigned)altbtn);
    if (altbtnInRange) {
      unsigned altBit = bitIndexForButtonNumber((unsigned)altbtn);
      fDriver->setAltHeld((pkt.buttonMask & (1u << altBit)) != 0);
    }
    bool altHeld = fDriver->isAltHeld();

    int muteBtn = fDriver->params.mutebutton;
    bool muteBtnInRange = muteBtn > 0 && ownsButtonNumber((unsigned)muteBtn);
    unsigned muteBit = muteBtnInRange ? bitIndexForButtonNumber((unsigned)muteBtn) : SnipsButtons::kCount;

    for (unsigned i = 0; i < SnipsButtons::kCount; i++) {
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
  }

  fPrevButtonMask = pkt.buttonMask;
  triggerPercent = pkt.triggerPercent;
  stickXPercent = pkt.stickXPercent;
  stickYPercent = pkt.stickYPercent;
  batteryPercent = pkt.batteryPercent;
  chargeState = pkt.chargeState;
  fConnected = true;
  fLastPacket = now;
}

void SnipsRemote::checkFailsafe(uint32_t fst) {
  if (!fConnected) return;
  if (fLastPacket + fst >= millis()) return;

  fConnected = false;
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

void SnipsRemote::fillPairFields(const ButtonAction &action, char *outLabel,
                                  char *outValue) const {
  outLabel[0] = '\0';
  outValue[0] = '\0';
  if (!fDriver) return;

  switch (action.action) {
    case ButtonAction::kVolumeStep: {
      // Must match AmidalaController::stepVolume()'s wheel resolution
      // exactly (and AmidalaAudio::setAltVolumeNoResponse()'s own
      // fallthrough) -- see its comment.
      uint8_t wheel = (action.volstep.target && fDriver->params.altvolumewheel != 0)
                          ? fDriver->params.altvolumewheel
                          : fDriver->params.volumewheel;
      uint8_t value = fDriver->fAudio.getEffectiveVolume(wheel);
      snprintf(outLabel, Snips::DownlinkPacket::kFieldLength + 1, "%s",
               action.volstep.target ? "AltVol" : "Vol");
      snprintf(outValue, Snips::DownlinkPacket::kFieldLength + 1, "%u", value);
      break;
    }
    case ButtonAction::kThrottleStep:
      snprintf(outLabel, Snips::DownlinkPacket::kFieldLength + 1, "%s", "Drive");
      snprintf(outValue, Snips::DownlinkPacket::kFieldLength + 1, "%u",
               fDriver->params.driveSpeedPct);
      break;
    default:
      break;  // not assigned to a step action -- leave both fields blank
  }
}

Snips::DownlinkPacket SnipsRemote::buildDownlinkPacket() const {
  Snips::DownlinkPacket pkt;
  pkt.handedness = (fSide == kRight) ? Snips::DownlinkPacket::Handedness::kRight
                                      : Snips::DownlinkPacket::Handedness::kLeft;
  if (!fDriver) return pkt;

  unsigned leftUpButton = buttonNumber(SnipsButtons::kLeftUp);
  unsigned rightUpButton = buttonNumber(SnipsButtons::kRightUp);
  fillPairFields(fDriver->params.B[leftUpButton - 1], pkt.leftLabel, pkt.leftValue);
  fillPairFields(fDriver->params.B[rightUpButton - 1], pkt.rightLabel, pkt.rightValue);
  return pkt;
}
