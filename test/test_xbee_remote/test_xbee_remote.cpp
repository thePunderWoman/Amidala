// test_xbee_remote.cpp
// Tests for include/xbee_remote.h:
//   XBeePocketRemote — base remote class
//   DriveController  — construction / type smoke-test
//   DomeController   — construction / type smoke-test
//
// DriveController and DomeController method bodies (notify, onConnect, etc.)
// call AmidalaController and are defined in AmidalaFirmware.ino; they are not
// exercised here.

#include "arduino_mock.h"
#include "xbee_remote.h"
#include <unity.h>
#include <string.h>

// Stub implementations — the real bodies live in AmidalaFirmware.ino and need
// AmidalaController to be fully defined.  These stubs satisfy the linker for
// the native test build without touching fDriver.
void DriveController::notify() {}
void DriveController::onConnect() {}
void DriveController::onDisconnect() {}
void DomeController::notify() {}
void DomeController::process() {}
void DomeController::onConnect() {}
void DomeController::onDisconnect() {}
bool DomeController::beginWebCapture() { return false; }
bool DomeController::stopWebCapture() { return false; }
// addGesture()'s real body reads fDriver->params.gesturetimeout (issue #172
// follow-up); fDriver is null in these tests (see TestDomeController below),
// so stub it with a fixed timeout -- none of these tests inspect
// fGestureTimeOut, only fGestureBuffer/fGestureAxis via resetGestureState().
void DomeController::addGesture(char ch) {
  if (size_t(fGesturePtr - fGestureBuffer) < sizeof(fGestureBuffer) - 1) {
    *fGesturePtr++ = ch;
    *fGesturePtr = '\0';
    fGestureTimeOut = millis() + 1000;
  }
}

void setUp(void) {}
void tearDown(void) {}

// ---- DomeController: gesture state reset (issues #163, #167) ----------------
// process() itself needs a fully-constructed AmidalaController and isn't
// exercised natively (see stub above), so these tests reach the protected
// gesture-collection members directly through a thin exposing subclass.

class TestDomeController : public DomeController {
public:
    TestDomeController() : DomeController(nullptr) {}
    using DomeController::addGesture;
    using DomeController::resetGestureState;
    using DomeController::trimTrailingCenter;
    using DomeController::fGestureBuffer;
    using DomeController::fGestureAxis;
};

void test_dome_gesture_buffer_starts_empty() {
    TestDomeController dc;
    TEST_ASSERT_EQUAL(0, strlen(dc.fGestureBuffer));
}

void test_dome_gesture_reset_clears_leftover_text() {
    TestDomeController dc;
    // Simulate a completed gesture leaving text in the buffer, as a real
    // stroke sequence would via addGesture().
    dc.addGesture('6');
    TEST_ASSERT_EQUAL_STRING("6", dc.fGestureBuffer);

    // Starting the next gesture collection (or a timeout) must clear it —
    // a zero-stroke click never calls addGesture() again to overwrite it.
    dc.resetGestureState();
    TEST_ASSERT_EQUAL(0, strlen(dc.fGestureBuffer));
}

void test_dome_gesture_click_after_reset_is_empty_not_stale() {
    TestDomeController dc;
    dc.addGesture('6');
    dc.resetGestureState();

    // A zero-stroke "click" gesture: nothing calls addGesture() before the
    // buffer is read. It must read back empty, not the previous gesture.
    TEST_ASSERT_EQUAL_STRING("", dc.fGestureBuffer);
}

void test_dome_gesture_reset_clears_leftover_axis() {
    // Regression: a gesture that ends (L3 released) while the stick is still
    // deflected leaves fGestureAxis stuck nonzero -- it's normally only
    // cleared when the stick passes back through center. Left stuck, the
    // *next* gesture's direction detection in process() (gated on
    // `!fGestureAxis`) never fires, so every gesture after the first submits
    // empty no matter what's actually drawn. Reported after #163 shipped:
    // that fix correctly stopped the stale-buffer replay, which had been
    // masking this separate bug the whole time.
    TestDomeController dc;
    dc.fGestureAxis = '4';  // as if the prior gesture ended mid-stroke-left

    dc.resetGestureState();

    TEST_ASSERT_EQUAL(0, dc.fGestureAxis);
}

// ---- DomeController: trimTrailingCenter (issue #138 gesture capture) --------

void test_dome_trim_trailing_center_strips_single_trailing_five() {
    TestDomeController dc;
    dc.addGesture('2');
    dc.addGesture('5');
    dc.addGesture('8');
    dc.addGesture('5');
    dc.trimTrailingCenter();
    TEST_ASSERT_EQUAL_STRING("258", dc.fGestureBuffer);
}

void test_dome_trim_trailing_center_leaves_non_center_ending_unchanged() {
    TestDomeController dc;
    dc.addGesture('2');
    dc.addGesture('5');
    dc.addGesture('8');
    dc.trimTrailingCenter();
    TEST_ASSERT_EQUAL_STRING("258", dc.fGestureBuffer);
}

