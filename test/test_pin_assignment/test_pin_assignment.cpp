// test_pin_assignment.cpp
// Unit tests for include/pin_assignment.h (issue #133): each of the 11
// board header pins picks a ROLE TYPE (DOUT/Analog/PPM/Servo/Hall); counts
// per type are derived from however many pins currently have it, subject
// to hardware ceilings (Servo<=8, PPM<=1, Hall<=1, Analog only ADC1 pins).
// Pure logic, no Arduino dependency.

#include "pin_assignment.h"
#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

// ---- isPinInAssignablePool() -------------------------------------------

void test_pool_accepts_all_eleven_pins() {
    for (uint8_t p : kAssignablePins) {
        TEST_ASSERT_TRUE(isPinInAssignablePool(p));
    }
}

void test_pool_rejects_pins_outside_the_pool() {
    uint8_t outside[] = {0, 7, 8, 9, 10, 11, 17, 18, 21, 22, 38, 43, 44, 48};
    for (uint8_t p : outside) {
        TEST_ASSERT_FALSE(isPinInAssignablePool(p));
    }
}

// ---- isRoleValidForPin() -- ADC1 narrowing for Analog only --------------

void test_analog_accepts_adc1_capable_pool_pins() {
    TEST_ASSERT_TRUE(isRoleValidForPin(1, PinRoleType::kAnalog));
    TEST_ASSERT_TRUE(isRoleValidForPin(6, PinRoleType::kAnalog));
}

void test_analog_rejects_non_adc1_pool_pin() {
    // 39 is in the pool (default DOUT) but isn't ADC1-capable.
    TEST_ASSERT_FALSE(isRoleValidForPin(39, PinRoleType::kAnalog));
    TEST_ASSERT_FALSE(isRoleValidForPin(47, PinRoleType::kAnalog));
}

void test_non_analog_roles_accept_any_pool_pin_including_adc1_ones() {
    TEST_ASSERT_TRUE(isRoleValidForPin(1, PinRoleType::kDout));
    TEST_ASSERT_TRUE(isRoleValidForPin(2, PinRoleType::kServo));
    TEST_ASSERT_TRUE(isRoleValidForPin(39, PinRoleType::kPpm));
    TEST_ASSERT_TRUE(isRoleValidForPin(40, PinRoleType::kHall));
}

void test_role_check_rejects_pins_outside_the_pool() {
    TEST_ASSERT_FALSE(isRoleValidForPin(8, PinRoleType::kDout));   // I2C pin
    TEST_ASSERT_FALSE(isRoleValidForPin(17, PinRoleType::kServo)); // RoboClaw UART pin
}

// ---- pinIndexOf() --------------------------------------------------------

void test_pin_index_of_known_pins() {
    TEST_ASSERT_EQUAL(0,  pinIndexOf(1));
    TEST_ASSERT_EQUAL(6,  pinIndexOf(39));
    TEST_ASSERT_EQUAL(10, pinIndexOf(47));
}

void test_pin_index_of_unknown_pin_is_out_of_bounds() {
    TEST_ASSERT_TRUE(pinIndexOf(8) >= 11);
}

// ---- countPinsWithRole() / nthPinWithRole() ------------------------------

// Fixture mirroring today's real default assignment (kAssignablePins order:
// 1,2,3,4,5,6,39,40,41,42,47), ROBOCLAW variant (pin 40 = Hall).
static void defaultRoboclawRoles(PinRoleType out[11]) {
    out[0] = PinRoleType::kAnalog; // 1
    out[1] = PinRoleType::kAnalog; // 2
    out[2] = PinRoleType::kServo;  // 3
    out[3] = PinRoleType::kServo;  // 4
    out[4] = PinRoleType::kServo;  // 5
    out[5] = PinRoleType::kServo;  // 6
    out[6] = PinRoleType::kDout;   // 39
    out[7] = PinRoleType::kHall;   // 40
    out[8] = PinRoleType::kDout;   // 41
    out[9] = PinRoleType::kDout;   // 42
    out[10] = PinRoleType::kPpm;   // 47
}

void test_count_pins_with_role_default_roboclaw() {
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);
    TEST_ASSERT_EQUAL(2, countPinsWithRole(roles, PinRoleType::kAnalog));
    TEST_ASSERT_EQUAL(4, countPinsWithRole(roles, PinRoleType::kServo));
    TEST_ASSERT_EQUAL(3, countPinsWithRole(roles, PinRoleType::kDout));
    TEST_ASSERT_EQUAL(1, countPinsWithRole(roles, PinRoleType::kHall));
    TEST_ASSERT_EQUAL(1, countPinsWithRole(roles, PinRoleType::kPpm));
}

void test_nth_pin_with_role_matches_expected_defaults() {
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);
    TEST_ASSERT_EQUAL(1, nthPinWithRole(roles, PinRoleType::kAnalog, 0));
    TEST_ASSERT_EQUAL(2, nthPinWithRole(roles, PinRoleType::kAnalog, 1));
    TEST_ASSERT_EQUAL(3, nthPinWithRole(roles, PinRoleType::kServo, 0));
    TEST_ASSERT_EQUAL(6, nthPinWithRole(roles, PinRoleType::kServo, 3));
    TEST_ASSERT_EQUAL(39, nthPinWithRole(roles, PinRoleType::kDout, 0));
    TEST_ASSERT_EQUAL(40, nthPinWithRole(roles, PinRoleType::kHall, 0));
    TEST_ASSERT_EQUAL(47, nthPinWithRole(roles, PinRoleType::kPpm, 0));
}

void test_nth_pin_with_role_out_of_range_returns_sentinel() {
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);
    TEST_ASSERT_EQUAL(kNoPin, nthPinWithRole(roles, PinRoleType::kAnalog, 2)); // only 2 exist
    TEST_ASSERT_EQUAL(kNoPin, nthPinWithRole(roles, PinRoleType::kHall, 1));   // only 1 exists
}

