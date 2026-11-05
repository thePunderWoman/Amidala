// snips_drive.h
// Feeds a Snips controller's stick into the existing drive/dome remote
// objects, so the right controller drives and the left one turns the dome
// through exactly the code the XBee pocket remotes and RC mode use:
// TankDrive/DomeDrive read fDriveStick/fDomeStick, and DriveController/
// DomeController::notify() supply the lag-based safety stop, connect/
// disconnect enabling, and (dome side) gesture collection.
//
// Only the stick and, on the left, the stick click (gesture start/end, like
// the dome remote's own reserved stick click) go through the remote object.
// Every other Snips button is dispatched by SnipsRemote itself (see
// snips_remote.h), so the remote's own face buttons stay released and never
// fire buttons 1-9.
//
// Deliberately free of any controller dependency so it's unit-testable
// natively; XBeePocketRemote itself is header-only.
#pragma once

#include <stdint.h>

#include "snips_buttons.h"
#include "snips_packet.h"
#include "xbee_remote.h"

namespace SnipsDrive {

// XBeePocketRemote::update() turns its raw 0-1024 x/y into stick values with
// map(raw, 0, 1024, 127, -128). This is the inverse, rounded up so every
// stick value -128..127 comes back exactly (map() truncates toward zero).
inline uint16_t rawForStick(int stick) {
  if (stick > 127) stick = 127;
  if (stick < -128) stick = -128;
  return (uint16_t)(((127 - stick) * 1024 + 254) / 255);
}

// Snips stick percent (-100..100) to the JoystickController convention the
// drive code expects (PS3/HID style, the same one the Bluetooth gamepad
// sends): right is positive X, forward is negative Y. The controller reports
// positive X as physically left and positive Y as up, so both flip.
inline int stickXFromPercent(int8_t percent) { return -(int)percent * 127 / 100; }
inline int stickYFromPercent(int8_t percent) { return -(int)percent * 127 / 100; }

// Raw wheel value that update() maps to an analog reading of 0. Volume (l1)
// and the throttle boost (l2) aren't driven from Snips through the remote
// object, and the remote's constructor default (0) maps to 255 -- full
// volume and full throttle boost -- so both are pinned here instead.
constexpr uint16_t kWheelOff = 1024;

// Fills `remote` from one decoded uplink. `stickClickIsGesture` is true for
// the dome-side (left) controller, whose stick click starts/ends gestures.
inline void applyUplink(const Snips::UplinkPacket &pkt, bool stickClickIsGesture,
                        uint32_t nowMs, XBeePocketRemote *remote) {
  remote->x = rawForStick(stickXFromPercent(pkt.stickXPercent));
  remote->y = rawForStick(stickYFromPercent(pkt.stickYPercent));
  remote->w1 = kWheelOff;
  remote->w2 = kWheelOff;
  for (bool &b : remote->button) b = false;
  remote->button[4] = stickClickIsGesture &&
                      (pkt.buttonMask & (1u << SnipsButtons::kStickClick)) != 0;
  remote->lastPacket = nowMs;
  remote->type = XBeePocketRemote::kXBee;
}

// The controller's final uplinks before it powers off carry
// kFlagShuttingDown, so the drive stops right away instead of after the
// failsafe timeout.
inline void markShutDown(XBeePocketRemote *remote) {
  remote->type = XBeePocketRemote::kFailsafe;
}

// Called every animate() tick for each remote, mirroring the XBee branch in
// AmidalaController::animate(): once no uplink has arrived within the
// failsafe timeout the remote goes failsafe, and update() runs while it
// was live so notify()'s safety stop and onDisconnect() get their chance.
//
// A remote that markShutDown() already made failsafe still gets one more
// update() while it's connected, which is what runs onDisconnect().
inline void tick(XBeePocketRemote *remote, uint32_t failsafeMs, uint32_t nowMs) {
  const bool live = remote->type == XBeePocketRemote::kXBee;
  if (!live && !remote->isConnected()) return;
  if (live && nowMs - remote->lastPacket > failsafeMs) {
    remote->type = XBeePocketRemote::kFailsafe;
  }
  remote->update();
}

}  // namespace SnipsDrive