void test_dome_trim_trailing_center_leaves_empty_buffer_unchanged() {
    TestDomeController dc;
    dc.trimTrailingCenter();
    TEST_ASSERT_EQUAL_STRING("", dc.fGestureBuffer);
}

// ---- DomeController: end-vs-timeout branch priority (issue #172 field report) --
// The real branching lives in DomeController::process() (src/drive_controllers.cpp),
// which needs a fully-constructed AmidalaController and isn't exercised natively
// (see stub comment above) -- so, same approach as FakeDblPress in
// test_double_press.cpp, this mirrors just the three-way branch exactly:
//   if (!collecting)         -> start
//   else if (l3 released)    -> end + fire   (must be checked before the deadline)
//   else if (deadline < now) -> timeout, discard
//   else                     -> add stroke
// Before the fix, the deadline check ran first, so an end press landing on the
// same tick the deadline had already passed took the timeout branch instead --
// process() never even looked at the L3 release, silently discarding the
// gesture and eating the click meant to end it. That read in the field as
// "gestures need a long wait before the next one registers."
enum class FakeGestureOutcome { kNone, kStarted, kEnded, kTimedOut, kStroke };

FakeGestureOutcome fakeGestureTick(bool collecting, bool l3Released,
                                   uint32_t deadline, uint32_t now) {
    if (!collecting) {
        return FakeGestureOutcome::kStarted;
    } else if (l3Released) {
        return FakeGestureOutcome::kEnded;
    } else if (deadline < now) {
        return FakeGestureOutcome::kTimedOut;
    }
    return FakeGestureOutcome::kStroke;
}

// A same-tick L3 release must end the gesture even when the nominal deadline
// has already passed -- this is the exact scenario that used to get eaten.
void test_gesture_l3_release_wins_over_expired_deadline() {
    FakeGestureOutcome outcome = fakeGestureTick(
        /*collecting=*/true, /*l3Released=*/true, /*deadline=*/1000, /*now=*/1500);
    TEST_ASSERT_TRUE(outcome == FakeGestureOutcome::kEnded);
}

// No L3 release and the deadline has passed -- still a genuine timeout.
void test_gesture_timeout_fires_when_l3_not_released() {
    FakeGestureOutcome outcome = fakeGestureTick(
        /*collecting=*/true, /*l3Released=*/false, /*deadline=*/1000, /*now=*/1500);
    TEST_ASSERT_TRUE(outcome == FakeGestureOutcome::kTimedOut);
}

// L3 release well before the deadline -- the common case, must still end normally.
void test_gesture_l3_release_before_deadline_ends_normally() {
    FakeGestureOutcome outcome = fakeGestureTick(
        /*collecting=*/true, /*l3Released=*/true, /*deadline=*/1000, /*now=*/500);
    TEST_ASSERT_TRUE(outcome == FakeGestureOutcome::kEnded);
}

// ---- XBeePocketRemote: initial state ----------------------------------------

void test_xbee_type_enum_values() {
    // Verify the enum constants have the expected integer values.
    TEST_ASSERT_EQUAL(0, (int)XBeePocketRemote::kFailsafe);
    TEST_ASSERT_EQUAL(1, (int)XBeePocketRemote::kXBee);
    TEST_ASSERT_EQUAL(2, (int)XBeePocketRemote::kRC);
}

void test_xbee_failsafe_returns_true_when_type_failsafe() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kFailsafe;
    TEST_ASSERT_TRUE(r.failsafe());
}

void test_xbee_failsafe_returns_false_when_type_xbee() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    TEST_ASSERT_FALSE(r.failsafe());
}

void test_xbee_failsafe_returns_false_when_type_rc() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kRC;
    TEST_ASSERT_FALSE(r.failsafe());
}

void test_xbee_initial_axes_zero() {
    XBeePocketRemote r;
    TEST_ASSERT_EQUAL(512, r.x);   // center: map(512,0,1024,127,-128)==0 (neutral)
    TEST_ASSERT_EQUAL(512, r.y);
    TEST_ASSERT_EQUAL(0, r.w1);
    TEST_ASSERT_EQUAL(0, r.w2);
}

void test_xbee_buttons_false_after_explicit_clear() {
    XBeePocketRemote r;
    memset(r.button, 0, sizeof(r.button));
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_FALSE(r.button[i]);
}

// ---- XBeePocketRemote: update() analog mapping ------------------------------

void test_xbee_update_center_stick_maps_near_zero() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    r.x = 512;  // midpoint of [0,1024] → maps to ~0 in [127,-128]
    r.y = 512;
    r.w1 = 0;
    r.w2 = 0;
    r.update();
    // map(512, 0, 1024, 127, -128): result is ~0 (integer division may give -1 or 0)
    TEST_ASSERT_INT_WITHIN(2, 0, (int)r.state.analog.stick.lx);
    TEST_ASSERT_INT_WITHIN(2, 0, (int)r.state.analog.stick.ly);
}

void test_xbee_update_full_left_stick() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    r.x = 0;    // min → map to 127
    r.y = 0;
    r.update();
    TEST_ASSERT_EQUAL(127, r.state.analog.stick.lx);
}

