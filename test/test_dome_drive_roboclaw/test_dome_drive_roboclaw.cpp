// test_dome_drive_roboclaw.cpp
// Unit tests for the pure position-math functions used by the RoboClaw dome
// drive.  All functions under test live in include/dome_position_math.h and
// have zero hardware or Reeltwo dependencies, so they run cleanly on the
// native PlatformIO test environment.
//
// Tests cover:
//   dome_normalize_degrees()       — wraparound and negative handling
//   dome_encoder_to_degrees()      — tick→angle conversion, front offset
//   dome_compute_ticks_per_rev()   — calibration ratio calculation
//   Integration scenarios          — full calibration + positioning pipeline
//   dome_homing_step()             — state-machine homing decisions
//   dome_calibration_trigger()     — state-machine calibration decisions
//   dome_obstruction_check()       — stall detection logic
//   dome_save/load_calibration()   — EEPROM round-trip
//   DomeDriveRoboClaw::stop()      — auto-motion state clearing (e-stop bug)
//   DomeDriveRoboClaw::setEnable() — motor lock-out

#include "arduino_mock.h"
#include "dome_position_math.h"
#include "drive_config.h"
#include "dome_drive_roboclaw.h"
// Pull in the implementation so the linker can find DomeDriveRoboClaw methods.
// test_build_src=no means src/ files are not compiled automatically.
#include "../../src/dome_drive_roboclaw.cpp"
#include <unity.h>

void setUp(void)    { memset(EEPROM.data, 0, sizeof(EEPROM.data)); }
void tearDown(void) {}

// ---- dome_normalize_degrees() -----------------------------------------------

void test_normalize_zero() {
    TEST_ASSERT_EQUAL(0, dome_normalize_degrees(0));
}

void test_normalize_359() {
    TEST_ASSERT_EQUAL(359, dome_normalize_degrees(359));
}

void test_normalize_360_wraps_to_zero() {
    TEST_ASSERT_EQUAL(0, dome_normalize_degrees(360));
}

void test_normalize_361() {
    TEST_ASSERT_EQUAL(1, dome_normalize_degrees(361));
}

void test_normalize_720_wraps_to_zero() {
    TEST_ASSERT_EQUAL(0, dome_normalize_degrees(720));
}

void test_normalize_negative_one() {
    // -1° wraps to 359°.
    TEST_ASSERT_EQUAL(359, dome_normalize_degrees(-1));
}

void test_normalize_negative_90() {
    TEST_ASSERT_EQUAL(270, dome_normalize_degrees(-90));
}

void test_normalize_negative_360() {
    TEST_ASSERT_EQUAL(0, dome_normalize_degrees(-360));
}

void test_normalize_negative_361() {
    TEST_ASSERT_EQUAL(359, dome_normalize_degrees(-361));
}

// ---- dome_compute_ticks_per_rev() -------------------------------------------

void test_calibration_basic() {
    // 10 dome revolutions, 12000 encoder ticks → 1200 ticks/rev.
    int32_t tpr = dome_compute_ticks_per_rev(12000, 10);
    TEST_ASSERT_EQUAL(1200, tpr);
}

void test_calibration_non_round() {
    // 10 revolutions, 12050 ticks → integer division → 1205 ticks/rev.
    int32_t tpr = dome_compute_ticks_per_rev(12050, 10);
    TEST_ASSERT_EQUAL(1205, tpr);
}

void test_calibration_single_rotation() {
    int32_t tpr = dome_compute_ticks_per_rev(1200, 1);
    TEST_ASSERT_EQUAL(1200, tpr);
}

void test_calibration_zero_ticks_returns_zero() {
    // Invalid input: no ticks counted.
    TEST_ASSERT_EQUAL(0, dome_compute_ticks_per_rev(0, 10));
}

void test_calibration_zero_rotations_returns_zero() {
    // Invalid input: division by zero guard.
    TEST_ASSERT_EQUAL(0, dome_compute_ticks_per_rev(12000, 0));
}

void test_calibration_negative_ticks_returns_zero() {
    // Encoder counted backwards — invalid calibration.
    TEST_ASSERT_EQUAL(0, dome_compute_ticks_per_rev(-12000, 10));
}

void test_calibration_large_gear_ratio() {
    // Motor turns 15× per dome revolution, CPR=1200 → 18000 ticks/dome-rev.
    int32_t tpr = dome_compute_ticks_per_rev(180000, 10);
    TEST_ASSERT_EQUAL(18000, tpr);
}

// ---- dome_encoder_to_degrees() — no front offset ----------------------------

void test_encoder_to_degrees_zero_ticks() {
    // At the hall-sensor position with no offset: 0°.
    TEST_ASSERT_EQUAL(0, dome_encoder_to_degrees(0, 1200, 0));
}

void test_encoder_to_degrees_quarter_turn() {
    // 300 ticks (1/4 of 1200) → 90°.
    TEST_ASSERT_EQUAL(90, dome_encoder_to_degrees(300, 1200, 0));
}

void test_encoder_to_degrees_half_turn() {
    TEST_ASSERT_EQUAL(180, dome_encoder_to_degrees(600, 1200, 0));
}

void test_encoder_to_degrees_three_quarter_turn() {
    TEST_ASSERT_EQUAL(270, dome_encoder_to_degrees(900, 1200, 0));
}

