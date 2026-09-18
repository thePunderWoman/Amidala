// snips_buttons.h
// All 12 button bits carried in Snips::UplinkPacket::buttonMask (see
// snips_packet.h), matching SnipsControllers/include/buttons.h's
// Buttons::Index 1:1. The last 4 (LeftUp/LeftDown/RightUp/RightDown) are
// the stateful "step a value" buttons (issue #204 phase 2) -- fully
// reassignable via the normal ButtonAction system (see
// ButtonAction::kVolumeStep/kThrottleStep) like any other button, not
// fixed to a particular meaning by hardware position. Phase 1 excluded
// these 4 bits entirely; phase 2 wires them up like the rest.
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
  kLeftUp,
  kLeftDown,
  kRightUp,
  kRightDown,
  kCount
};

}  // namespace SnipsButtons
