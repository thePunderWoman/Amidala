// pin_assignment.h
// Pure validation logic for runtime GPIO role reassignment (issue #133).
//
// Model: each of the 11 pins already physically broken out on this board's
// headers gets assigned a ROLE TYPE (DOUT / Analog / PPM / Servo / Hall).
// The COUNT in each category is derived from however many pins currently
// have that type -- e.g. free up a pin from DOUT duty, retype it Servo, and
// you get one fewer DOUT and one more servo channel. This is a role swap
// among the 11 existing header pins, not a general "pick any ESP32-S3 GPIO"
// tool: the pool below never contains a pin that isn't already wired to a
// header on this board.
//
// Hardware ceilings enforced here:
//   - Servo <= 8: the ESP32-S3's LEDC peripheral has exactly 8 PWM channels,
//     full stop, regardless of how many pins are in the pool.
//   - Analog: only GPIO1-10 are ADC1-capable (WiFi-safe) on ESP32-S3; within
//     the pool that's {1,2,3,4,5,6}.
//   - PPM <= 1: there's a single PPMDecoder instance.
//   - Hall <= 1: there's a single dome hall-sensor input. Only meaningful
//     when DOME_DRIVE_ROBOCLAW is compiled in -- this header stays agnostic
//     of DOME_DRIVE (no #include of drive_config.h), so it's the caller's
//     job to decide whether to ever offer/use the Hall role at all. Hall
//     defaults to GPIO40 (this PCB's hall sensor trace) but is reassignable
//     if a user has physically rewired their hall sensor to a different pin.
//   - DOUT: no ceiling beyond the pool size -- plain digitalWrite() has no
//     hardware channel-count limit, unlike PWM/LEDC.
//
// I2C (fixed) and all three UART roles (RoboClaw/WCB/aux, also fixed) are
// NOT part of this pool; see issue #133 for that follow-up work.
//
// Deliberately free of any Arduino/params dependency so it's unit-testable
// natively (same pattern as safety_stop_latch.h / bt_scan_policy.h /
// dome_position_math.h).

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum class PinRoleType : uint8_t { kDout, kAnalog, kPpm, kServo, kHall, kCount };

// The 11 pins already physically broken out on the board's headers today --
// the entire assignable pool. Also the single source of truth for the web
// UI's per-pin rows (see buildFullConfigJson()'s "pinRoles"/pin list).
//
// GPIO3 (default role: Servo) is a JTAG-strapping pin, but it's already in
// production use as a PWM output today with no issue (Reeltwo's own
// ServoDispatchESP32::validPWM() has this exclusion commented out
// upstream) -- keeping it in the pool introduces no new risk, since it was
// already implicitly in play.
static constexpr uint8_t kAssignablePins[11] = {
    1, 2, 3, 4, 5, 6, 39, 40, 41, 42, 47,
};

static constexpr uint8_t kMaxServoChannels = 8;  // ESP32-S3 LEDC hardware ceiling
static constexpr uint8_t kMaxPpmPins       = 1;  // single PPMDecoder instance
static constexpr uint8_t kMaxHallPins      = 1;  // single dome hall-sensor input

// 0xFF sentinel: "no such pin" (used by nthPinWithRole()).
static constexpr uint8_t kNoPin = 0xFF;

inline bool isPinInAssignablePool(uint8_t pin) {
    for (uint8_t p : kAssignablePins) {
        if (p == pin) return true;
    }
    return false;
}

// Is `role` even electrically valid for `pin`, independent of current
// counts elsewhere? Analog requires ADC1-capability (pin <= 10, which
// within the pool means {1,2,3,4,5,6}); every other role accepts any pool
// pin, since plain digital I/O, PWM, and interrupt-driven input all work
// fine on ADC-capable pins too.
inline bool isRoleValidForPin(uint8_t pin, PinRoleType role) {
    if (!isPinInAssignablePool(pin)) return false;
    if (role == PinRoleType::kAnalog) return pin <= 10;
    return true;
}

// allRoles is an [11]-element array indexed the same as kAssignablePins,
// i.e. allRoles[i] is the role currently assigned to kAssignablePins[i].