void test_encoder_to_degrees_full_turn_wraps() {
    // 1200 ticks = one full revolution → back to 0°.
    TEST_ASSERT_EQUAL(0, dome_encoder_to_degrees(1200, 1200, 0));
}

void test_encoder_to_degrees_one_and_quarter_turn() {
    // 1500 ticks = 1.25 revolutions → 90°.
    TEST_ASSERT_EQUAL(90, dome_encoder_to_degrees(1500, 1200, 0));
}

void test_encoder_to_degrees_negative_quarter_turn() {
    // -300 ticks (dome spun CCW 90°) → 270°.
    TEST_ASSERT_EQUAL(270, dome_encoder_to_degrees(-300, 1200, 0));
}

void test_encoder_to_degrees_negative_half_turn() {
    TEST_ASSERT_EQUAL(180, dome_encoder_to_degrees(-600, 1200, 0));
}

void test_encoder_to_degrees_zero_ticks_per_rev_returns_zero() {
    // Guard against division by zero.
    TEST_ASSERT_EQUAL(0, dome_encoder_to_degrees(1000, 0, 0));
}

// ---- dome_encoder_to_degrees() — with front offset --------------------------

void test_front_offset_shifts_zero_to_offset() {
    // At the hall position (relTicks=0) with frontOffset=90:
    // rawAngle = 0 - 90 = -90 → normalize → 270°.
    // 90 divides evenly into 1200 CPR (90 * 1200 / 360 = 300 ticks exactly).
    TEST_ASSERT_EQUAL(270, dome_encoder_to_degrees(0, 1200, 90));
}

void test_front_offset_at_front_reads_zero() {
    // Dome moved 90° past the hall sensor → now facing front.
    // relTicks = 90 * 1200 / 360 = 300 (exact — no truncation error).
    int32_t frontTicks = (int32_t)(90 * 1200 / 360);  // 300 exactly
    TEST_ASSERT_EQUAL(0, dome_encoder_to_degrees(frontTicks, 1200, 90));
}

void test_front_offset_90_degrees_past_front() {
    // 90° front offset + 90° past front = 180° encoder travel.
    // 180 * 1200 / 360 = 600 ticks (exact).
    int32_t ticks = (int32_t)((90 + 90) * 1200 / 360);  // 600
    TEST_ASSERT_EQUAL(90, dome_encoder_to_degrees(ticks, 1200, 90));
}

void test_front_offset_zero_means_hall_is_front() {
    // With frontOffset=0, the hall position IS the front.
    TEST_ASSERT_EQUAL(0, dome_encoder_to_degrees(0, 1200, 0));
    TEST_ASSERT_EQUAL(90, dome_encoder_to_degrees(300, 1200, 0));
}

void test_front_offset_360_same_as_zero() {
    // A 360° offset is the same as 0° (normalize handles it).
    TEST_ASSERT_EQUAL(dome_encoder_to_degrees(300, 1200, 0),
                      dome_encoder_to_degrees(300, 1200, 360));
}

// ---- Integration: calibration → positioning pipeline -----------------------

void test_full_pipeline_basic() {
    // Simulate: 10 dome revolutions measured as 12000 encoder ticks.
    // Then command the dome to 90° (quarter turn from front, front=hall).
    int32_t tpr = dome_compute_ticks_per_rev(12000, 10);
    TEST_ASSERT_EQUAL(1200, tpr);

    // After 300 encoder ticks (1/4 dome rev), dome should read 90°.
    TEST_ASSERT_EQUAL(90, dome_encoder_to_degrees(300, tpr, 0));
}

void test_full_pipeline_with_front_offset() {
    // Motor ratio 10:1, CPR=1200 → 12000 ticks/dome-rev.
    // Use 90° offset (exact: 90 * 12000 / 360 = 3000 ticks, no truncation).
    int32_t tpr        = dome_compute_ticks_per_rev(120000, 10);  // 12000
    int     frontOffset = 90;

    // At the hall position, dome-relative-to-front = 360-90 = 270°.
    TEST_ASSERT_EQUAL(270, dome_encoder_to_degrees(0, tpr, frontOffset));

    // Dome moved to face front: 90°/360° * 12000 = 3000 ticks (exact).
    int32_t frontTicks = (int32_t)(90L * 12000L / 360L);  // 3000
    TEST_ASSERT_EQUAL(0, dome_encoder_to_degrees(frontTicks, tpr, frontOffset));

    // Dome turned 180° past front: (90+180)/360 * 12000 = 9000 ticks (exact).
    int32_t backTicks = (int32_t)(270L * 12000L / 360L);  // 9000
    TEST_ASSERT_EQUAL(180, dome_encoder_to_degrees(backTicks, tpr, frontOffset));
}

void test_full_pipeline_negative_motion() {
    // Dome reversed (CCW): encoder goes negative.
    int32_t tpr = dome_compute_ticks_per_rev(12000, 10);  // 1200

    // -300 ticks = dome moved 90° CCW from hall = 270° when front=hall.
    TEST_ASSERT_EQUAL(270, dome_encoder_to_degrees(-300, tpr, 0));
}

// ---- Calibration debounce / hall trigger counting --------------------------

void test_calibration_requires_positive_ticks() {
    // If the encoder went the wrong direction, calibration should fail.
    TEST_ASSERT_EQUAL(0, dome_compute_ticks_per_rev(-5000, 10));
}

