// snips_buttons.h
// The 8 fire-and-forget button bits carried in Snips::UplinkPacket::buttonMask
// (see snips_packet.h), mirroring SnipsControllers/include/buttons.h's
// Buttons::Index. The 4 stateful up/down bits (LeftUp/LeftDown/RightUp/
// RightDown -- the generic "echo the current value back to the OLED" pair,
// see thePunderWoman/Amidala#204) are intentionally omitted: Phase 1 only
// wires up the fire-and-forget buttons through the existing ButtonAction
// system, not the stateful echo-back concept.
#pragma once

#include <stdint.h>

namespace SnipsButtons {

enum Index : uint8_t {
  kMacro1 = 0,
  kMacro2,
  kMacro3,
  kMacro4,
  kMacro5,
  kMacro6,
  kBumper,
  kStickClick,
  kCount
};

}  // namespace SnipsButtons
