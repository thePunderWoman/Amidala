// test_motor_lock.cpp
// Tests for include/motor_lock.h -- what blocks AmidalaController's
// enableController() (foot drive) and enableDomeController() (dome).

#include "motor_lock.h"
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_starts_unlocked() {
    MotorLock lock;
    TEST_ASSERT_TRUE(lock.allowsDriveEnable());
    TEST_ASSERT_TRUE(lock.allowsDomeEnable());
    TEST_ASSERT_FALSE(lock.estopActive());
    TEST_ASSERT_FALSE(lock.driveMotorsOff());
    TEST_ASSERT_FALSE(lock.domeMotorOff());
    TEST_ASSERT_FALSE(lock.anyMotorsOff());
}

void test_estop_blocks_both_until_cleared() {
    // Regression: a remote reconnecting or the lag-based safety stop
    // recovering used to re-enable the drive behind an active web e-stop.
    MotorLock lock;
    lock.setEstop(true);
    TEST_ASSERT_FALSE(lock.allowsDriveEnable());
    TEST_ASSERT_FALSE(lock.allowsDomeEnable());
    TEST_ASSERT_FALSE(lock.anyMotorsOff());  // e-stop isn't "motors off"
    lock.setEstop(false);
    TEST_ASSERT_TRUE(lock.allowsDriveEnable());
    TEST_ASSERT_TRUE(lock.allowsDomeEnable());
}

void test_foot_motors_off_blocks_only_the_drive() {
    MotorLock lock;
    lock.setDriveMotorsOff(true);
    TEST_ASSERT_FALSE(lock.allowsDriveEnable());
    TEST_ASSERT_TRUE(lock.allowsDomeEnable());
    TEST_ASSERT_TRUE(lock.anyMotorsOff());
    lock.setDriveMotorsOff(false);
    TEST_ASSERT_TRUE(lock.allowsDriveEnable());
    TEST_ASSERT_FALSE(lock.anyMotorsOff());
}

void test_dome_motor_off_blocks_only_the_dome() {
    MotorLock lock;
    lock.setDomeMotorOff(true);
    TEST_ASSERT_TRUE(lock.allowsDriveEnable());
    TEST_ASSERT_FALSE(lock.allowsDomeEnable());
    TEST_ASSERT_TRUE(lock.anyMotorsOff());
    lock.setDomeMotorOff(false);
    TEST_ASSERT_TRUE(lock.allowsDomeEnable());
    TEST_ASSERT_FALSE(lock.anyMotorsOff());
}

void test_resume_keeps_switched_off_motors_off() {
    MotorLock lock;
    lock.setEstop(true);
    lock.setDriveMotorsOff(true);
    lock.setEstop(false);  // resume while the feet are still switched off
    TEST_ASSERT_FALSE(lock.allowsDriveEnable());
    TEST_ASSERT_TRUE(lock.allowsDomeEnable());
    lock.setDriveMotorsOff(false);
    TEST_ASSERT_TRUE(lock.allowsDriveEnable());
}

void test_all_motors_toggle_turns_both_off_when_both_on() {
    MotorLock lock;
    TEST_ASSERT_TRUE(lock.nextAllMotorsOff());
}

void test_all_motors_toggle_turns_both_on_when_both_off() {
    MotorLock lock;
    lock.setDriveMotorsOff(true);
    lock.setDomeMotorOff(true);
    TEST_ASSERT_FALSE(lock.nextAllMotorsOff());
}

void test_all_motors_toggle_turns_both_off_from_a_mixed_state() {
    // Only one off: the combined toggle must not start the other motor.
    MotorLock lock;
    lock.setDriveMotorsOff(true);
    TEST_ASSERT_TRUE(lock.nextAllMotorsOff());
    lock.setDriveMotorsOff(false);
    lock.setDomeMotorOff(true);
    TEST_ASSERT_TRUE(lock.nextAllMotorsOff());
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_starts_unlocked);
    RUN_TEST(test_estop_blocks_both_until_cleared);
    RUN_TEST(test_foot_motors_off_blocks_only_the_drive);
    RUN_TEST(test_dome_motor_off_blocks_only_the_dome);
    RUN_TEST(test_resume_keeps_switched_off_motors_off);
    RUN_TEST(test_all_motors_toggle_turns_both_off_when_both_on);
    RUN_TEST(test_all_motors_toggle_turns_both_on_when_both_off);
    RUN_TEST(test_all_motors_toggle_turns_both_off_from_a_mixed_state);
    return UNITY_END();
}
