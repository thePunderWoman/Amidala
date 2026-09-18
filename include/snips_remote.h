// snips_remote.h
// SnipsRemote — models one physical Snips controller (see
// thePunderWoman/Amidala#204 and the sibling thePunderWoman/SnipsControllers
// firmware). Two instances exist, one per hand: kRight (drive-bound, button
// numbers 1-8) and kLeft (dome-bound, button numbers 9-16) -- the same
// right=drive/left=dome convention params.xbr/xbl already use for the XBee
// pocket remote.
//
// Deliberately NOT a subclass of XBeePocketRemote/JoystickController and NOT
// part of AmidalaController::remote[2] -- that model is built around 5 named
// PS-style fields tuned for the existing pocket-remote hardware, which
// Snips's 8-bit button mask doesn't map onto usefully, and nothing needs
// polymorphic iteration over remote[] here (dispatch is already gated on
// params.controllertype at the call site in controller.cpp's animate()).
// This mirrors BTGamepad, itself already a standalone sibling member rather
// than part of remote[].
//
// Method bodies that call AmidalaController are defined in
// src/snips_remote.cpp (same split as xbee_remote.h/drive_controllers.cpp)
// to avoid a circular header dependency.
//
// Phase 1 scope: fire-and-forget button dispatch only (press/long-press/
// alt-modifier/double-press, via the existing ButtonAction system) plus
// telemetry storage (trigger/stick/battery/charge state). Stick/trigger
// analog is decoded but not wired to any drive/dome output yet -- that's
// follow-up work, once the stateful echo-back display exists to give
// feedback. No downlink is sent in Phase 1.

#pragma once

#include <stdint.h>
#include "button_dispatch.h"
#include "snips_buttons.h"
#include "snips_packet.h"

class AmidalaController;

class SnipsRemote {
public:
  // Right = drive-bound, button numbers 1-8. Left = dome-bound, 9-16.
  enum Side { kRight, kLeft };

  SnipsRemote(AmidalaController *driver, Side side) : fDriver(driver), fSide(side) {}

  uint32_t addr = 0;  // low 32 bits of this controller's XBee address (params.xbr/xbl)

  bool isConnected() const { return fConnected; }
  bool failsafe() const { return !fConnected; }

  // Telemetry from the latest decoded uplink.
  uint8_t triggerPercent = 0;
  int8_t stickXPercent = 0;
  int8_t stickYPercent = 0;
  uint8_t batteryPercent = 0;
  Snips::ChargeState chargeState = Snips::ChargeState::kDone;

  // Called once per received+decoded uplink packet matching `addr`.
  void onUplinkReceived(const Snips::UplinkPacket &pkt);

  // Called once per animate() tick: ages out fConnected if no uplink has
  // arrived within `fst` ms, mirroring DriveController/DomeController's
  // connect/disconnect lifecycle.
  void checkFailsafe(uint32_t fst);

private:
  // Maps a SnipsButtons::Index bit to this side's 1-based button number.
  unsigned buttonNumber(unsigned bitIndex) const {
    return (fSide == kRight ? 1 : 9) + bitIndex;
  }

  AmidalaController *fDriver;
  Side fSide;
  bool fConnected = false;
  uint32_t fLastPacket = 0;
  uint16_t fPrevButtonMask = 0;
  ButtonLongPress fLongPress[SnipsButtons::kCount];
};
