// test_button_dispatch.cpp
// Tests for include/button_dispatch.h -- the shared long-press timer and
// alt-modifier dispatch used by XBeePocketRemote, BTGamepad, and SnipsRemote
// (issue #204). Previously each of the three hand-rolled its own copy of
// this logic; test_bt_gamepad.cpp's old test_dispatch_* section (a
// hand-duplicated mirror, since BTGamepad itself sits behind BLE headers
// that don't compile natively) covered the same ground -- this is now the
// real coverage against the actual production code, since button_dispatch.h
// has no BLE/Arduino dependency and compiles natively as-is.

#include "arduino_mock.h"
#include "button_dispatch.h"
#include <unity.h>

void setUp(void) { mock_millis_value = 0; }
void tearDown(void) {}

struct FakeDriver {
    int buttonUpCount[25]   = {0};  // 1-based, index 0 unused
    int altButtonCount[25]  = {0};
    int longButtonCount[25] = {0};

    void noteButtonUp(unsigned num)      { buttonUpCount[num]++; }
    void processAltButton(unsigned num)  { altButtonCount[num]++; }
    void processLongButton(unsigned num) { longButtonCount[num]++; }
};

// ---- ButtonLongPress ---------------------------------------------------------

void test_longpress_no_event_when_never_pressed() {
    ButtonLongPress lp;
    auto r = lp.update(false, false, false, 100, 3000);
    TEST_ASSERT_FALSE(r.up);
    TEST_ASSERT_FALSE(r.longUp);
}

void test_longpress_quick_tap_reports_up_no_long() {
    ButtonLongPress lp;
    lp.update(true, false, true, 0, 3000);       // down at t=0
    auto r = lp.update(false, true, false, 100, 3000);  // up at t=100 (short hold)
    TEST_ASSERT_TRUE(r.up);
    TEST_ASSERT_FALSE(r.longUp);
}

void test_longpress_fires_once_after_threshold_while_held() {
    ButtonLongPress lp;
    lp.update(true, false, true, 1, 3000);  // down at t=1 (not 0 -- see note below)
    auto r1 = lp.update(false, false, true, 3002, 3000);  // still held, past threshold
    TEST_ASSERT_TRUE(r1.longUp);

    // Must not fire again on a subsequent tick while still held.
    auto r2 = lp.update(false, false, true, 4000, 3000);
    TEST_ASSERT_FALSE(r2.longUp);
}

void test_longpress_suppresses_trailing_up_after_firing() {
    ButtonLongPress lp;
    lp.update(true, false, true, 1, 3000);
    lp.update(false, false, true, 3002, 3000);  // long-press fires

    auto r = lp.update(false, true, false, 3100, 3000);  // release after long-press
    TEST_ASSERT_FALSE(r.up);  // suppressed
    TEST_ASSERT_FALSE(r.longUp);
}

void test_longpress_released_before_threshold_does_not_fire() {
    ButtonLongPress lp;
    lp.update(true, false, true, 1, 3000);
    auto r = lp.update(false, true, false, 2999, 3000);  // released just under threshold
    TEST_ASSERT_TRUE(r.up);
    TEST_ASSERT_FALSE(r.longUp);
}

void test_longpress_custom_threshold_respected() {
    ButtonLongPress lp;
    lp.update(true, false, true, 1, 500);
    auto r = lp.update(false, false, true, 502, 500);
    TEST_ASSERT_TRUE(r.longUp);
}

// ---- dispatchButtonPress ------------------------------------------------------

void test_dispatch_up_with_no_alt_fires_notebuttonup() {
    FakeDriver driver;
    dispatchButtonPress(driver, 3, /*up=*/true, /*longUp=*/false, /*altbtn=*/0, /*altHeld=*/false);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[3]);
    TEST_ASSERT_EQUAL(0, driver.altButtonCount[3]);
}

void test_dispatch_up_while_alt_held_routes_to_alt_layer() {
    FakeDriver driver;
    dispatchButtonPress(driver, 2, true, false, /*altbtn=*/1, /*altHeld=*/true);
    TEST_ASSERT_EQUAL(1, driver.altButtonCount[2]);
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[2]);
}

void test_dispatch_suppresses_the_alt_buttons_own_press() {
    FakeDriver driver;
    // altbtn==num: neither noteButtonUp nor processAltButton should fire.
    dispatchButtonPress(driver, 1, true, false, /*altbtn=*/1, /*altHeld=*/true);
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[1]);
    TEST_ASSERT_EQUAL(0, driver.altButtonCount[1]);
}

void test_dispatch_long_press_fires_when_alt_not_held() {
    FakeDriver driver;
    dispatchButtonPress(driver, 4, false, /*longUp=*/true, /*altbtn=*/0, /*altHeld=*/false);
    TEST_ASSERT_EQUAL(1, driver.longButtonCount[4]);
}

void test_dispatch_long_press_suppressed_while_alt_held() {
    FakeDriver driver;
    dispatchButtonPress(driver, 4, false, true, /*altbtn=*/1, /*altHeld=*/true);
    TEST_ASSERT_EQUAL(0, driver.longButtonCount[4]);
}

void test_dispatch_long_press_suppressed_for_alt_buttons_own_button() {
    FakeDriver driver;
    dispatchButtonPress(driver, 1, false, true, /*altbtn=*/1, /*altHeld=*/false);
    TEST_ASSERT_EQUAL(0, driver.longButtonCount[1]);
}

void test_dispatch_button_24_supported() {
    // MAX_BUTTONS (issue #204) is 24 -- confirm the dispatch path isn't
    // hardcoded to the old 9-button range.
    FakeDriver driver;
    dispatchButtonPress(driver, 24, true, false, 0, false);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[24]);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_longpress_no_event_when_never_pressed);
    RUN_TEST(test_longpress_quick_tap_reports_up_no_long);
    RUN_TEST(test_longpress_fires_once_after_threshold_while_held);
    RUN_TEST(test_longpress_suppresses_trailing_up_after_firing);
    RUN_TEST(test_longpress_released_before_threshold_does_not_fire);
    RUN_TEST(test_longpress_custom_threshold_respected);

    RUN_TEST(test_dispatch_up_with_no_alt_fires_notebuttonup);
    RUN_TEST(test_dispatch_up_while_alt_held_routes_to_alt_layer);
    RUN_TEST(test_dispatch_suppresses_the_alt_buttons_own_press);
    RUN_TEST(test_dispatch_long_press_fires_when_alt_not_held);
    RUN_TEST(test_dispatch_long_press_suppressed_while_alt_held);
    RUN_TEST(test_dispatch_long_press_suppressed_for_alt_buttons_own_button);
    RUN_TEST(test_dispatch_button_24_supported);

    return UNITY_END();
}
