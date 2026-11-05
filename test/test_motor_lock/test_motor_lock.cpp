// test_motor_lock.cpp
// Tests for include/motor_lock.h -- what blocks AmidalaController's
// enableController()/enableDomeController().

#include "motor_lock.h"
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_starts_unlocked() {
    MotorLock lock;
    TEST_ASSERT_TRUE(lock.allowsEnable());
    TEST_ASSERT_FALSE(lock.estopActive());
    TEST_ASSERT_FALSE(lock.motorsDisabled());
}

void test_estop_blocks_enable_until_cleared() {
    // Regression: a remote reconnecting or the lag-based safety stop
    // recovering used to re-enable the drive behind an active web e-stop.
    MotorLock lock;
    lock.setEstop(true);
    TEST_ASSERT_FALSE(lock.allowsEnable());
    lock.setEstop(false);
    TEST_ASSERT_TRUE(lock.allowsEnable());
}

void test_motors_off_blocks_enable_until_cleared() {
    MotorLock lock;
    lock.setMotorsDisabled(true);
    TEST_ASSERT_FALSE(lock.allowsEnable());
    lock.setMotorsDisabled(false);
    TEST_ASSERT_TRUE(lock.allowsEnable());
}

void test_both_reasons_are_independent() {
    MotorLock lock;
    lock.setEstop(true);
    lock.setMotorsDisabled(true);
    lock.setEstop(false);  // resume while motors are still switched off
    TEST_ASSERT_FALSE(lock.allowsEnable());
    lock.setMotorsDisabled(false);
    TEST_ASSERT_TRUE(lock.allowsEnable());
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_starts_unlocked);
    RUN_TEST(test_estop_blocks_enable_until_cleared);
    RUN_TEST(test_motors_off_blocks_enable_until_cleared);
    RUN_TEST(test_both_reasons_are_independent);
    return UNITY_END();
}