void test_xbee_update_full_right_stick() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    r.x = 1024;  // max → map to -128
    r.update();
    TEST_ASSERT_EQUAL(-128, r.state.analog.stick.lx);
}

void test_xbee_update_w1_maps_to_l1() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    r.w1 = 0;     // min → map(0,0,1024,255,0) = 255
    r.update();
    TEST_ASSERT_EQUAL(255, r.state.analog.button.l1);
}

void test_xbee_update_w1_max_maps_to_zero() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    r.w1 = 1024;  // max → map(1024,0,1024,255,0) = 0
    r.update();
    TEST_ASSERT_EQUAL(0, r.state.analog.button.l1);
}

void test_xbee_update_button_state_reflected() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    memset(r.button, 0, sizeof(r.button));
    r.button[0] = true;   // triangle
    r.button[2] = true;   // cross
    r.update();
    TEST_ASSERT_TRUE(r.state.button.triangle);
    TEST_ASSERT_TRUE(r.state.button.cross);
    TEST_ASSERT_FALSE(r.state.button.circle);
}

// ---- XBeePocketRemote: button event detection --------------------------------

void test_xbee_update_button_down_event_fires_on_press() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    // First update: button released
    r.button[1] = false;  // circle
    r.update();
    TEST_ASSERT_FALSE(r.event.button_down.circle);

    // Second update: button pressed
    r.button[1] = true;
    r.update();
    TEST_ASSERT_TRUE(r.event.button_down.circle);
}

void test_xbee_update_button_up_event_fires_on_release() {
    XBeePocketRemote r;
    r.type = XBeePocketRemote::kXBee;
    // Press
    r.button[1] = true;
    r.update();
    // Release
    r.button[1] = false;
    r.update();
    TEST_ASSERT_TRUE(r.event.button_up.circle);
}

// ---- DriveController / DomeController: construction -------------------------

void test_drive_controller_constructs() {
    // Pass nullptr — we're not calling any fDriver methods, just constructing.
    DriveController dc(nullptr);
    TEST_ASSERT_EQUAL_PTR(nullptr, dc.fDriver);
}

void test_dome_controller_constructs() {
    DomeController dc(nullptr);
    TEST_ASSERT_EQUAL_PTR(nullptr, dc.fDriver);
}

void test_drive_controller_is_xbee_remote() {
    DriveController dc(nullptr);
    XBeePocketRemote *base = &dc;
    TEST_ASSERT_NOT_NULL(base);
}

void test_dome_controller_is_xbee_remote() {
    DomeController dc(nullptr);
    XBeePocketRemote *base = &dc;
    TEST_ASSERT_NOT_NULL(base);
}

void test_dome_controller_failsafe_settable() {
    DomeController dc(nullptr);
    dc.type = XBeePocketRemote::kFailsafe;
    TEST_ASSERT_TRUE(dc.failsafe());
    dc.type = XBeePocketRemote::kXBee;
    TEST_ASSERT_FALSE(dc.failsafe());
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_xbee_type_enum_values);
    RUN_TEST(test_xbee_failsafe_returns_true_when_type_failsafe);
    RUN_TEST(test_xbee_failsafe_returns_false_when_type_xbee);
    RUN_TEST(test_xbee_failsafe_returns_false_when_type_rc);
    RUN_TEST(test_xbee_initial_axes_zero);
    RUN_TEST(test_xbee_buttons_false_after_explicit_clear);

    RUN_TEST(test_xbee_update_center_stick_maps_near_zero);
    RUN_TEST(test_xbee_update_full_left_stick);
    RUN_TEST(test_xbee_update_full_right_stick);
    RUN_TEST(test_xbee_update_w1_maps_to_l1);
    RUN_TEST(test_xbee_update_w1_max_maps_to_zero);
    RUN_TEST(test_xbee_update_button_state_reflected);

    RUN_TEST(test_xbee_update_button_down_event_fires_on_press);
    RUN_TEST(test_xbee_update_button_up_event_fires_on_release);

    RUN_TEST(test_drive_controller_constructs);
    RUN_TEST(test_dome_controller_constructs);
    RUN_TEST(test_drive_controller_is_xbee_remote);
    RUN_TEST(test_dome_controller_is_xbee_remote);
    RUN_TEST(test_dome_controller_failsafe_settable);

    RUN_TEST(test_dome_gesture_buffer_starts_empty);
    RUN_TEST(test_dome_gesture_reset_clears_leftover_text);
    RUN_TEST(test_dome_gesture_click_after_reset_is_empty_not_stale);
    RUN_TEST(test_dome_gesture_reset_clears_leftover_axis);

    RUN_TEST(test_dome_trim_trailing_center_strips_single_trailing_five);
    RUN_TEST(test_dome_trim_trailing_center_leaves_non_center_ending_unchanged);
    RUN_TEST(test_dome_trim_trailing_center_leaves_empty_buffer_unchanged);

    RUN_TEST(test_gesture_l3_release_wins_over_expired_deadline);
    RUN_TEST(test_gesture_timeout_fires_when_l3_not_released);
    RUN_TEST(test_gesture_l3_release_before_deadline_ends_normally);

    return UNITY_END();
}