void test_calibration_minimum_one_rotation() {
    // Edge case: exactly 1 rotation.
    int32_t tpr = dome_compute_ticks_per_rev(1200, 1);
    TEST_ASSERT_EQUAL(1200, tpr);
    // Confirm degrees conversion works.
    TEST_ASSERT_EQUAL(0,   dome_encoder_to_degrees(0, tpr, 0));
    TEST_ASSERT_EQUAL(90,  dome_encoder_to_degrees(300, tpr, 0));
    TEST_ASSERT_EQUAL(180, dome_encoder_to_degrees(600, tpr, 0));
}

// ---- dome_angular_error() ----------------------------------------------------

void test_angular_error_same_angle() {
    // No rotation needed when target == current.
    TEST_ASSERT_EQUAL(0, dome_angular_error(90, 90));
}

void test_angular_error_clockwise_short() {
    // 10° clockwise from 80 → 90.
    TEST_ASSERT_EQUAL(10, dome_angular_error(90, 80));
}

void test_angular_error_counterclockwise_short() {
    // 10° CCW from 90 → 80.
    TEST_ASSERT_EQUAL(-10, dome_angular_error(80, 90));
}

void test_angular_error_at_180_boundary() {
    // Exactly 180° — could be either direction; result is +180 per the formula.
    TEST_ASSERT_EQUAL(180, dome_angular_error(180, 0));
}

void test_angular_error_chooses_short_path_over_180() {
    // From 350° to 10° = +20° CW (not −340° CCW).
    TEST_ASSERT_EQUAL(20, dome_angular_error(10, 350));
}

void test_angular_error_chooses_ccw_short_path() {
    // From 10° to 350° = −20° CCW (not +340° CW).
    TEST_ASSERT_EQUAL(-20, dome_angular_error(350, 10));
}

void test_angular_error_full_circle_is_zero() {
    // 360° apart → same position → 0 error.
    TEST_ASSERT_EQUAL(0, dome_angular_error(360, 0));
}

// ---- dome_stick_to_angle() ---------------------------------------------------

void test_stick_to_angle_forward() {
    // JoystickController: ly positive = toward user (back). Pass -ly so that
    // physical "push forward" (ly = -128 raw → -(-128) = +128 passed) → 0°.
    TEST_ASSERT_EQUAL(0, dome_stick_to_angle(0, 128));
}

void test_stick_to_angle_right() {
    // Full right (lx=+127, ly=0) → 90°.
    int angle = dome_stick_to_angle(127, 0);
    // Allow ±2° for floating-point rounding.
    TEST_ASSERT_INT_WITHIN(2, 90, angle);
}

void test_stick_to_angle_back() {
    // Full back: raw ly=+127, after negation passed as -127 → 180°.
    int angle = dome_stick_to_angle(0, -127);
    TEST_ASSERT_INT_WITHIN(2, 180, angle);
}

void test_stick_to_angle_left() {
    // Full left (lx=-128, ly=0) → 270°.
    int angle = dome_stick_to_angle(-128, 0);
    TEST_ASSERT_INT_WITHIN(2, 270, angle);
}

void test_stick_to_angle_deadband_returns_minus1() {
    // Stick near center — within default 0.2 deadband → -1.
    TEST_ASSERT_EQUAL(-1, dome_stick_to_angle(0, 0));
    TEST_ASSERT_EQUAL(-1, dome_stick_to_angle(10, 5));
}

void test_stick_to_angle_deadband_boundary() {
    // At exactly the boundary magnitude for deadband=0.2:
    // Need |v| >= 0.2 where v = sqrt(x^2+y^2), x=lx/128, y=ly/128.
    // 0.2 * 128 ≈ 25.6 → magnitude of 26/128 ≈ 0.203 — should return a valid angle.
    int angle = dome_stick_to_angle(26, 0);
    TEST_ASSERT_NOT_EQUAL(-1, angle);
}

void test_stick_to_angle_diagonal_forward_right() {
    // Roughly 45°: equal lx and ly.
    int angle = dome_stick_to_angle(100, 100);
    TEST_ASSERT_INT_WITHIN(3, 45, angle);
}

// ---- dome_abs_stick_speed() --------------------------------------------------
// Fudge=10, speedMin=5, speedTarget=100, decelZone=20 (default) unless noted.

void test_abs_stick_speed_within_fudge_returns_zero() {
    // |error| <= fudge → dead zone, motor should stop.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dome_abs_stick_speed(0,  10, 5, 100));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dome_abs_stick_speed(10, 10, 5, 100));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dome_abs_stick_speed(-10, 10, 5, 100));
}

void test_abs_stick_speed_just_outside_fudge_gives_min_speed() {
    // |error| = fudge+1, inside decel zone → near speedMin.
    // pct = 5 + 1 * (100-5) / 20 = 5 + 4 = 9 → 0.09f
    float s = dome_abs_stick_speed(11, 10, 5, 100);
    TEST_ASSERT_EQUAL_FLOAT(0.09f, s);
}

void test_abs_stick_speed_cruise_zone_gives_target_speed() {
    // |error| > fudge + decelZone (10+20=30) → full cruise speed.
    TEST_ASSERT_EQUAL_FLOAT(1.0f, dome_abs_stick_speed(31, 10, 5, 100));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, dome_abs_stick_speed(90, 10, 5, 100));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, dome_abs_stick_speed(180, 10, 5, 100));
}

