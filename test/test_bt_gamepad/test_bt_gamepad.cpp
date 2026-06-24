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
#include "JoystickController.h"

void setUp(void)    {}
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

    return UNITY_END();
}
