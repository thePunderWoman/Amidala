// test_bt_gamepad.cpp
// Unit tests for BTGamepad::_parseReport() — the HID report parser.
//
// The BLE connection layer (BLEDevice, BLEScan, etc.) is hardware-only and
// cannot be tested natively.  The report-parsing logic is pure data
// transformation with no hardware dependency, so we test it in isolation by
// duplicating the parse function here (same algorithm, no BLE includes).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include "arduino_mock.h"
#include "JoystickController.h"

void setUp(void)    { mock_millis_value = 0; }
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Minimal replica of BTGamepad state — just the fields _parseReport uses.
// ---------------------------------------------------------------------------

struct ParseState {
    JoystickController::State state;
    bool notified;

    ParseState() : notified(false) { memset(&state, 0, sizeof(state)); }
};

// Mirror of BTGamepad::_parseReport from src/bt_gamepad.cpp.
// Must stay in sync with the firmware implementation.
static void parse_report(ParseState& ps, const uint8_t* d, size_t len) {
    size_t off = 0;
    if (len >= 10 && d[0] == 0x01) off = 1;

    if (len - off >= 9) {
        uint8_t b0 = d[off + 0];
        uint8_t b1 = d[off + 1];
        uint8_t hat = d[off + 2];
        int8_t lx = (int8_t)((int)d[off + 3] - 128);
        int8_t ly = (int8_t)((int)d[off + 4] - 128);
        int8_t rx = (int8_t)((int)d[off + 5] - 128);
        int8_t ry = (int8_t)((int)d[off + 6] - 128);
        uint8_t lt = d[off + 7];
        uint8_t rt = d[off + 8];

        ps.state.analog.stick.lx = lx;
        ps.state.analog.stick.ly = ly;
        ps.state.analog.stick.rx = rx;
        ps.state.analog.stick.ry = ry;
        ps.state.analog.button.l2 = lt;
        ps.state.analog.button.r2 = rt;

        ps.state.button.cross    = (b0 >> 0) & 1;
        ps.state.button.circle   = (b0 >> 1) & 1;
        ps.state.button.square   = (b0 >> 2) & 1;
        ps.state.button.triangle = (b0 >> 3) & 1;
        ps.state.button.l1       = (b0 >> 4) & 1;
        ps.state.button.r1       = (b0 >> 5) & 1;
        ps.state.button.select   = (b0 >> 6) & 1;
        ps.state.button.start    = (b0 >> 7) & 1;
        ps.state.button.l3       = (b1 >> 0) & 1;
        ps.state.button.r3       = (b1 >> 1) & 1;
        ps.state.button.ps       = (b1 >> 2) & 1;

        ps.state.button.up    = (hat == 0 || hat == 1 || hat == 7);
        ps.state.button.right = (hat == 1 || hat == 2 || hat == 3);
        ps.state.button.down  = (hat == 3 || hat == 4 || hat == 5);
        ps.state.button.left  = (hat == 5 || hat == 6 || hat == 7);

        ps.notified = true;
    } else if (len - off >= 14) {
        uint16_t lx16 = (uint16_t)d[off+2] | ((uint16_t)d[off+3] << 8);
        uint16_t ly16 = (uint16_t)d[off+4] | ((uint16_t)d[off+5] << 8);
        uint16_t rx16 = (uint16_t)d[off+6] | ((uint16_t)d[off+7] << 8);
        uint16_t ry16 = (uint16_t)d[off+8] | ((uint16_t)d[off+9] << 8);
        uint16_t lt16 = (uint16_t)d[off+10] | ((uint16_t)d[off+11] << 8);
        uint16_t rt16 = (uint16_t)d[off+12] | ((uint16_t)d[off+13] << 8);

        ps.state.analog.stick.lx = (int8_t)((int)(lx16 >> 8) - 128);
        ps.state.analog.stick.ly = (int8_t)((int)(ly16 >> 8) - 128);
        ps.state.analog.stick.rx = (int8_t)((int)(rx16 >> 8) - 128);
        ps.state.analog.stick.ry = (int8_t)((int)(ry16 >> 8) - 128);
        ps.state.analog.button.l2 = (uint8_t)(lt16 >> 2);
        ps.state.analog.button.r2 = (uint8_t)(rt16 >> 2);

        if (len - off >= 16) {
            uint8_t b = d[off + 14];
            ps.state.button.cross    = (b >> 4) & 1;
            ps.state.button.circle   = (b >> 5) & 1;
            ps.state.button.square   = (b >> 6) & 1;
            ps.state.button.triangle = (b >> 7) & 1;
            uint8_t b2 = d[off + 15];
            ps.state.button.l1     = (b2 >> 0) & 1;
            ps.state.button.r1     = (b2 >> 1) & 1;
            ps.state.button.select = (b2 >> 2) & 1;
            ps.state.button.start  = (b2 >> 3) & 1;
            ps.state.button.l3     = (b2 >> 4) & 1;
            ps.state.button.r3     = (b2 >> 5) & 1;
            uint8_t hat = d[off + 0] & 0xF;
            ps.state.button.up    = (hat == 1);
            ps.state.button.right = (hat == 3);
            ps.state.button.down  = (hat == 5);
            ps.state.button.left  = (hat == 7);
        }
        ps.notified = true;
    }
}