void test_abs_stick_speed_at_decel_zone_boundary_gives_target_speed() {
    // |error| = fudge + decelZone exactly → just enters decel, pct = speedTarget.
    // pct = 5 + 20 * 95 / 20 = 5 + 95 = 100 → 1.0f
    float s = dome_abs_stick_speed(30, 10, 5, 100);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, s);
}

void test_abs_stick_speed_negative_error_gives_negative_speed() {
    // Negative error → negative (CCW) motor command, same magnitude.
    float pos = dome_abs_stick_speed( 90, 10, 5, 100);
    float neg = dome_abs_stick_speed(-90, 10, 5, 100);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -pos, neg);
}

void test_abs_stick_speed_capped_at_target() {
    // Error beyond cruise zone still gives target speed, not more.
    float s = dome_abs_stick_speed(200, 0, 5, 100);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, s);
}

void test_abs_stick_speed_custom_decel_zone() {
    // decelZone=10: within 10° of fudge boundary → ramp; beyond → cruise.
    TEST_ASSERT_EQUAL_FLOAT(1.0f, dome_abs_stick_speed(21, 10, 5, 100, 10)); // cruise
    // |error|=15, pct = 5 + (15-10)*95/10 = 5+47 = 52 → 0.52f
    float s = dome_abs_stick_speed(15, 10, 5, 100, 10);
    TEST_ASSERT_EQUAL_FLOAT(0.52f, s);
}

// ---- dome_homing_step() -------------------------------------------------------

void test_homing_hall_fires_returns_complete() {
    TEST_ASSERT_EQUAL(kHomingComplete,
        dome_homing_step(true, 0, 15000));
}

void test_homing_no_hall_not_timed_out_returns_continue() {
    TEST_ASSERT_EQUAL(kHomingContinue,
        dome_homing_step(false, 5000, 15000));
}

void test_homing_elapsed_equals_timeout_still_continues() {
    // Boundary: elapsedMs == timeoutMs — the check is >, not >=, so still continuing.
    TEST_ASSERT_EQUAL(kHomingContinue,
        dome_homing_step(false, 15000, 15000));
}

void test_homing_elapsed_exceeds_timeout_returns_timeout() {
    TEST_ASSERT_EQUAL(kHomingTimeout,
        dome_homing_step(false, 15001, 15000));
}

void test_homing_hall_fires_overrides_timeout() {
    // If the hall fires at the same cycle the timeout would trigger, hall wins.
    TEST_ASSERT_EQUAL(kHomingComplete,
        dome_homing_step(true, 99999, 15000));
}

void test_homing_zero_elapsed_returns_continue() {
    TEST_ASSERT_EQUAL(kHomingContinue,
        dome_homing_step(false, 0, 15000));
}

// ---- dome_calibration_trigger() ----------------------------------------------

void test_cal_trigger_first_returns_set_reference() {
    DomeCalibrationResult r = dome_calibration_trigger(1, 0, 10);
    TEST_ASSERT_EQUAL(kCalibrationSetReference, r.action);
    TEST_ASSERT_EQUAL(0, r.tpr);
}

void test_cal_trigger_second_returns_continue() {
    DomeCalibrationResult r = dome_calibration_trigger(2, 1200, 10);
    TEST_ASSERT_EQUAL(kCalibrationContinue, r.action);
    TEST_ASSERT_EQUAL(0, r.tpr);
}

void test_cal_trigger_mid_returns_continue() {
    DomeCalibrationResult r = dome_calibration_trigger(6, 6000, 10);
    TEST_ASSERT_EQUAL(kCalibrationContinue, r.action);
    TEST_ASSERT_EQUAL(0, r.tpr);
}

void test_cal_trigger_final_returns_complete_with_tpr() {
    // 10 rotations, 12000 ticks → tpr=1200; trigger count = nRotations+1 = 11.
    DomeCalibrationResult r = dome_calibration_trigger(11, 12000, 10);
    TEST_ASSERT_EQUAL(kCalibrationComplete, r.action);
    TEST_ASSERT_EQUAL(1200, r.tpr);
}

void test_cal_trigger_final_zero_ticks_returns_error() {
    DomeCalibrationResult r = dome_calibration_trigger(11, 0, 10);
    TEST_ASSERT_EQUAL(kCalibrationError, r.action);
    TEST_ASSERT_EQUAL(0, r.tpr);
}

void test_cal_trigger_final_negative_ticks_returns_error() {
    DomeCalibrationResult r = dome_calibration_trigger(11, -12000, 10);
    TEST_ASSERT_EQUAL(kCalibrationError, r.action);
    TEST_ASSERT_EQUAL(0, r.tpr);
}

void test_cal_trigger_final_non_round_tpr_uses_integer_division() {
    // 12050 ticks / 10 rotations = 1205 (integer division).
    DomeCalibrationResult r = dome_calibration_trigger(11, 12050, 10);
    TEST_ASSERT_EQUAL(kCalibrationComplete, r.action);
    TEST_ASSERT_EQUAL(1205, r.tpr);
}

// ---- dome_obstruction_check() ------------------------------------------------

