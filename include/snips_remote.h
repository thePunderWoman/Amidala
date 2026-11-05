// snips_remote.h
// SnipsRemote — models one physical Snips controller (see
// thePunderWoman/Amidala#204 and the sibling thePunderWoman/SnipsControllers
// firmware). Two instances exist, one per hand: kRight (drive-bound) and
// kLeft (dome-bound) -- the same right=drive/left=dome convention
// params.xbr/xbl already use for the XBee pocket remote.
//
// Button numbering is deliberately NOT a simple contiguous 1-12/13-24 split
// (see buttonNumber()) -- CONTROLLER_TYPE_XBEE/_BLUETOOTH give buttons 1-9 a
// real, live meaning (drive/dome stick presses), and B[]/LB[]/AB[]/DB[] are
// shared storage across all controller types (see params.h). A compiled-in
// non-kNone default for a Snips button landing on 1-9 would silently change
// stock XBee/Bluetooth behavior for every non-Snips rig the moment this
// shipped, not just reserve a harmless unused slot the way the fire-and-
// forget buttons (which get no defaults) safely do. So: fire-and-forget
// buttons stay at 1-8 (Right) / 9-16 (Left) -- unchanged from Phase 1, still
// collision-free since nothing defaults them -- and the 4 stateful buttons
// for BOTH sides are pushed out past 16 entirely (Left's at 17-20, Right's
// at 21-24), so the ones that DO get non-kNone defaults never land below 10.
//
// Deliberately NOT a subclass of XBeePocketRemote/JoystickController and NOT
// part of AmidalaController::remote[2] -- that model is built around 5 named
// PS-style fields tuned for the existing pocket-remote hardware, which
// Snips's 12-bit button mask doesn't map onto usefully, and nothing needs
// polymorphic iteration over remote[] here (dispatch is already gated on
// params.controllertype at the call site in controller.cpp's animate()).
// This mirrors BTGamepad, itself already a standalone sibling member rather
// than part of remote[].
//
// Method bodies that call AmidalaController are defined in
// src/snips_remote.cpp (same split as xbee_remote.h/drive_controllers.cpp)
// to avoid a circular header dependency.
//
// Phase 2 (issue #204): all 12 buttons per side are wired through the
// existing ButtonAction system, including the 4 stateful "step a value"
// buttons (kLeftUp/kLeftDown/kRightUp/kRightDown) via
// ButtonAction::kVolumeStep/kThrottleStep -- these are fully reassignable
// like any other button, not fixed to a meaning by hardware position.
// buildDownlinkPacket() derives the controller's status screen data fresh
// on every call from current config + live state (see its own comment), and
// xbee_spi.cpp sends the result after every processed uplink -- both to
// show a changed value and because the physical controller treats a
// downlink's absence for 5s as "disconnected" regardless of uplink flow.

#pragma once

#include <stdint.h>
#include "button_dispatch.h"
#include "snips_buttons.h"
#include "snips_packet.h"

class AmidalaController;
class XBeePocketRemote;

class SnipsRemote {
public:
  // Right = drive-bound. Left = dome-bound.
  enum Side { kRight, kLeft };

  SnipsRemote(AmidalaController *driver, Side side) : fDriver(driver), fSide(side) {}

  uint32_t addr = 0;  // low 32 bits of this controller's XBee address (params.xbr/xbl)
  uint16_t addr16 = 0;  // sender's 16-bit network address, from the latest uplink's
                        // XBeeReceivePacket -- needed to address a downlink reply.

  bool isConnected() const { return fConnected; }
  bool failsafe() const { return !fConnected; }

  // Telemetry from the latest decoded uplink.
  uint8_t triggerPercent = 0;
  int8_t stickXPercent = 0;
  int8_t stickYPercent = 0;
  uint8_t batteryPercent = 0;
  Snips::ChargeState chargeState = Snips::ChargeState::kDone;