// ---- validateRoleChange() ------------------------------------------------

void test_validate_accepts_reselecting_own_current_role() {
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);
    PinRoleValidationResult r = validateRoleChange(39, PinRoleType::kDout, roles);
    TEST_ASSERT_TRUE(r.ok);
}

void test_validate_rejects_pin_outside_pool() {
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);
    PinRoleValidationResult r = validateRoleChange(8, PinRoleType::kDout, roles);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_NOT_NULL(r.reason);
}

void test_validate_rejects_analog_on_non_adc1_pin() {
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);
    PinRoleValidationResult r = validateRoleChange(39, PinRoleType::kAnalog, roles);
    TEST_ASSERT_FALSE(r.ok);
}

void test_validate_accepts_trading_a_dout_pin_for_a_fifth_servo() {
    // The whole point of #133: free up a DOUT pin, retype it Servo.
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);  // 4 servos, 3 douts (39,41,42) currently
    PinRoleValidationResult r = validateRoleChange(39, PinRoleType::kServo, roles);
    TEST_ASSERT_TRUE(r.ok);
}

void test_validate_rejects_ninth_servo_at_ledc_ceiling() {
    // 8 pins already servo-typed (using all pool pins except 3 fixed).
    PinRoleType roles[11];
    for (int i = 0; i < 8; i++) roles[i] = PinRoleType::kServo;
    roles[8]  = PinRoleType::kDout;
    roles[9]  = PinRoleType::kDout;
    roles[10] = PinRoleType::kPpm;
    PinRoleValidationResult r = validateRoleChange(41 /* roles[8] */, PinRoleType::kServo, roles);
    TEST_ASSERT_FALSE(r.ok);
}

void test_validate_accepts_eighth_servo_at_exact_ceiling() {
    PinRoleType roles[11];
    for (int i = 0; i < 7; i++) roles[i] = PinRoleType::kServo;  // 7 so far
    roles[7]  = PinRoleType::kDout;
    roles[8]  = PinRoleType::kDout;
    roles[9]  = PinRoleType::kDout;
    roles[10] = PinRoleType::kPpm;
    PinRoleValidationResult r = validateRoleChange(40 /* roles[7] */, PinRoleType::kServo, roles);
    TEST_ASSERT_TRUE(r.ok);  // becomes the 8th -- exactly at the ceiling, not over
}

void test_validate_rejects_second_ppm_pin() {
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);  // pin 47 already PPM
    PinRoleValidationResult r = validateRoleChange(39, PinRoleType::kPpm, roles);
    TEST_ASSERT_FALSE(r.ok);
}

void test_validate_rejects_second_hall_pin() {
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);  // pin 40 already Hall
    PinRoleValidationResult r = validateRoleChange(39, PinRoleType::kHall, roles);
    TEST_ASSERT_FALSE(r.ok);
}

void test_validate_accepts_moving_hall_to_a_different_pin_after_freeing_original() {
    // Simulates: user already changed pin 40 away from Hall (e.g. to Dout),
    // now wants a different pin to be Hall instead (physically rewired).
    PinRoleType roles[11];
    defaultRoboclawRoles(roles);
    roles[7] = PinRoleType::kDout;  // pin 40 no longer Hall
    PinRoleValidationResult r = validateRoleChange(39, PinRoleType::kHall, roles);
    TEST_ASSERT_TRUE(r.ok);
}

void test_validate_unlimited_dout_count() {
    // No ceiling on DOUT beyond the pool -- even a pin already counted many
    // times over among "others" (impossible in practice since each pin has
    // exactly one role, but confirms no artificial DOUT cap exists).
    PinRoleType roles[11];
    for (int i = 0; i < 10; i++) roles[i] = PinRoleType::kDout;
    roles[10] = PinRoleType::kPpm;
    PinRoleValidationResult r = validateRoleChange(47, PinRoleType::kDout, roles);
    TEST_ASSERT_TRUE(r.ok);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_pool_accepts_all_eleven_pins);
    RUN_TEST(test_pool_rejects_pins_outside_the_pool);

    RUN_TEST(test_analog_accepts_adc1_capable_pool_pins);
    RUN_TEST(test_analog_rejects_non_adc1_pool_pin);
    RUN_TEST(test_non_analog_roles_accept_any_pool_pin_including_adc1_ones);
    RUN_TEST(test_role_check_rejects_pins_outside_the_pool);

    RUN_TEST(test_pin_index_of_known_pins);
    RUN_TEST(test_pin_index_of_unknown_pin_is_out_of_bounds);

    RUN_TEST(test_count_pins_with_role_default_roboclaw);
    RUN_TEST(test_nth_pin_with_role_matches_expected_defaults);
    RUN_TEST(test_nth_pin_with_role_out_of_range_returns_sentinel);

    RUN_TEST(test_validate_accepts_reselecting_own_current_role);
    RUN_TEST(test_validate_rejects_pin_outside_pool);
    RUN_TEST(test_validate_rejects_analog_on_non_adc1_pin);
    RUN_TEST(test_validate_accepts_trading_a_dout_pin_for_a_fifth_servo);
    RUN_TEST(test_validate_rejects_ninth_servo_at_ledc_ceiling);
    RUN_TEST(test_validate_accepts_eighth_servo_at_exact_ceiling);
    RUN_TEST(test_validate_rejects_second_ppm_pin);
    RUN_TEST(test_validate_rejects_second_hall_pin);
    RUN_TEST(test_validate_accepts_moving_hall_to_a_different_pin_after_freeing_original);
    RUN_TEST(test_validate_unlimited_dout_count);

    return UNITY_END();
}