// Helper: check that the result flags match expected values.
static void assert_obstruction(DomeObstructionResult r,
                                bool startTimer, bool declare, bool clearTimer) {
    TEST_ASSERT_EQUAL(startTimer, r.startTimer);
    TEST_ASSERT_EQUAL(declare,    r.declareObstruction);
    TEST_ASSERT_EQUAL(clearTimer, r.clearTimer);
}

void test_obstruction_not_commanded_clears_timer() {
    // Speed below 0.05 threshold — no stall concern.
    DomeObstructionResult r = dome_obstruction_check(0.0f, 0, false, 0, 500);
    assert_obstruction(r, false, false, true);
}

void test_obstruction_speed_just_below_threshold_clears_timer() {
    DomeObstructionResult r = dome_obstruction_check(0.04f, 0, false, 0, 500);
    assert_obstruction(r, false, false, true);
}

void test_obstruction_negative_speed_below_threshold_clears_timer() {
    DomeObstructionResult r = dome_obstruction_check(-0.04f, 0, false, 0, 500);
    assert_obstruction(r, false, false, true);
}

void test_obstruction_motor_moving_clears_timer() {
    // Commanded and encoder speed >= 10 — no stall.
    DomeObstructionResult r = dome_obstruction_check(0.5f, 50, false, 0, 500);
    assert_obstruction(r, false, false, true);
}

void test_obstruction_motor_moving_negative_speed_clears_timer() {
    DomeObstructionResult r = dome_obstruction_check(-0.5f, -50, false, 0, 500);
    assert_obstruction(r, false, false, true);
}

void test_obstruction_stall_starts_timer() {
    // Commanded but encoder barely moving (< 10) and timer not yet active.
    DomeObstructionResult r = dome_obstruction_check(0.5f, 5, false, 0, 500);
    assert_obstruction(r, true, false, false);
}

void test_obstruction_stall_within_timeout_waits() {
    // Timer running but not yet exceeded.
    DomeObstructionResult r = dome_obstruction_check(0.5f, 0, true, 400, 500);
    assert_obstruction(r, false, false, false);
}

void test_obstruction_stall_at_timeout_boundary_waits() {
    // elapsed == timeout: the check is >, not >=, so still waiting.
    DomeObstructionResult r = dome_obstruction_check(0.5f, 0, true, 500, 500);
    assert_obstruction(r, false, false, false);
}

void test_obstruction_stall_exceeds_timeout_declares() {
    DomeObstructionResult r = dome_obstruction_check(0.5f, 0, true, 501, 500);
    assert_obstruction(r, false, true, false);
}

void test_obstruction_encoder_exactly_10_clears_timer() {
    // Boundary: actualEncoderSpeed == 10 → considered moving (abs >= 10).
    DomeObstructionResult r = dome_obstruction_check(0.5f, 10, true, 400, 500);
    assert_obstruction(r, false, false, true);
}

void test_obstruction_encoder_9_is_stall() {
    // abs(9) < 10 → stall; timer not active → start it.
    DomeObstructionResult r = dome_obstruction_check(0.5f, 9, false, 0, 500);
    assert_obstruction(r, true, false, false);
}

// ---- dome_save_calibration() + dome_load_calibration() -----------------------

void test_eeprom_load_returns_zero_when_no_signature() {
    // Mock EEPROM is all zeros (setUp clears it) — no RC01 signature.
    TEST_ASSERT_EQUAL(0, dome_load_calibration());
}

void test_eeprom_load_returns_zero_when_wrong_signature() {
    int addr = DOME_ROBOCLAW_EEPROM_ADDR;
    EEPROM.data[addr + 0] = 'X';
    EEPROM.data[addr + 1] = 'X';
    EEPROM.data[addr + 2] = 'X';
    EEPROM.data[addr + 3] = 'X';
    TEST_ASSERT_EQUAL(0, dome_load_calibration());
}

void test_eeprom_round_trip_stores_and_retrieves_tpr() {
    dome_save_calibration(1200);
    TEST_ASSERT_EQUAL(1200, dome_load_calibration());
}

void test_eeprom_round_trip_large_tpr() {
    dome_save_calibration(18000);
    TEST_ASSERT_EQUAL(18000, dome_load_calibration());
}

void test_eeprom_save_writes_rc01_signature() {
    dome_save_calibration(1200);
    int addr = DOME_ROBOCLAW_EEPROM_ADDR;
    TEST_ASSERT_EQUAL(DOME_ROBOCLAW_EEPROM_SIG0, EEPROM.read(addr + 0));
    TEST_ASSERT_EQUAL(DOME_ROBOCLAW_EEPROM_SIG1, EEPROM.read(addr + 1));
    TEST_ASSERT_EQUAL(DOME_ROBOCLAW_EEPROM_SIG2, EEPROM.read(addr + 2));
    TEST_ASSERT_EQUAL(DOME_ROBOCLAW_EEPROM_SIG3, EEPROM.read(addr + 3));
}

void test_eeprom_load_returns_zero_when_tpr_is_zero() {
    // Write valid signature but store tpr=0.
    int addr = DOME_ROBOCLAW_EEPROM_ADDR;
    EEPROM.write(addr + 0, DOME_ROBOCLAW_EEPROM_SIG0);
    EEPROM.write(addr + 1, DOME_ROBOCLAW_EEPROM_SIG1);
    EEPROM.write(addr + 2, DOME_ROBOCLAW_EEPROM_SIG2);
    EEPROM.write(addr + 3, DOME_ROBOCLAW_EEPROM_SIG3);
    int32_t zero = 0;
    EEPROM.put(addr + 4, zero);
    TEST_ASSERT_EQUAL(0, dome_load_calibration());
}

