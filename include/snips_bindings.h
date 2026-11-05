// snips_bindings.h
// Configurable analog inputs for Snips controllers: each binding maps one
// controller's trigger, tilt or rotation rate to something on the droid --
// a drive accelerator, dome speed, dome position, a servo, or a button.
// Set on the Controllers page and stored as sbind= lines in config.txt.
//
// Pure: parsing, formatting and evaluation live here so they're
// unit-testable natively. AmidalaController::applySnipsBindings()
// (src/snips_bindings_apply.cpp) turns an Outputs into real effects.
//
// IMU values come from the controller's uplink (Snips::UplinkPacket), in
// the controller's frame: +X right, +Y toward the stick, +Z out of the top
// face. Pitch is + with the stick end raised, roll + with the right edge
// raised, and rotation rates follow the right-hand rule.
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "button_dispatch.h"
#include "snips_packet.h"

namespace SnipsBindings {

enum class Source : uint8_t {
  kTrigger = 0,  // 0..100 %
  kPitch,        // -90..90 degrees
  kRoll,         // -90..90 degrees
  kRateX,        // degrees per second
  kRateY,
  kRateZ,
  kCount
};

enum class Target : uint8_t {
  kNone = 0,
  kDriveAccel,    // drive speed from 0 up to the speed cap
  kDomeSpeed,     // dome speed from 0 up to its configured maximum
  kDomePosition,  // tilt: +/- arg degrees around front; rate: follows the turn
  kServo,         // servo channel `arg` (1-based) across its travel
  kButton,        // virtual button buttonFor(index), with press/long/alt/double
  kCount
};

constexpr int kMaxBindings = 6;

// Virtual button numbers 25-30, after the Snips controllers' 24 physical
// buttons, so a binding used as a button gets the same B/LB/AB/DB layers.
constexpr unsigned kFirstVirtualButton = 25;
inline unsigned buttonFor(int index) { return kFirstVirtualButton + (unsigned)index; }

// A binding fires its button when its value rises past kPressAbove and lets
// go below kReleaseBelow, so a value hovering at one point can't chatter.
constexpr float kPressAbove = 0.6f;
constexpr float kReleaseBelow = 0.4f;

// Integrating a rotation rate into a dome heading skips gaps longer than
// this (a dropped link), rather than turning the dome by a burst.
constexpr uint32_t kMaxIntegrateGapMs = 250;

struct Binding {
  bool used = false;
  uint8_t side = 0;  // SnipsRemote::Side: 0 right, 1 left
  Source source = Source::kTrigger;
  Target target = Target::kNone;
  uint8_t arg = 0;     // kServo: channel; kDomePosition (tilt): range degrees
  int16_t inMin = 0;   // source units, mapped to 0
  int16_t inMax = 100; // source units, mapped to 1
  bool invert = false;
};

// Default input range for a source, used when the UI adds a binding.
inline void defaultRange(Source source, int16_t *outMin, int16_t *outMax) {
  switch (source) {
    case Source::kTrigger: *outMin = 0; *outMax = 100; break;
    case Source::kPitch:
    case Source::kRoll: *outMin = -45; *outMax = 45; break;
    default: *outMin = -180; *outMax = 180; break;
  }
}

inline bool isRate(Source source) {
  return source == Source::kRateX || source == Source::kRateY || source == Source::kRateZ;
}

// The raw value of `source` in this uplink, or false if the uplink doesn't
// carry it (an older controller firmware, or no IMU on that board).
inline bool sourceValue(Source source, const Snips::UplinkPacket &pkt, float *out) {
  switch (source) {
    case Source::kTrigger: *out = pkt.triggerPercent; return true;
    case Source::kPitch:
      if (!(pkt.imuFlags & Snips::UplinkPacket::kImuTiltValid)) return false;
      *out = pkt.pitchDeg;
      return true;
    case Source::kRoll:
      if (!(pkt.imuFlags & Snips::UplinkPacket::kImuTiltValid)) return false;
      *out = pkt.rollDeg;
      return true;
    case Source::kRateX:
    case Source::kRateY:
    case Source::kRateZ: {
      if (!(pkt.imuFlags & Snips::UplinkPacket::kImuRatesValid)) return false;
      int axis = (int)source - (int)Source::kRateX;
      *out = pkt.rateDeciDps[axis] / 10.0f;
      return true;
    }
    default: return false;
  }
}

// Maps a raw value through the binding's range to 0..1, clamped, then
// inverted if asked. A zero-width range reads 0.
inline float normalize(const Binding &b, float raw) {
  float span = (float)b.inMax - (float)b.inMin;
  float v = span == 0 ? 0.0f : (raw - b.inMin) / span;
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  return b.invert ? 1.0f - v : v;
}

// ---- config line ------------------------------------------------------------
// sbind=N,side,source,target,arg,inMin,inMax,invert  (N 1-based). The part
// after "sbind=" is also the value POSTed as /api/config key sbind_<N-1>.

// Parses "side,source,target,arg,inMin,inMax,invert" into `out`. Returns
// false (leaving `out` untouched) on a short or out-of-range line.
inline bool parseBinding(const char *csv, Binding *out) {
  long v[7];
  int n = 0;
  const char *p = csv;
  while (n < 7) {
    char *end = nullptr;
    v[n] = strtol(p, &end, 10);
    if (end == p) return false;
    n++;
    if (*end != ',') break;
    p = end + 1;
  }
  if (n < 7) return false;
  if (v[0] < 0 || v[0] > 1) return false;
  if (v[1] < 0 || v[1] >= (long)Source::kCount) return false;
  if (v[2] < 0 || v[2] >= (long)Target::kCount) return false;
  if (v[3] < 0 || v[3] > 255) return false;
  if (v[4] < -32768 || v[4] > 32767 || v[5] < -32768 || v[5] > 32767) return false;
  Binding b;
  b.used = true;
  b.side = (uint8_t)v[0];
  b.source = (Source)v[1];
  b.target = (Target)v[2];
  b.arg = (uint8_t)v[3];
  b.inMin = (int16_t)v[4];
  b.inMax = (int16_t)v[5];
  b.invert = v[6] != 0;
  *out = b;
  return true;
}

// Writes the parseBinding() form into `buf`. Returns the length written.
inline int formatBinding(const Binding &b, char *buf, size_t size) {
  return snprintf(buf, size, "%u,%u,%u,%u,%d,%d,%u", (unsigned)b.side,
                  (unsigned)b.source, (unsigned)b.target, (unsigned)b.arg,
                  (int)b.inMin, (int)b.inMax, b.invert ? 1u : 0u);
}

// ---- evaluation ---------------------------------------------------------------

struct ServoMove {
  uint8_t channel;  // 1-based
  float pos;        // 0..1 across the channel's travel
};

struct ButtonEvent {
  unsigned num;
  bool up;
  bool longUp;
};

// What one uplink asks for. A scale of -1 means "this side has no binding
// for it".
struct Outputs {
  float driveScale = -1;
  float domeScale = -1;
  bool hasDomeAngle = false;
  float domeAngle = 0;   // degrees from front, from a tilt binding
  float domeTurn = 0;    // degrees to add, from a rotation-rate binding
  ServoMove servos[kMaxBindings];
  int servoCount = 0;
  ButtonEvent buttons[kMaxBindings];
  int buttonCount = 0;
};

class Evaluator {
public:
  // Evaluates every binding for `side` against one uplink.
  void evaluate(uint8_t side, const Snips::UplinkPacket &pkt, uint32_t nowMs,
                const Binding *bindings, Outputs *out) {
    for (int i = 0; i < kMaxBindings; i++) {
      const Binding &b = bindings[i];
      if (!b.used || b.side != side || b.target == Target::kNone) continue;
      float raw;
      if (!sourceValue(b.source, pkt, &raw)) continue;
      float v = normalize(b, raw);
      switch (b.target) {
        case Target::kDriveAccel:
          if (v > out->driveScale) out->driveScale = v;
          break;
        case Target::kDomeSpeed:
          if (v > out->domeScale) out->domeScale = v;
          break;
        case Target::kDomePosition:
          if (isRate(b.source)) {
            out->domeTurn += integrate(i, b.invert ? -raw : raw, nowMs);
          } else {
            out->hasDomeAngle = true;
            out->domeAngle = (v * 2.0f - 1.0f) * b.arg;
          }
          break;
        case Target::kServo:
          if (b.arg >= 1) out->servos[out->servoCount++] = {b.arg, v};
          break;
        case Target::kButton:
          pressButton(i, v, nowMs, out);
          break;
        default:
          break;
      }
    }
  }

