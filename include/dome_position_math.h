// dome_position_math.h
// Pure logic helpers for the RoboClaw dome drive.
//
// This file contains two categories of functions:
//
//   1. Zero-dependency math (position conversion, calibration arithmetic).
//      These have no hardware or Arduino dependencies at all.
//
//   2. EEPROM helpers (dome_load_calibration / dome_save_calibration).
//      These call the global EEPROM object, which is either the real Arduino
//      EEPROM (embedded build) or MockEEPROM from arduino_mock.h (native
//      tests).  Callers must ensure EEPROM is visible before including this
//      header — in practice that means:
//        Embedded:    #include <EEPROM.h>  (pulled in via controller.h)
//        Native test: #include "arduino_mock.h"  (already first in test files)
//
//   3. State-machine decision functions (homing, calibration, obstruction).
//      These are also zero-dependency and are the testable core of the drive
//      logic that was previously hidden inside hardware-touching methods.
//
// All functions are inline so the header can be included in multiple TUs
// without linker conflicts.

#pragma once

#include <stdint.h>
#include <math.h>

// EEPROM is not included here — see note above.
// In embedded builds drive_config.h (included before this header) already
// pulled in <EEPROM.h> transitively via controller.h.
#include "drive_config.h"

/**
 * Compute the pause duration (ms) for a dome sequence-pause command.
 *
 * @param argSeconds  seconds from the serial arg; <= 0 means "use default"
 * @param defaultMs   value to use when argSeconds <= 0
 * @param maxMs       upper cap applied to both the default and explicit arg
 * @return            duration in milliseconds, clamped to [0, maxMs]
 */
static inline uint32_t dome_sequence_pause_duration_ms(int      argSeconds,
                                                       uint32_t defaultMs,
                                                       uint32_t maxMs) {
    uint32_t ms = (argSeconds <= 0) ? defaultMs
                                     : (uint32_t)argSeconds * 1000u;
    if (ms > maxMs) ms = maxMs;
    return ms;
}

/**
 * Normalize any integer to the 0–359 range (handles negative values).
 */
static inline int dome_normalize_degrees(int degrees) {
    degrees = degrees % 360;
    if (degrees < 0) degrees += 360;
    return degrees;
}

/**
 * Signed shortest-path angular error from 'current' to 'target'.
 *
 * Returns a value in [-180, +180] degrees:
 *   positive = clockwise rotation needed
 *   negative = counterclockwise rotation needed
 */
static inline int dome_angular_error(int target, int current) {
    int err = dome_normalize_degrees(target) - dome_normalize_degrees(current);
    if (err > 180)  err -= 360;
    if (err < -180) err += 360;
    return err;
}

/**
 * Convert a joystick (lx, ly) position to a dome heading in degrees.
 *
 * Axis convention (matches JoystickController):
 *   lx: negative = full left (-128), positive = full right (+127)
 *   ly: positive = pushed toward user (down/back); negate before passing if
 *       you want physical "push forward" to map to 0°.
 *
 * The returned angle maps intuitively to dome front:
 *   0° = forward, 90° = right, 180° = back, 270° = left
 *
 * @param lx        Horizontal stick axis (-128 to +127).
 * @param ly        Vertical stick axis   (-128 to +127).
 * @param deadband  Minimum stick magnitude (0.0–1.0) before an angle is
 *                  returned.  Defaults to 0.2 (matches DomeDrive deadband).
 * @return          Dome heading 0–359, or -1 if the stick is within the
 *                  deadband (no meaningful direction).
 */
static inline int dome_stick_to_angle(int lx, int ly,
                                      float deadband = 0.2f) {
    float x = (float)lx / 128.0f;
    float y = (float)ly / 128.0f;
    float mag = sqrtf(x * x + y * y);
    if (mag < deadband) return -1;
    // atan2f(x, y): x-right/y-forward convention gives 0° = forward.
    float angle = atan2f(x, y) * (180.0f / (float)M_PI);
    return dome_normalize_degrees((int)angle);
}

/**
 * Convert a signed encoder-tick offset (enc - homeEncoderTick) to a
 * dome angle in degrees.
 *
 * @param relTicks      Encoder ticks since the last hall-sensor trigger.
 *                      Positive = clockwise (matches positive motor command).
 * @param ticksPerRev   Motor encoder ticks per one complete dome revolution.
 *                      Must be > 0; returns 0 if zero is passed.
 * @param frontOffset   Degrees from the hall-sensor trigger position to the
 *                      dome "front".  Subtracted from the raw angle so that
 *                      the returned value is 0 when the dome faces forward.
 * @return              Dome angle in degrees, 0–359, where 0 = front.
 */
static inline int dome_encoder_to_degrees(int32_t relTicks,
                                          int32_t ticksPerRev,
                                          int     frontOffset) {
    if (ticksPerRev == 0) return 0;
    long degrees = ((long)relTicks * 360L) / (long)ticksPerRev;
    degrees -= (long)frontOffset;
    return dome_normalize_degrees((int)degrees);
}