void test_eeprom_overwrite_updates_value() {
    dome_save_calibration(1200);
    dome_save_calibration(1500);
    TEST_ASSERT_EQUAL(1500, dome_load_calibration());
}

// ---- dome_sequence_pause_duration_ms() --------------------------------------

void test_seqpause_zero_arg_uses_default() {
    // arg=0 means "no explicit arg provided" → use defaultMs.
    TEST_ASSERT_EQUAL_UINT32(
        30000u,
        dome_sequence_pause_duration_ms(0, 30000u, 300000u));
}

void test_seqpause_negative_arg_uses_default() {
    // Negative arg should be treated as "not provided".
    TEST_ASSERT_EQUAL_UINT32(
        30000u,
        dome_sequence_pause_duration_ms(-5, 30000u, 300000u));
}

void test_seqpause_positive_arg_converted_to_ms() {
    // 15 seconds → 15000 ms.
    TEST_ASSERT_EQUAL_UINT32(
        15000u,
        dome_sequence_pause_duration_ms(15, 30000u, 300000u));
}

void test_seqpause_arg_exceeding_cap_is_clamped() {
    // Huge arg clamps to maxMs.
    TEST_ASSERT_EQUAL_UINT32(
        300000u,
        dome_sequence_pause_duration_ms(9999, 30000u, 300000u));
}

void test_seqpause_default_exceeding_cap_is_clamped() {
    // Even the default is bounded by maxMs.
    TEST_ASSERT_EQUAL_UINT32(
        100u,
        dome_sequence_pause_duration_ms(0, 5000u, 100u));
}

void test_seqpause_arg_at_cap_boundary_stays() {
    // Exactly at the cap should not be altered.
    TEST_ASSERT_EQUAL_UINT32(
        300000u,
        dome_sequence_pause_duration_ms(300, 30000u, 300000u));
}

// ---- DomeDriveRoboClaw::stop() state-clearing (e-stop regression) ----------
// Bug: stop() only cleared kStateGoToAngle and kStateRandom. kStateHoming,
// kStateCalibrating, and kStateAbsoluteStick were NOT cleared, so their
// handlers restarted the motor on the very next animate() cycle after an
// e-stop was issued.
// Fix: stop() now clears every auto-motion state to kStateHomed.
//
// Additional bug: emergencyStop() / domeEmergencyStop() did not call
// setEnable(false), so driveFromJoystick() would immediately re-command the
// motor from the current stick position on the next animate() cycle.
// Fix: emergencyStop() and domeEmergencyStop() now call setEnable(false).
// The /api/resume handler calls setEnable(true) to re-enable movement.

// Helper: a freshly-constructed drive is in kStateManual (default).
static JoystickController sTestStick;

static DomeDriveRoboClaw make_drive() {
    return DomeDriveRoboClaw(128, 1, 5, sTestStick);
}

void test_stop_clears_kStateHoming() {
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateHoming);
    drive.stop();
    TEST_ASSERT_EQUAL_INT(DomeDriveRoboClaw::kStateHomed, drive.getStateForTest());
}

void test_stop_clears_kStateCalibrating() {
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateCalibrating);
    drive.stop();
    TEST_ASSERT_EQUAL_INT(DomeDriveRoboClaw::kStateHomed, drive.getStateForTest());
}

void test_stop_clears_kStateAbsoluteStick() {
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateAbsoluteStick);
    drive.stop();
    TEST_ASSERT_EQUAL_INT(DomeDriveRoboClaw::kStateHomed, drive.getStateForTest());
}

void test_stop_clears_kStateGoToAngle() {
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateGoToAngle);
    drive.stop();
    TEST_ASSERT_EQUAL_INT(DomeDriveRoboClaw::kStateHomed, drive.getStateForTest());
}

void test_stop_clears_kStateRandom() {
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateRandom);
    drive.stop();
    TEST_ASSERT_EQUAL_INT(DomeDriveRoboClaw::kStateHomed, drive.getStateForTest());
}

void test_stop_preserves_kStateObstructed() {
    // Obstruction handler manages its own transitions; stop() must not
    // clobber that state or it would reset the obstruction lock prematurely.
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateObstructed);
    drive.stop();
    TEST_ASSERT_EQUAL_INT(DomeDriveRoboClaw::kStateObstructed, drive.getStateForTest());
}

void test_stop_preserves_kStateManual() {
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateManual);
    drive.stop();
    TEST_ASSERT_EQUAL_INT(DomeDriveRoboClaw::kStateManual, drive.getStateForTest());
}

void test_stop_preserves_kStateHomed() {
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateHomed);
    drive.stop();
    TEST_ASSERT_EQUAL_INT(DomeDriveRoboClaw::kStateHomed, drive.getStateForTest());
}

void test_stop_zeros_motor_command() {
    auto drive = make_drive();
    drive.setLastCommandedSpeedForTest(0.75f);
    drive.stop();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, drive.getLastCommandedSpeed());
}

