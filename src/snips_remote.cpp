// snips_remote.cpp
// SnipsRemote method bodies that call AmidalaController -- see
// include/snips_remote.h for the class and its design rationale.

#include "controller.h"
#include "snips_remote.h"

void SnipsRemote::onUplinkReceived(const Snips::UplinkPacket &pkt) {
  uint32_t now = millis();

  if (fDriver) {
    unsigned rangeMin = (fSide == kRight) ? 1 : 9;
    unsigned rangeMax = rangeMin + SnipsButtons::kCount - 1;

    int altbtn = fDriver->params.altbtn;
    bool altbtnInRange = (altbtn >= (int)rangeMin && altbtn <= (int)rangeMax);
    if (altbtnInRange) {
      unsigned altBit = (unsigned)altbtn - rangeMin;
      fDriver->setAltHeld((pkt.buttonMask & (1u << altBit)) != 0);
    }
    bool altHeld = fDriver->isAltHeld();

    int muteBtn = fDriver->params.mutebutton;
    bool muteBtnInRange = (muteBtn >= (int)rangeMin && muteBtn <= (int)rangeMax);
    unsigned muteBit = muteBtnInRange ? (unsigned)muteBtn - rangeMin : SnipsButtons::kCount;

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
  unsigned rangeMin = (fSide == kRight) ? 1 : 9;
  unsigned rangeMax = rangeMin + SnipsButtons::kCount - 1;
  int altbtn = fDriver->params.altbtn;
  if (altbtn >= (int)rangeMin && altbtn <= (int)rangeMax)
    fDriver->setAltHeld(false);
  int muteBtn = fDriver->params.mutebutton;
  if (muteBtn >= (int)rangeMin && muteBtn <= (int)rangeMax)
    fDriver->resetMutePressTimer();
}