/**
 * Compute ticks-per-dome-revolution from raw calibration measurements.
 *
 * During calibration the firmware counts the encoder ticks accumulated over
 * `nRotations` complete dome revolutions (hall-sensor triggers).  This
 * function divides to get the average ticks per revolution.
 *
 * @param totalEncoderTicks  enc(end) - enc(start), must be > 0.
 * @param nRotations         Number of complete revolutions recorded, > 0.
 * @return                   Ticks per dome revolution, or 0 on invalid input.
 */
static inline int32_t dome_compute_ticks_per_rev(int32_t totalEncoderTicks,
                                                  int     nRotations) {
    if (nRotations <= 0 || totalEncoderTicks <= 0) return 0;
    return totalEncoderTicks / (int32_t)nRotations;
}

/**
 * Compute the proportional motor speed fraction for closed-loop dome movement.
 *
 * Uses a cruise-then-decelerate curve:
 *   |error| <= fudge                    → 0.0  (dead zone, hold position)
 *   fudge < |error| <= fudge+decelZone  → linear ramp: speedMin … speedTarget
 *   |error| > fudge+decelZone           → speedTarget (cruise at full speed)
 *
 * This keeps the dome moving briskly most of the time and only slows for the
 * final approach, preventing the sluggishness of a pure proportional curve.
 *
 * @param error        Signed angular error in degrees [-180, +180].
 *                     Positive = CW rotation needed → positive speed.
 * @param fudge        Dead-zone half-width in degrees (>= 0).
 * @param speedMin     Minimum speed % [0, 100] at the edge of the decel zone.
 * @param speedTarget  Maximum / cruise speed % [0, 100].
 * @param decelZone    Distance in degrees over which speed ramps from speedMin
 *                     to speedTarget (default 20°).
 * @return             Motor speed fraction in [-1.0, +1.0]; sign matches error.
 */
static inline float dome_abs_stick_speed(int error, int fudge,
                                         int speedMin, int speedTarget,
                                         int decelZone = 20) {
    int absErr = error < 0 ? -error : error;
    if (absErr <= fudge) return 0.0f;

    float speed;
    if (absErr > fudge + decelZone) {
        // Cruising — run at full commanded speed.
        speed = (float)speedTarget / 100.0f;
    } else {
        // Deceleration zone — linear ramp from speedMin to speedTarget.
        int pct = speedMin + (absErr - fudge) * (speedTarget - speedMin) / decelZone;
        if (pct > speedTarget) pct = speedTarget;
        speed = (float)pct / 100.0f;
    }
    return error < 0 ? -speed : speed;
}

// ===========================================================================
// State-machine decision functions
//
// These express the pure logic of each drive state.  They take all inputs as
// parameters and return a result struct — no member variables, no hardware
// calls, no side effects.  The hardware-touching methods in
// DomeDriveRoboClaw are thin wrappers that feed real sensor readings in and
// act on the returned result.
// ===========================================================================

// ---------------------------------------------------------------------------
// Homing state
// ---------------------------------------------------------------------------

/** Result of one homing-state step. */
enum DomeHomingAction {
    kHomingContinue,   ///< Still searching — keep the motor spinning.
    kHomingComplete,   ///< Hall sensor fired — homing done.
    kHomingTimeout,    ///< Time limit expired without finding the sensor.
};

/**
 * Decide what to do during one animate() cycle while homing.
 *
 * @param hallFired   True if the hall sensor triggered this cycle.
 * @param elapsedMs   Milliseconds since the homing sweep started.
 * @param timeoutMs   Maximum allowed homing duration before giving up.
 */
static inline DomeHomingAction dome_homing_step(bool     hallFired,
                                                uint32_t elapsedMs,
                                                uint32_t timeoutMs) {
    if (hallFired)            return kHomingComplete;
    if (elapsedMs > timeoutMs) return kHomingTimeout;
    return kHomingContinue;
}

// ---------------------------------------------------------------------------
// Calibration state
// ---------------------------------------------------------------------------

/** What happened when a hall-sensor trigger was processed during calibration. */
enum DomeCalibrationAction {
    kCalibrationSetReference, ///< First trigger: snapshot start encoder tick.
    kCalibrationContinue,     ///< Intermediate trigger: keep spinning.
    kCalibrationComplete,     ///< Final trigger: ratio computed successfully.
    kCalibrationError,        ///< Final trigger: tick count was invalid.
};

struct DomeCalibrationResult {
    DomeCalibrationAction action;
    int32_t tpr; ///< Ticks-per-dome-rev; non-zero only when kCalibrationComplete.
};

/**
 * Process one hall-sensor trigger during calibration.
 *
 * Call this every time the hall sensor fires while fState == kStateCalibrating.
 * The caller is responsible for snapshotting the start encoder tick when
 * action == kCalibrationSetReference and for computing totalTicksSinceStart
 * (enc_current - enc_start) before calling on subsequent triggers.
 *
 * @param newTriggerCount      Trigger count *after* incrementing (1-based).
 * @param totalTicksSinceStart enc(now) - enc(start); only used on the final
 *                             trigger (newTriggerCount == nRotations + 1).
 * @param nRotations           Target number of complete dome revolutions.
 */