  // Called once per received+decoded uplink packet matching `addr`.
  // `senderAddr16` is the sender's current 16-bit network address (stored
  // in `addr16` above for the downlink reply xbee_spi.cpp sends right
  // after this call).
  void onUplinkReceived(const Snips::UplinkPacket &pkt, uint16_t senderAddr16);

  // Called once per animate() tick: ages out fConnected if no uplink has
  // arrived within `fst` ms, mirroring DriveController/DomeController's
  // connect/disconnect lifecycle. An uplink flagged kFlagShuttingDown
  // disconnects straight away instead.
  void checkFailsafe(uint32_t fst);

  // Builds this controller's downlink reply: fixed handedness for this
  // Side plus the droid's live volume (whichever wheel this controller's own
  // volume step button adjusts -- see SnipsDownlink::volumeWheelFor()),
  // drive speed cap, e-stop/muse/auto dome state and the latest sequence.
  // Recomputed every call rather than edge-tracked, so a config change
  // shows on the very next downlink. Defined in src/snips_remote.cpp (needs
  // the full AmidalaController); the wire decisions are in snips_downlink.h.
  Snips::DownlinkPacket buildDownlinkPacket() const;

  // Button numbering (see the file header comment for why this isn't a
  // simple contiguous split): fire-and-forget buttons (Macro1-6, Bumper,
  // Stick Click) are 1-8 (Right) / 9-16 (Left); the 4 stateful buttons
  // (LeftUp, LeftDown, RightUp, RightDown) are 17-20 (Left) / 21-24 (Right).
  // Public/static so web UI-adjacent code and tests can share the exact
  // same mapping rather than re-deriving it.
  static unsigned buttonNumberFor(Side side, unsigned bitIndex) {
    if (bitIndex < SnipsButtons::kLeftUp) {  // fire-and-forget: bits 0-7
      return (side == kRight ? 1 : 9) + bitIndex;
    }
    // stateful: bits 8-11 (kLeftUp..kRightDown)
    return (side == kRight ? 21 : 17) + (bitIndex - SnipsButtons::kLeftUp);
  }

  // True if button number `num` belongs to `side`'s range (see
  // buttonNumberFor()). Left ends up contiguous (9-20); Right does not
  // (1-8 plus 21-24), so this can't be a simple min/max comparison.
  static bool ownsButtonNumber(Side side, unsigned num) {
    if (side == kLeft) return num >= 9 && num <= 20;
    return (num >= 1 && num <= 8) || (num >= 21 && num <= 24);
  }

  // Inverse of buttonNumberFor() -- which SnipsButtons::Index bit button
  // `num` (already confirmed via ownsButtonNumber()) corresponds to on
  // `side`. Public/static alongside the other two for the same reason.
  static unsigned bitIndexForButtonNumber(Side side, unsigned num) {
    if (side == kLeft) return num - 9;
    return (num <= 8) ? (num - 1) : (SnipsButtons::kLeftUp + (num - 21));
  }

  // The left (dome-bound) controller's stick click starts and ends gestures,
  // like the XBee dome remote's own stick click (see snips_drive.h), so it's
  // never dispatched as button 16 and can't be the alt or mute button.
  static bool isGestureButton(Side side, unsigned bitIndex) {
    return side == kLeft && bitIndex == SnipsButtons::kStickClick;
  }

private:
  unsigned buttonNumber(unsigned bitIndex) const { return buttonNumberFor(fSide, bitIndex); }
  bool ownsButtonNumber(unsigned num) const { return ownsButtonNumber(fSide, num); }
  unsigned bitIndexForButtonNumber(unsigned num) const {
    return bitIndexForButtonNumber(fSide, num);
  }

  // The drive (right) or dome (left) remote this controller's stick feeds.
  XBeePocketRemote &stickRemote() const;

  // Marks this controller disconnected and clears the alt/mute state it
  // owns, mirroring DriveController/DomeController::onDisconnect().
  void disconnect();

  AmidalaController *fDriver;
  Side fSide;
  bool fConnected = false;
  uint32_t fLastPacket = 0;
  uint16_t fPrevButtonMask = 0;
  ButtonLongPress fLongPress[SnipsButtons::kCount];
};
