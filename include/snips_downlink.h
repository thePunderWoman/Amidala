// snips_downlink.h
// What Amidala tells a Snips controller's status screen, as a pure builder
// plus the small bits of state it needs. SnipsRemote::buildDownlinkPacket()
// (src/snips_remote.cpp) gathers the live values from AmidalaController into
// a DroidStatus and calls build(); everything that decides what goes on the
// wire lives here so it's unit-testable natively, without a controller.
//
// See SnipsControllers' include/status_screen.h for how the controller
// draws each field.
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "button_actions.h"
#include "snips_packet.h"

namespace SnipsDownlink {

// Remembers the most recently started sequence for the downlink. The id
// goes up on every start, so the controller can tell a new start of the same
// sequence from repeats of one it has already shown. The name is only sent
// for kShowMs after the start: the controller shows it for that long when
// no progress is reported, and a controller that connects later won't show
// a sequence that finished long ago.
class SequenceTracker {
public:
  static constexpr uint32_t kShowMs = 5000;

  void start(const char *name, uint32_t nowMs) {
    fId++;
    snprintf(fName, sizeof(fName), "%s", name);
    fStartMs = nowMs;
    fStarted = true;
  }

  uint8_t id() const { return fId; }

  const char *nameAt(uint32_t nowMs) const {
    return (fStarted && nowMs - fStartMs < kShowMs) ? fName : "";
  }

private:
  uint8_t fId = 0;
  char fName[Snips::DownlinkPacket::kSequenceNameLength + 1] = {};
  uint32_t fStartMs = 0;
  bool fStarted = false;
};

// The volume wheel a controller's screen reads: the one its own volume step
// button adjusts (LeftUp checked first, then RightUp), so the number moves
// when that button is pressed. Mirrors AmidalaController::stepVolume()'s
// wheel resolution, including AmidalaAudio::setAltVolumeNoResponse()'s
// fallthrough to volumewheel when altvolumewheel is 0. A controller with no
// volume step button reads the main volumewheel.
inline uint8_t volumeWheelFor(const ButtonAction &leftUp, const ButtonAction &rightUp,
                              uint8_t volumewheel, uint8_t altvolumewheel) {
  const ButtonAction *pair[] = {&leftUp, &rightUp};
  for (const ButtonAction *action : pair) {
    if (action->action == ButtonAction::kVolumeStep) {
      return (action->volstep.target && altvolumewheel != 0) ? altvolumewheel : volumewheel;
    }
  }
  return volumewheel;
}

// Live values for one controller's downlink. -1 = unknown for every
// percentage.
struct DroidStatus {
  bool rightHand = false;
  int volumePercent = -1;
  int throttlePercent = -1;  // the drive speed cap
  int droidBatteryPercent = -1;
  bool estop = false;
  bool motorsDisabled = false;
  bool muse = false;
  bool autoDome = false;
  uint8_t sequenceId = 0;
  const char *sequenceName = "";
  int sequenceProgressPercent = -1;
};

// Percentages outside 0-100 are clamped; negative means unknown.
inline uint8_t wirePercent(int percent) {
  if (percent < 0) return Snips::DownlinkPacket::kUnknown;
  return percent > 100 ? 100 : (uint8_t)percent;
}

inline Snips::DownlinkPacket build(const DroidStatus &status) {
  Snips::DownlinkPacket pkt;
  pkt.handedness = status.rightHand ? Snips::DownlinkPacket::Handedness::kRight
                                    : Snips::DownlinkPacket::Handedness::kLeft;
  pkt.volumePercent = wirePercent(status.volumePercent);
  pkt.throttlePercent = wirePercent(status.throttlePercent);
  pkt.droidBatteryPercent = wirePercent(status.droidBatteryPercent);
  pkt.sequenceProgressPercent = wirePercent(status.sequenceProgressPercent);
  if (status.estop) pkt.flags |= Snips::DownlinkPacket::kFlagEstop;
  if (status.motorsDisabled) pkt.flags |= Snips::DownlinkPacket::kFlagMotorsDisabled;
  if (status.muse) pkt.flags |= Snips::DownlinkPacket::kFlagMuse;
  if (status.autoDome) pkt.flags |= Snips::DownlinkPacket::kFlagAutoDome;
  pkt.sequenceId = status.sequenceId;
  snprintf(pkt.sequenceName, sizeof(pkt.sequenceName), "%s",
           status.sequenceName ? status.sequenceName : "");
  return pkt;
}

}  // namespace SnipsDownlink