  // Forgets held buttons and integration state for `side`'s bindings, so a
  // reconnect starts clean and a held binding doesn't fire on its first
  // uplink back.
  void reset(uint8_t side, const Binding *bindings) {
    for (int i = 0; i < kMaxBindings; i++) {
      if (bindings[i].side != side) continue;
      fPressed[i] = false;
      fLongPress[i] = ButtonLongPress();
      fLastMs[i] = 0;
      fHaveLast[i] = false;
    }
  }

private:
  float integrate(int i, float rateDps, uint32_t nowMs) {
    float turned = 0;
    if (fHaveLast[i] && nowMs - fLastMs[i] <= kMaxIntegrateGapMs) {
      turned = rateDps * (float)(nowMs - fLastMs[i]) / 1000.0f;
    }
    fHaveLast[i] = true;
    fLastMs[i] = nowMs;
    return turned;
  }

  void pressButton(int i, float v, uint32_t nowMs, Outputs *out) {
    bool wasPressed = fPressed[i];
    if (!wasPressed && v >= kPressAbove) fPressed[i] = true;
    else if (wasPressed && v <= kReleaseBelow) fPressed[i] = false;
    bool down = !wasPressed && fPressed[i];
    bool up = wasPressed && !fPressed[i];
    auto r = fLongPress[i].update(down, up, fPressed[i], nowMs, DEFAULT_LONG_PRESS_MS);
    if (r.up || r.longUp) out->buttons[out->buttonCount++] = {buttonFor(i), r.up, r.longUp};
  }

  bool fPressed[kMaxBindings] = {};
  ButtonLongPress fLongPress[kMaxBindings];
  uint32_t fLastMs[kMaxBindings] = {};
  bool fHaveLast[kMaxBindings] = {};
};

}  // namespace SnipsBindings