void test_setEnable_false_blocks_joystick_drive() {
    // Simulate an e-stop: motor was running, setEnable(false) is called,
    // then animate() fires with a non-zero joystick input.
    // Expected: driveFromJoystick() sees !fEnabled and issues stop(),
    // so fLastCommandedSpeed must remain 0.
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateManual);

    // Fake a connected joystick at full-right deflection.
    sTestStick.onConnect();
    sTestStick.state.analog.stick.rx = 127;

    // Prime the commanded speed as if the motor had been running.
    drive.setLastCommandedSpeedForTest(0.5f);

    // E-stop: disable the drive.
    drive.setEnable(false);
    TEST_ASSERT_FALSE(drive.getEnable());

    // One animate() cycle — driveFromJoystick() must call stop(), not drive.
    drive.animate();

    TEST_ASSERT_EQUAL_FLOAT(0.0f, drive.getLastCommandedSpeed());

    // Cleanup: reset stick state so it doesn't affect other tests.
    sTestStick.onDisconnect();
    sTestStick.state.analog.stick.rx = 0;
}

// ---- setAltDomeStick() — fallback when primary stick is disconnected ---------

static JoystickController sAltStick;

void test_alt_stick_used_when_primary_disconnected() {
    // Primary stick disconnected, alt stick connected at full right deflection.
    // Expected: driveFromJoystick() uses the alt stick and commands a non-zero speed.
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateManual);
    drive.setAltDomeStick(&sAltStick);

    sTestStick.onDisconnect();
    sAltStick.onConnect();
    sAltStick.state.analog.stick.rx = 127;

    drive.animate();

    TEST_ASSERT_NOT_EQUAL(0.0f, drive.getLastCommandedSpeed());

    // Cleanup
    sAltStick.onDisconnect();
    sAltStick.state.analog.stick.rx = 0;
}

void test_primary_stick_takes_priority_over_alt() {
    // Both sticks connected — primary wins and the alt is ignored.
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateManual);
    drive.setAltDomeStick(&sAltStick);

    sTestStick.onConnect();
    sTestStick.state.analog.stick.rx = 0; // primary at centre
    sAltStick.onConnect();
    sAltStick.state.analog.stick.rx = 127; // alt at full deflection

    drive.animate();

    // Primary is centred → command should be zero (deadband).
    TEST_ASSERT_EQUAL_FLOAT(0.0f, drive.getLastCommandedSpeed());

    // Cleanup
    sTestStick.onDisconnect();
    sTestStick.state.analog.stick.rx = 0;
    sAltStick.onDisconnect();
    sAltStick.state.analog.stick.rx = 0;
}