// ---------------------------------------------------------------------------
// Generic gamepad layout tests (9-byte report, no report ID)
// ---------------------------------------------------------------------------

void test_generic_sticks_centred() {
    // All bytes zero except axes at 128 (centre).
    uint8_t rpt[] = {0x00, 0x00, 0x08, 128, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_EQUAL_INT8(0, ps.state.analog.stick.lx);
    TEST_ASSERT_EQUAL_INT8(0, ps.state.analog.stick.ly);
    TEST_ASSERT_EQUAL_INT8(0, ps.state.analog.stick.rx);
    TEST_ASSERT_EQUAL_INT8(0, ps.state.analog.stick.ry);
    TEST_ASSERT_TRUE(ps.notified);
}

void test_generic_left_stick_full_right() {
    uint8_t rpt[] = {0x00, 0x00, 0x08, 255, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_EQUAL_INT8(127, ps.state.analog.stick.lx);
    TEST_ASSERT_EQUAL_INT8(0,   ps.state.analog.stick.ly);
}

void test_generic_left_stick_full_left() {
    uint8_t rpt[] = {0x00, 0x00, 0x08, 0, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_EQUAL_INT8(-128, ps.state.analog.stick.lx);
}

void test_generic_right_stick_mapped() {
    uint8_t rpt[] = {0x00, 0x00, 0x08, 128, 128, 200, 50, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_EQUAL_INT8((int8_t)(200 - 128), ps.state.analog.stick.rx);
    TEST_ASSERT_EQUAL_INT8((int8_t)(50  - 128), ps.state.analog.stick.ry);
}

void test_generic_cross_button() {
    uint8_t rpt[] = {0x01, 0x00, 0x08, 128, 128, 128, 128, 0, 0}; // bit 0 of b0
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_TRUE(ps.state.button.cross);
    TEST_ASSERT_FALSE(ps.state.button.circle);
    TEST_ASSERT_FALSE(ps.state.button.square);
    TEST_ASSERT_FALSE(ps.state.button.triangle);
}

void test_generic_all_face_buttons() {
    uint8_t rpt[] = {0x0F, 0x00, 0x08, 128, 128, 128, 128, 0, 0}; // bits 0-3
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_TRUE(ps.state.button.cross);
    TEST_ASSERT_TRUE(ps.state.button.circle);
    TEST_ASSERT_TRUE(ps.state.button.square);
    TEST_ASSERT_TRUE(ps.state.button.triangle);
}

void test_generic_shoulder_buttons() {
    uint8_t rpt[] = {0x30, 0x00, 0x08, 128, 128, 128, 128, 0, 0}; // bits 4-5
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_TRUE(ps.state.button.l1);
    TEST_ASSERT_TRUE(ps.state.button.r1);
    TEST_ASSERT_FALSE(ps.state.button.cross);
}

void test_generic_start_select() {
    uint8_t rpt[] = {0xC0, 0x00, 0x08, 128, 128, 128, 128, 0, 0}; // bits 6-7
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_TRUE(ps.state.button.select);
    TEST_ASSERT_TRUE(ps.state.button.start);
}

void test_generic_thumbstick_buttons() {
    uint8_t rpt[] = {0x00, 0x03, 0x08, 128, 128, 128, 128, 0, 0}; // bits 0-1 of b1
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_TRUE(ps.state.button.l3);
    TEST_ASSERT_TRUE(ps.state.button.r3);
}

void test_generic_triggers() {
    uint8_t rpt[] = {0x00, 0x00, 0x08, 128, 128, 128, 128, 200, 150};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_EQUAL_UINT8(200, ps.state.analog.button.l2);
    TEST_ASSERT_EQUAL_UINT8(150, ps.state.analog.button.r2);
}

// ---- Hat switch / d-pad ---------------------------------------------------

void test_hat_north_sets_up() {
    uint8_t rpt[] = {0x00, 0x00, 0, 128, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_TRUE(ps.state.button.up);
    TEST_ASSERT_FALSE(ps.state.button.right);
    TEST_ASSERT_FALSE(ps.state.button.down);
    TEST_ASSERT_FALSE(ps.state.button.left);
}

void test_hat_east_sets_right() {
    uint8_t rpt[] = {0x00, 0x00, 2, 128, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_FALSE(ps.state.button.up);
    TEST_ASSERT_TRUE(ps.state.button.right);
    TEST_ASSERT_FALSE(ps.state.button.down);
    TEST_ASSERT_FALSE(ps.state.button.left);
}

void test_hat_south_sets_down() {
    uint8_t rpt[] = {0x00, 0x00, 4, 128, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_FALSE(ps.state.button.up);
    TEST_ASSERT_FALSE(ps.state.button.right);
    TEST_ASSERT_TRUE(ps.state.button.down);
    TEST_ASSERT_FALSE(ps.state.button.left);
}

void test_hat_west_sets_left() {
    uint8_t rpt[] = {0x00, 0x00, 6, 128, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_FALSE(ps.state.button.up);
    TEST_ASSERT_FALSE(ps.state.button.right);
    TEST_ASSERT_FALSE(ps.state.button.down);
    TEST_ASSERT_TRUE(ps.state.button.left);
}

void test_hat_northeast_sets_up_and_right() {
    uint8_t rpt[] = {0x00, 0x00, 1, 128, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_TRUE(ps.state.button.up);
    TEST_ASSERT_TRUE(ps.state.button.right);
    TEST_ASSERT_FALSE(ps.state.button.down);
    TEST_ASSERT_FALSE(ps.state.button.left);
}

void test_hat_centre_clears_all_dpad() {
    uint8_t rpt[] = {0x00, 0x00, 8, 128, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_FALSE(ps.state.button.up);
    TEST_ASSERT_FALSE(ps.state.button.right);
    TEST_ASSERT_FALSE(ps.state.button.down);
    TEST_ASSERT_FALSE(ps.state.button.left);
}

// ---- Report-ID prefix handling -------------------------------------------

void test_report_id_prefix_skipped_when_first_byte_is_0x01() {
    // 10-byte report: report ID 0x01 + 9 payload bytes.
    uint8_t rpt[] = {0x01, 0x01, 0x00, 0x08, 128, 128, 128, 128, 0, 0};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_TRUE(ps.state.button.cross); // b0=0x01 after skipping report ID
    TEST_ASSERT_TRUE(ps.notified);
}

void test_short_report_ignored() {
    // 4 bytes — too short for either layout; must not notify.
    uint8_t rpt[] = {0x00, 0x00, 0x00, 0x00};
    ParseState ps;
    parse_report(ps, rpt, sizeof(rpt));
    TEST_ASSERT_FALSE(ps.notified);
}

// ---------------------------------------------------------------------------
// Button dispatch (issue #203) — mirror of BTGamepad::_checkLongPress() /
// _dispatchButtons() in src/bt_gamepad.cpp. Same "hand-duplicated, must stay
// in sync" approach as parse_report() above, for the same reason: the real
// class depends on BLE headers that don't compile natively, and the real
// dispatch target is AmidalaController, which pulls in the whole firmware.
// A fake driver stands in for AmidalaController here, recording calls
// instead of acting on them.
// ---------------------------------------------------------------------------

struct FakeDriver {
    int altbtn = 0;
    int mutebutton = 0;
    bool altHeld = false;

    int buttonUpCount[6]   = {0};  // 1-based, index 0 unused
    int altButtonCount[6]  = {0};
    int longButtonCount[6] = {0};
    int muteBtnUpCount     = 0;

    bool isAltHeld() const { return altHeld; }
    void setAltHeld(bool held) { altHeld = held; }
    void noteButtonUp(int num)     { buttonUpCount[num]++; }
    void processAltButton(int num) { altButtonCount[num]++; }
    void processLongButton(int num){ longButtonCount[num]++; }
    void noteMuteBtnUp()           { muteBtnUpCount++; }
};

struct LongPress { uint32_t pressTime = 0; bool longPress = false; };
struct LongPressSet { LongPress l3, triangle, circle, cross, square; };

static bool checkLongPress(LongPress& lp, bool down, bool& up, bool held) {
    static const uint32_t kLongPressTime = 3000;
    bool longUp = false;
    if (down) {
        lp.pressTime = millis();
        lp.longPress = false;
    } else if (up) {
        lp.pressTime = 0;
        if (lp.longPress) up = false;
        lp.longPress = false;
    } else if (lp.pressTime != 0 && held) {
        if (lp.pressTime + kLongPressTime < millis()) {
            lp.pressTime = 0;
            lp.longPress = true;
            longUp = true;
        }
    }
    return longUp;
}

static void dispatchButtons(FakeDriver& driver, LongPressSet& lp,
                             const JoystickController::State& prev,
                             const JoystickController::State& state) {
    bool up_triangle = prev.button.triangle && !state.button.triangle;
    bool up_circle   = prev.button.circle   && !state.button.circle;
    bool up_cross    = prev.button.cross    && !state.button.cross;
    bool up_square   = prev.button.square   && !state.button.square;
    bool up_l3       = prev.button.l3       && !state.button.l3;

    bool down_triangle = !prev.button.triangle && state.button.triangle;
    bool down_circle   = !prev.button.circle   && state.button.circle;
    bool down_cross    = !prev.button.cross    && state.button.cross;
    bool down_square   = !prev.button.square   && state.button.square;
    bool down_l3       = !prev.button.l3       && state.button.l3;

    bool long_triangle = checkLongPress(lp.triangle, down_triangle, up_triangle, state.button.triangle);
    bool long_circle   = checkLongPress(lp.circle,   down_circle,   up_circle,   state.button.circle);
    bool long_cross    = checkLongPress(lp.cross,    down_cross,    up_cross,    state.button.cross);
    bool long_square   = checkLongPress(lp.square,   down_square,   up_square,   state.button.square);
    bool long_l3       = checkLongPress(lp.l3,       down_l3,       up_l3,       state.button.l3);

    int altbtn = driver.altbtn;
    if (altbtn >= 1 && altbtn <= 5) {
        bool held = (altbtn == 1) ? state.button.triangle :
                    (altbtn == 2) ? state.button.circle   :
                    (altbtn == 3) ? state.button.cross    :
                    (altbtn == 4) ? state.button.square   : state.button.l3;
        driver.setAltHeld(held);
    }
    bool altHeld = driver.isAltHeld();

#define BT_DISPATCH(name, num) \
    if (up_##name && altbtn != (num)) { \
        altHeld ? driver.processAltButton(num) : driver.noteButtonUp(num); \
    } \
    if (long_##name && altbtn != (num) && !altHeld) driver.processLongButton(num);

    BT_DISPATCH(triangle, 1)
    BT_DISPATCH(circle,   2)
    BT_DISPATCH(cross,    3)
    BT_DISPATCH(square,   4)
    BT_DISPATCH(l3,       5)
#undef BT_DISPATCH

    int muteBtn = driver.mutebutton;
    if (muteBtn >= 1 && muteBtn <= 5) {
        bool muteUp = (muteBtn == 1) ? up_triangle : (muteBtn == 2) ? up_circle :
                      (muteBtn == 3) ? up_cross    : (muteBtn == 4) ? up_square : up_l3;
        if (muteUp) driver.noteMuteBtnUp();
    }
}

void test_dispatch_triangle_press_release_fires_slot1() {
    FakeDriver driver;
    LongPressSet lp;
    JoystickController::State prev = {}, state = {};

    state.button.triangle = 1;
    dispatchButtons(driver, lp, prev, state);  // down: no dispatch yet
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[1]);

    prev = state;
    state.button.triangle = 0;
    dispatchButtons(driver, lp, prev, state);  // up: slot 1 fires
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[1]);
}

void test_dispatch_l3_press_release_fires_slot5() {
    FakeDriver driver;
    LongPressSet lp;
    JoystickController::State prev = {}, state = {};

    state.button.l3 = 1;
    dispatchButtons(driver, lp, prev, state);
    prev = state;
    state.button.l3 = 0;
    dispatchButtons(driver, lp, prev, state);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[5]);
}

void test_dispatch_altbtn_suppresses_its_own_button_and_sets_held() {
    FakeDriver driver;
    driver.altbtn = 1;  // triangle is the alt modifier
    LongPressSet lp;
    JoystickController::State prev = {}, state = {};

    state.button.triangle = 1;  // hold alt
    dispatchButtons(driver, lp, prev, state);
    TEST_ASSERT_TRUE(driver.isAltHeld());

    prev = state;
    state.button.triangle = 0;  // release alt -- must NOT dispatch slot 1
    dispatchButtons(driver, lp, prev, state);
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[1]);
    TEST_ASSERT_EQUAL(0, driver.altButtonCount[1]);
}

void test_dispatch_routes_to_alt_layer_while_alt_held() {
    FakeDriver driver;
    driver.altbtn = 1;  // triangle is the alt modifier
    LongPressSet lp;
    JoystickController::State prev = {}, state = {};

    // Hold triangle (alt), then press+release circle.
    state.button.triangle = 1;
    dispatchButtons(driver, lp, prev, state);
    prev = state;
    state.button.circle = 1;
    dispatchButtons(driver, lp, prev, state);
    prev = state;
    state.button.circle = 0;
    dispatchButtons(driver, lp, prev, state);

    TEST_ASSERT_EQUAL(1, driver.altButtonCount[2]);
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[2]);
}

void test_dispatch_long_press_fires_after_threshold_and_suppresses_up() {
    FakeDriver driver;
    LongPressSet lp;
    JoystickController::State prev = {}, state = {};

    // Starts at t=1, not t=0 -- a pressTime of exactly 0 is indistinguishable
    // from "no press in progress" in both this and the real long-press timer
    // (xbee_remote.h has the same property), so t=0 itself can't be used here.
    mock_millis_value = 1;
    state.button.cross = 1;
    dispatchButtons(driver, lp, prev, state);  // down at t=1
    prev = state;  // next "report" arrives with cross still held

    mock_millis_value = 3002;  // past the 3000ms long-press threshold
    dispatchButtons(driver, lp, prev, state);  // still held -- long-press fires
    TEST_ASSERT_EQUAL(1, driver.longButtonCount[3]);

    prev = state;
    state.button.cross = 0;
    dispatchButtons(driver, lp, prev, state);  // release after a long-press
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[3]);  // suppressed, same as XBee
}

void test_dispatch_mutebutton_slot_notifies_on_up() {
    FakeDriver driver;
    driver.mutebutton = 4;  // square
    LongPressSet lp;
    JoystickController::State prev = {}, state = {};

    state.button.square = 1;
    dispatchButtons(driver, lp, prev, state);
    prev = state;
    state.button.square = 0;
    dispatchButtons(driver, lp, prev, state);

    TEST_ASSERT_EQUAL(1, driver.muteBtnUpCount);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[4]);  // mute is additive, not exclusive
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Generic gamepad layout
    RUN_TEST(test_generic_sticks_centred);
    RUN_TEST(test_generic_left_stick_full_right);
    RUN_TEST(test_generic_left_stick_full_left);
    RUN_TEST(test_generic_right_stick_mapped);
    RUN_TEST(test_generic_cross_button);
    RUN_TEST(test_generic_all_face_buttons);
    RUN_TEST(test_generic_shoulder_buttons);
    RUN_TEST(test_generic_start_select);
    RUN_TEST(test_generic_thumbstick_buttons);
    RUN_TEST(test_generic_triggers);

    // Hat / d-pad
    RUN_TEST(test_hat_north_sets_up);
    RUN_TEST(test_hat_east_sets_right);
    RUN_TEST(test_hat_south_sets_down);
    RUN_TEST(test_hat_west_sets_left);
    RUN_TEST(test_hat_northeast_sets_up_and_right);
    RUN_TEST(test_hat_centre_clears_all_dpad);

    // Report-ID and edge cases
    RUN_TEST(test_report_id_prefix_skipped_when_first_byte_is_0x01);
    RUN_TEST(test_short_report_ignored);

    // Button dispatch (issue #203)
    RUN_TEST(test_dispatch_triangle_press_release_fires_slot1);
    RUN_TEST(test_dispatch_l3_press_release_fires_slot5);
    RUN_TEST(test_dispatch_altbtn_suppresses_its_own_button_and_sets_held);
    RUN_TEST(test_dispatch_routes_to_alt_layer_while_alt_held);
    RUN_TEST(test_dispatch_long_press_fires_after_threshold_and_suppresses_up);
    RUN_TEST(test_dispatch_mutebutton_slot_notifies_on_up);

    return UNITY_END();
}