inline uint8_t countPinsWithRole(const PinRoleType allRoles[11], PinRoleType role) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < 11; i++) {
        if (allRoles[i] == role) count++;
    }
    return count;
}

// Returns kAssignablePins[i] for the n-th (0-based) pin whose role is
// `role`, in kAssignablePins order, or kNoPin if fewer than n+1 exist.
inline uint8_t nthPinWithRole(const PinRoleType allRoles[11], PinRoleType role, uint8_t n) {
    for (uint8_t i = 0; i < 11; i++) {
        if (allRoles[i] != role) continue;
        if (n == 0) return kAssignablePins[i];
        n--;
    }
    return kNoPin;
}

// Index of `pin` within kAssignablePins, or 11 (out of bounds) if not found.
inline uint8_t pinIndexOf(uint8_t pin) {
    for (uint8_t i = 0; i < 11; i++) {
        if (kAssignablePins[i] == pin) return i;
    }
    return 11;
}

// String encoding for role types -- the config.txt / web JSON representation
// (e.g. "pin1role=analog"). Shared by config parsing/persistence
// (config.cpp, config_file.h), the web API (web_api.h), and the config POST
// handler (wifi_ap.cpp) so the scheme lives in exactly one place.
inline const char *pinRoleToString(PinRoleType role) {
    switch (role) {
        case PinRoleType::kDout:   return "dout";
        case PinRoleType::kAnalog: return "analog";
        case PinRoleType::kPpm:    return "ppm";
        case PinRoleType::kServo:  return "servo";
        case PinRoleType::kHall:   return "hall";
        default:                   return "dout";
    }
}

// Parses a pinRoleToString()-encoded string back to a PinRoleType. Returns
// false (leaving *outRole untouched) if `s` doesn't match any known role.
inline bool pinRoleFromString(const char *s, PinRoleType *outRole) {
    if (strcmp(s, "dout") == 0)   { *outRole = PinRoleType::kDout;   return true; }
    if (strcmp(s, "analog") == 0) { *outRole = PinRoleType::kAnalog; return true; }
    if (strcmp(s, "ppm") == 0)    { *outRole = PinRoleType::kPpm;    return true; }
    if (strcmp(s, "servo") == 0)  { *outRole = PinRoleType::kServo;  return true; }
    if (strcmp(s, "hall") == 0)   { *outRole = PinRoleType::kHall;   return true; }
    return false;
}

struct PinRoleValidationResult {
    bool ok;
    const char *reason;  // static string literal; nullptr when ok
};

// Full check for setting `pin`'s role to newRole: electrical validity for
// that specific pin, plus (for Servo/PPM/Hall, which have hardware-forced
// count ceilings) confirming the change wouldn't push that role's count
// past its ceiling. Counts newRole occurrences in `allRoles` EXCLUDING
// `pin`'s own current entry, so re-selecting a pin's own current role is
// always valid. `allRoles` is indexed the same as kAssignablePins.
inline PinRoleValidationResult validateRoleChange(uint8_t pin, PinRoleType newRole,
                                                  const PinRoleType allRoles[11]) {
    uint8_t pinIndex = pinIndexOf(pin);
    if (pinIndex >= 11) {
        return {false, "not one of the board's reassignable pins"};
    }
    if (!isRoleValidForPin(pin, newRole)) {
        return {false, "not ADC-capable"};  // only isRoleValidForPin() failure case today
    }

    uint8_t othersWithRole = 0;
    for (uint8_t i = 0; i < 11; i++) {
        if (i == pinIndex) continue;
        if (allRoles[i] == newRole) othersWithRole++;
    }

    switch (newRole) {
        case PinRoleType::kServo:
            if (othersWithRole >= kMaxServoChannels)
                return {false, "servo channels full (8/8)"};
            break;
        case PinRoleType::kPpm:
            if (othersWithRole >= kMaxPpmPins)
                return {false, "PPM input already assigned to another pin"};
            break;
        case PinRoleType::kHall:
            if (othersWithRole >= kMaxHallPins)
                return {false, "hall sensor input already assigned to another pin"};
            break;
        default:
            break;  // kDout, kAnalog: no count ceiling beyond electrical validity
    }
    return {true, nullptr};
}
