// step_value.h
// Pure clamp/step arithmetic shared by AmidalaController::stepVolume()/
// stepDriveSpeed() (issue #204 phase 2) -- kept as a standalone, hardware-
// free helper so the actual increment math is unit-testable without a full
// AmidalaController.
#pragma once

#include <stdint.h>

// Steps `current` by `step` in the given direction, clamped to [min, max].
// `step` is not itself clamped to the remaining headroom -- e.g.
// stepValue(98, 5, true) returns `max` (100), not an out-of-range 103.
inline uint8_t stepValue(uint8_t current, uint8_t step, bool up,
                          uint8_t min = 0, uint8_t max = 100) {
  if (up) {
    uint16_t next = (uint16_t)current + step;
    return next > max ? max : (uint8_t)next;
  } else {
    int16_t next = (int16_t)current - step;
    return next < min ? min : (uint8_t)next;
  }
}
