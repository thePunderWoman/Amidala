// test_step_value.cpp
// Tests for include/step_value.h -- the pure clamp/step helper shared by
// AmidalaController::stepVolume()/stepDriveSpeed() (issue #204 phase 2).

#include "step_value.h"
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_step_up_within_range() {
    TEST_ASSERT_EQUAL(55, stepValue(50, 5, true));
}

void test_step_down_within_range() {
    TEST_ASSERT_EQUAL(45, stepValue(50, 5, false));
}

void test_step_up_clamps_at_max() {
    TEST_ASSERT_EQUAL(100, stepValue(98, 5, true));
}

void test_step_down_clamps_at_min() {
    TEST_ASSERT_EQUAL(0, stepValue(2, 5, false));
}

void test_step_up_exactly_to_max_no_overflow() {
    TEST_ASSERT_EQUAL(100, stepValue(95, 5, true));
}

void test_step_down_exactly_to_min() {
    TEST_ASSERT_EQUAL(0, stepValue(5, 5, false));
}

void test_custom_range() {
    TEST_ASSERT_EQUAL(20, stepValue(18, 5, true, 0, 20));
    TEST_ASSERT_EQUAL(10, stepValue(12, 5, false, 10, 20));
}

void test_large_step_from_zero_up() {
    TEST_ASSERT_EQUAL(100, stepValue(0, 250, true));
}

void test_large_step_from_max_down() {
    TEST_ASSERT_EQUAL(0, stepValue(100, 250, false));
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_step_up_within_range);
    RUN_TEST(test_step_down_within_range);
    RUN_TEST(test_step_up_clamps_at_max);
    RUN_TEST(test_step_down_clamps_at_min);
    RUN_TEST(test_step_up_exactly_to_max_no_overflow);
    RUN_TEST(test_step_down_exactly_to_min);
    RUN_TEST(test_custom_range);
    RUN_TEST(test_large_step_from_zero_up);
    RUN_TEST(test_large_step_from_max_down);

    return UNITY_END();
}