void test_alt_stick_idle_when_primary_disconnected_and_alt_centred() {
    // Alt stick connected but at centre — should produce no motion.
    auto drive = make_drive();
    drive.setStateForTest(DomeDriveRoboClaw::kStateManual);
    drive.setAltDomeStick(&sAltStick);

    sTestStick.onDisconnect();
    sAltStick.onConnect();
    sAltStick.state.analog.stick.rx = 0; // centre

    drive.animate();

    TEST_ASSERT_EQUAL_FLOAT(0.0f, drive.getLastCommandedSpeed());

    // Cleanup
    sAltStick.onDisconnect();
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_normalize_zero);
    RUN_TEST(test_normalize_359);
    RUN_TEST(test_normalize_360_wraps_to_zero);
    RUN_TEST(test_normalize_361);
    RUN_TEST(test_normalize_720_wraps_to_zero);
    RUN_TEST(test_normalize_negative_one);
    RUN_TEST(test_normalize_negative_90);
    RUN_TEST(test_normalize_negative_360);
    RUN_TEST(test_normalize_negative_361);

    RUN_TEST(test_calibration_basic);
    RUN_TEST(test_calibration_non_round);
    RUN_TEST(test_calibration_single_rotation);
    RUN_TEST(test_calibration_zero_ticks_returns_zero);
    RUN_TEST(test_calibration_zero_rotations_returns_zero);
    RUN_TEST(test_calibration_negative_ticks_returns_zero);
    RUN_TEST(test_calibration_large_gear_ratio);

    RUN_TEST(test_encoder_to_degrees_zero_ticks);
    RUN_TEST(test_encoder_to_degrees_quarter_turn);
    RUN_TEST(test_encoder_to_degrees_half_turn);
    RUN_TEST(test_encoder_to_degrees_three_quarter_turn);
    RUN_TEST(test_encoder_to_degrees_full_turn_wraps);
    RUN_TEST(test_encoder_to_degrees_one_and_quarter_turn);
    RUN_TEST(test_encoder_to_degrees_negative_quarter_turn);
    RUN_TEST(test_encoder_to_degrees_negative_half_turn);
    RUN_TEST(test_encoder_to_degrees_zero_ticks_per_rev_returns_zero);

    RUN_TEST(test_front_offset_shifts_zero_to_offset);
    RUN_TEST(test_front_offset_at_front_reads_zero);
    RUN_TEST(test_front_offset_90_degrees_past_front);
    RUN_TEST(test_front_offset_zero_means_hall_is_front);
    RUN_TEST(test_front_offset_360_same_as_zero);

    RUN_TEST(test_full_pipeline_basic);
    RUN_TEST(test_full_pipeline_with_front_offset);
    RUN_TEST(test_full_pipeline_negative_motion);

    RUN_TEST(test_calibration_requires_positive_ticks);
    RUN_TEST(test_calibration_minimum_one_rotation);

    RUN_TEST(test_angular_error_same_angle);
    RUN_TEST(test_angular_error_clockwise_short);
    RUN_TEST(test_angular_error_counterclockwise_short);
    RUN_TEST(test_angular_error_at_180_boundary);
    RUN_TEST(test_angular_error_chooses_short_path_over_180);
    RUN_TEST(test_angular_error_chooses_ccw_short_path);
    RUN_TEST(test_angular_error_full_circle_is_zero);

    RUN_TEST(test_stick_to_angle_forward);
    RUN_TEST(test_stick_to_angle_right);
    RUN_TEST(test_stick_to_angle_back);
    RUN_TEST(test_stick_to_angle_left);
    RUN_TEST(test_stick_to_angle_deadband_returns_minus1);
    RUN_TEST(test_stick_to_angle_deadband_boundary);
    RUN_TEST(test_stick_to_angle_diagonal_forward_right);

    RUN_TEST(test_abs_stick_speed_within_fudge_returns_zero);
    RUN_TEST(test_abs_stick_speed_just_outside_fudge_gives_min_speed);
    RUN_TEST(test_abs_stick_speed_cruise_zone_gives_target_speed);
    RUN_TEST(test_abs_stick_speed_at_decel_zone_boundary_gives_target_speed);
    RUN_TEST(test_abs_stick_speed_negative_error_gives_negative_speed);
    RUN_TEST(test_abs_stick_speed_capped_at_target);
    RUN_TEST(test_abs_stick_speed_custom_decel_zone);

    RUN_TEST(test_homing_hall_fires_returns_complete);
    RUN_TEST(test_homing_no_hall_not_timed_out_returns_continue);
    RUN_TEST(test_homing_elapsed_equals_timeout_still_continues);
    RUN_TEST(test_homing_elapsed_exceeds_timeout_returns_timeout);
    RUN_TEST(test_homing_hall_fires_overrides_timeout);
    RUN_TEST(test_homing_zero_elapsed_returns_continue);

    RUN_TEST(test_cal_trigger_first_returns_set_reference);
    RUN_TEST(test_cal_trigger_second_returns_continue);
    RUN_TEST(test_cal_trigger_mid_returns_continue);
    RUN_TEST(test_cal_trigger_final_returns_complete_with_tpr);
    RUN_TEST(test_cal_trigger_final_zero_ticks_returns_error);
    RUN_TEST(test_cal_trigger_final_negative_ticks_returns_error);
    RUN_TEST(test_cal_trigger_final_non_round_tpr_uses_integer_division);

    RUN_TEST(test_obstruction_not_commanded_clears_timer);
    RUN_TEST(test_obstruction_speed_just_below_threshold_clears_timer);
    RUN_TEST(test_obstruction_negative_speed_below_threshold_clears_timer);
    RUN_TEST(test_obstruction_motor_moving_clears_timer);
    RUN_TEST(test_obstruction_motor_moving_negative_speed_clears_timer);
    RUN_TEST(test_obstruction_stall_starts_timer);
    RUN_TEST(test_obstruction_stall_within_timeout_waits);
    RUN_TEST(test_obstruction_stall_at_timeout_boundary_waits);
    RUN_TEST(test_obstruction_stall_exceeds_timeout_declares);
    RUN_TEST(test_obstruction_encoder_exactly_10_clears_timer);
    RUN_TEST(test_obstruction_encoder_9_is_stall);

    RUN_TEST(test_eeprom_load_returns_zero_when_no_signature);
    RUN_TEST(test_eeprom_load_returns_zero_when_wrong_signature);
    RUN_TEST(test_eeprom_round_trip_stores_and_retrieves_tpr);
    RUN_TEST(test_eeprom_round_trip_large_tpr);
    RUN_TEST(test_eeprom_save_writes_rc01_signature);
    RUN_TEST(test_eeprom_load_returns_zero_when_tpr_is_zero);
    RUN_TEST(test_eeprom_overwrite_updates_value);

    RUN_TEST(test_seqpause_zero_arg_uses_default);
    RUN_TEST(test_seqpause_negative_arg_uses_default);
    RUN_TEST(test_seqpause_positive_arg_converted_to_ms);
    RUN_TEST(test_seqpause_arg_exceeding_cap_is_clamped);
    RUN_TEST(test_seqpause_default_exceeding_cap_is_clamped);
    RUN_TEST(test_seqpause_arg_at_cap_boundary_stays);

    RUN_TEST(test_stop_clears_kStateHoming);
    RUN_TEST(test_stop_clears_kStateCalibrating);
    RUN_TEST(test_stop_clears_kStateAbsoluteStick);
    RUN_TEST(test_stop_clears_kStateGoToAngle);
    RUN_TEST(test_stop_clears_kStateRandom);
    RUN_TEST(test_stop_preserves_kStateObstructed);
    RUN_TEST(test_stop_preserves_kStateManual);
    RUN_TEST(test_stop_preserves_kStateHomed);
    RUN_TEST(test_stop_zeros_motor_command);
    RUN_TEST(test_setEnable_false_blocks_joystick_drive);
    RUN_TEST(test_alt_stick_used_when_primary_disconnected);
    RUN_TEST(test_primary_stick_takes_priority_over_alt);
    RUN_TEST(test_alt_stick_idle_when_primary_disconnected_and_alt_centred);

    return UNITY_END();
}