static inline DomeCalibrationResult dome_calibration_trigger(
        int     newTriggerCount,
        int32_t totalTicksSinceStart,
        int     nRotations) {
    if (newTriggerCount == 1)
        return { kCalibrationSetReference, 0 };

    if (newTriggerCount == nRotations + 1) {
        int32_t tpr = dome_compute_ticks_per_rev(totalTicksSinceStart, nRotations);
        if (tpr <= 0)
            return { kCalibrationError, 0 };
        return { kCalibrationComplete, tpr };
    }

    return { kCalibrationContinue, 0 };
}

// ---------------------------------------------------------------------------
// Obstruction detection
// ---------------------------------------------------------------------------

struct DomeObstructionResult {
    bool startTimer;          ///< Start the stall timer now (was not running).
    bool declareObstruction;  ///< Timeout exceeded — stop and lock out auto.
    bool clearTimer;          ///< Reset the stall timer (motor moving or idle).
};

/**
 * Decide whether a stall is starting, ongoing, declared, or absent.
 *
 * Called every animate() cycle while the motor is in kStateHomed.
 *
 * @param commandedSpeed      Fraction of QPPS commanded (-1.0 to 1.0).
 *                            Values below 0.05 in magnitude = not commanded.
 * @param actualEncoderSpeed  Signed encoder ticks/sec read from RoboClaw.
 *                            Values below 10 in magnitude = considered stalled.
 * @param timerActive         True if the stall timer is currently running.
 * @param stallElapsedMs      Milliseconds since the stall timer started.
 *                            Only meaningful when timerActive == true.
 * @param timeoutMs           Stall duration required to declare obstruction.
 */
static inline DomeObstructionResult dome_obstruction_check(
        float    commandedSpeed,
        int32_t  actualEncoderSpeed,
        bool     timerActive,
        uint32_t stallElapsedMs,
        uint32_t timeoutMs) {
    // Motor not commanded — nothing to watch.
    if (commandedSpeed < 0.0f) commandedSpeed = -commandedSpeed; // fabsf
    if (commandedSpeed < 0.05f)
        return { false, false, true };

    // Motor is actually moving — clear any stall timer.
    int32_t absActual = actualEncoderSpeed < 0 ? -actualEncoderSpeed : actualEncoderSpeed;
    if (absActual >= 10)
        return { false, false, true };

    // Stalled: encoder barely moving despite a commanded speed.
    if (!timerActive)
        return { true, false, false };    // start timer

    if (stallElapsedMs > timeoutMs)
        return { false, true, false };    // declare obstruction

    return { false, false, false };       // still in window, keep waiting
}

// ===========================================================================
// EEPROM calibration persistence
//
// Callers must ensure the EEPROM global (real or mock) is visible before
// including this header (see file-level note).
// ===========================================================================

/**
 * Save the motor-to-dome calibration ratio to EEPROM.
 *
 * Layout (starting at DOME_ROBOCLAW_EEPROM_ADDR):
 *   [+0] 'R'  [+1] 'C'  [+2] '0'  [+3] '1'   4-byte signature
 *   [+4..+7]  int32_t   ticksPerDomeRev         (little-endian via put)
 *
 * @param tpr  Ticks-per-dome-revolution from calibration.  Must be > 0.
 */
static inline void dome_save_calibration(int32_t tpr) {
    int addr = DOME_ROBOCLAW_EEPROM_ADDR;
    EEPROM.write(addr + 0, DOME_ROBOCLAW_EEPROM_SIG0);
    EEPROM.write(addr + 1, DOME_ROBOCLAW_EEPROM_SIG1);
    EEPROM.write(addr + 2, DOME_ROBOCLAW_EEPROM_SIG2);
    EEPROM.write(addr + 3, DOME_ROBOCLAW_EEPROM_SIG3);
    EEPROM.put(addr + 4, tpr);
    EEPROM.commit();  // required on ESP32 — flushes RAM buffer to NVS flash
}

/**
 * Load the motor-to-dome calibration ratio from EEPROM.
 *
 * Returns the stored tpr if the 4-byte signature matches and the value is
 * positive; returns 0 if the signature is absent or the stored value is
 * invalid (indicating calibration has not been performed yet).
 */
static inline int32_t dome_load_calibration() {
    int addr = DOME_ROBOCLAW_EEPROM_ADDR;
    if (EEPROM.read(addr + 0) != DOME_ROBOCLAW_EEPROM_SIG0 ||
        EEPROM.read(addr + 1) != DOME_ROBOCLAW_EEPROM_SIG1 ||
        EEPROM.read(addr + 2) != DOME_ROBOCLAW_EEPROM_SIG2 ||
        EEPROM.read(addr + 3) != DOME_ROBOCLAW_EEPROM_SIG3)
        return 0;
    int32_t tpr = 0;
    EEPROM.get(addr + 4, tpr);
    return tpr > 0 ? tpr : 0;
}
