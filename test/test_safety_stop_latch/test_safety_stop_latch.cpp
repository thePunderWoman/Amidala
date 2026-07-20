// test_safety_stop_latch.cpp
// Unit tests for SafetyStopLatch (include/safety_stop_latch.h).
//
// Regression coverage for https://github.com/thePunderWoman/Amidala/issues/139:
// a temporary XBee packet drop (over notify()'s 500ms lag threshold but
// under the real failsafe timeout) tripped emergencyStop()/domeEmergencyStop()
// but nothing ever re-enabled the drive/dome once packets resumed -- only a
// full failsafe/reconnect cycle (onConnect()) did that, so a brief RF blip
// left the drive silently disabled until the remote was fully power-cycled.
//
// DriveController/DomeController::notify() (src/drive_controllers.cpp) can't
// be exercised directly in the native test env -- they call into
// AmidalaController, which can't be constructed without real hardware (same
// constraint noted in test/test_xbee_remote/test_xbee_remote.cpp) -- so this
// tests the extracted trip/recover decision logic those methods actually use.

#include "safety_stop_latch.h"
#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

void test_starts_untripped() {
    SafetyStopLatch l;
    TEST_ASSERT_FALSE(l.tripped());
}

void test_recover_is_a_noop_when_never_tripped() {
    SafetyStopLatch l;
    TEST_ASSERT_FALSE(l.recover());
    TEST_ASSERT_FALSE(l.tripped());
}

void test_trip_sets_tripped() {
    SafetyStopLatch l;
    l.trip();
    TEST_ASSERT_TRUE(l.tripped());
}

void test_recover_after_trip_returns_true_and_clears() {
    SafetyStopLatch l;
    l.trip();
    TEST_ASSERT_TRUE(l.recover());
    TEST_ASSERT_FALSE(l.tripped());
}

// The core bug: recover() must only fire the "please re-enable" signal once
// per trip, not on every tick afterward -- otherwise the caller would call
// enableController()/enableDomeController() every single tick forever.
void test_recover_only_fires_once_per_trip() {
    SafetyStopLatch l;
    l.trip();
    TEST_ASSERT_TRUE(l.recover());
    TEST_ASSERT_FALSE(l.recover());
    TEST_ASSERT_FALSE(l.recover());
}

// A brief drop (trip) followed by packets resuming (recover) must signal a
// re-enable is owed -- this is the exact sequence issue #139 got wrong.
void test_trip_then_recover_signals_reenable_owed() {
    SafetyStopLatch l;
    TEST_ASSERT_FALSE(l.tripped());  // connected
    l.trip();                        // lag > 500ms: safety stop fires
    TEST_ASSERT_TRUE(l.tripped());
    bool owesReenable = l.recover(); // packets resume
    TEST_ASSERT_TRUE(owesReenable);
    TEST_ASSERT_FALSE(l.tripped());
}

// Repeated trips (e.g. lagTime > 500 on several consecutive ticks before
// recovering) must not require multiple recover() calls to clear.
void test_repeated_trip_still_clears_on_single_recover() {
    SafetyStopLatch l;
    l.trip();
    l.trip();
    l.trip();
    TEST_ASSERT_TRUE(l.recover());
    TEST_ASSERT_FALSE(l.tripped());
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_starts_untripped);
    RUN_TEST(test_recover_is_a_noop_when_never_tripped);
    RUN_TEST(test_trip_sets_tripped);
    RUN_TEST(test_recover_after_trip_returns_true_and_clears);
    RUN_TEST(test_recover_only_fires_once_per_trip);
    RUN_TEST(test_trip_then_recover_signals_reenable_owed);
    RUN_TEST(test_repeated_trip_still_clears_on_single_recover);

    return UNITY_END();
}
