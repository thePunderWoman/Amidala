// test_button_names.cpp
// Tests for include/button_names.h -- the console/serial-monitor button
// labels that mirror the Controllers page (issue #227). Previously the log
// only said "Processing Button 7", which meant nothing without knowing the
// active controllertype's numbering.

#include "arduino_mock.h"
#include "button_names.h"
#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const char *lbl(uint8_t type, unsigned num) {
    static char buf[48];
    return ButtonNames::label(type, num, buf, sizeof(buf));
}

// ---- XBee pocket remote -----------------------------------------------------

void test_xbee_right_controller_buttons() {
    TEST_ASSERT_EQUAL_STRING("Right Controller Trigger Right (1)", lbl(CONTROLLER_TYPE_XBEE, 1));
    TEST_ASSERT_EQUAL_STRING("Right Controller Stick Press (5)",   lbl(CONTROLLER_TYPE_XBEE, 5));
}

void test_xbee_left_controller_buttons() {
    TEST_ASSERT_EQUAL_STRING("Left Controller Trigger Right (6)", lbl(CONTROLLER_TYPE_XBEE, 6));
    TEST_ASSERT_EQUAL_STRING("Left Controller Trigger Left (7)",  lbl(CONTROLLER_TYPE_XBEE, 7));
    TEST_ASSERT_EQUAL_STRING("Left Controller Bottom (9)",        lbl(CONTROLLER_TYPE_XBEE, 9));
}

void test_xbee_unnamed_buttons_fall_back_to_number() {
    TEST_ASSERT_EQUAL_STRING("Button 0",  lbl(CONTROLLER_TYPE_XBEE, 0));
    TEST_ASSERT_EQUAL_STRING("Button 10", lbl(CONTROLLER_TYPE_XBEE, 10));
    TEST_ASSERT_EQUAL_STRING("Button 24", lbl(CONTROLLER_TYPE_XBEE, 24));
}

void test_rc_uses_xbee_names_like_the_web_ui() {
    TEST_ASSERT_EQUAL_STRING("Left Controller Trigger Left (7)", lbl(CONTROLLER_TYPE_RC, 7));
}

// ---- Snips Controllers ------------------------------------------------------

// The same button number means something completely different than under
// XBee -- this is exactly the confusion the issue describes.
void test_same_number_reads_differently_per_controller_type() {
    TEST_ASSERT_EQUAL_STRING("Right Controller Top (3)",        lbl(CONTROLLER_TYPE_XBEE,  3));
    TEST_ASSERT_EQUAL_STRING("Right Snips Controller Macro 3 (3)", lbl(CONTROLLER_TYPE_SNIPS, 3));
}

void test_snips_right_fire_and_forget_buttons() {
    TEST_ASSERT_EQUAL_STRING("Right Snips Controller Macro 1 (1)",     lbl(CONTROLLER_TYPE_SNIPS, 1));
    TEST_ASSERT_EQUAL_STRING("Right Snips Controller Bumper (7)",      lbl(CONTROLLER_TYPE_SNIPS, 7));
    TEST_ASSERT_EQUAL_STRING("Right Snips Controller Stick Click (8)", lbl(CONTROLLER_TYPE_SNIPS, 8));
}

void test_snips_left_fire_and_forget_buttons() {
    TEST_ASSERT_EQUAL_STRING("Left Snips Controller Macro 1 (9)",     lbl(CONTROLLER_TYPE_SNIPS, 9));
    TEST_ASSERT_EQUAL_STRING("Left Snips Controller Stick Click (16)", lbl(CONTROLLER_TYPE_SNIPS, 16));
}

// Stateful buttons are pushed past 16 for both sides: Left's are 17-20,
// Right's are 21-24 (non-contiguous with Right's 1-8).
void test_snips_stateful_buttons_split_by_side() {
    TEST_ASSERT_EQUAL_STRING("Left Snips Controller Left Up (17)",    lbl(CONTROLLER_TYPE_SNIPS, 17));
    TEST_ASSERT_EQUAL_STRING("Left Snips Controller Right Down (20)", lbl(CONTROLLER_TYPE_SNIPS, 20));
    TEST_ASSERT_EQUAL_STRING("Right Snips Controller Left Up (21)",   lbl(CONTROLLER_TYPE_SNIPS, 21));
    TEST_ASSERT_EQUAL_STRING("Right Snips Controller Right Down (24)", lbl(CONTROLLER_TYPE_SNIPS, 24));
}

void test_snips_out_of_range_falls_back() {
    TEST_ASSERT_EQUAL_STRING("Button 0",  lbl(CONTROLLER_TYPE_SNIPS, 0));
    TEST_ASSERT_EQUAL_STRING("Button 25", lbl(CONTROLLER_TYPE_SNIPS, 25));
}

// ---- Bluetooth gamepad ------------------------------------------------------

void test_bt_has_no_side_prefix() {
    TEST_ASSERT_EQUAL_STRING("A / Cross (1)",              lbl(CONTROLLER_TYPE_BLUETOOTH, 1));
    TEST_ASSERT_EQUAL_STRING("Left Stick Click (L3) (5)",  lbl(CONTROLLER_TYPE_BLUETOOTH, 5));
    TEST_ASSERT_EQUAL_STRING("Left Bumper (10)",           lbl(CONTROLLER_TYPE_BLUETOOTH, 10));
    TEST_ASSERT_EQUAL_STRING("Right Trigger (13)",         lbl(CONTROLLER_TYPE_BLUETOOTH, 13));
}

void test_bt_reserved_slots_fall_back() {
    TEST_ASSERT_EQUAL_STRING("Button 6",  lbl(CONTROLLER_TYPE_BLUETOOTH, 6));
    TEST_ASSERT_EQUAL_STRING("Button 9",  lbl(CONTROLLER_TYPE_BLUETOOTH, 9));
    TEST_ASSERT_EQUAL_STRING("Button 14", lbl(CONTROLLER_TYPE_BLUETOOTH, 14));
}

// ---- Buffer safety ----------------------------------------------------------

void test_label_truncates_instead_of_overflowing() {
    char buf[8];
    memset(buf, 'x', sizeof(buf));
    ButtonNames::label(CONTROLLER_TYPE_SNIPS, 24, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(7, strlen(buf));  // NUL-terminated within the buffer
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_xbee_right_controller_buttons);
    RUN_TEST(test_xbee_left_controller_buttons);
    RUN_TEST(test_xbee_unnamed_buttons_fall_back_to_number);
    RUN_TEST(test_rc_uses_xbee_names_like_the_web_ui);
    RUN_TEST(test_same_number_reads_differently_per_controller_type);
    RUN_TEST(test_snips_right_fire_and_forget_buttons);
    RUN_TEST(test_snips_left_fire_and_forget_buttons);
    RUN_TEST(test_snips_stateful_buttons_split_by_side);
    RUN_TEST(test_snips_out_of_range_falls_back);
    RUN_TEST(test_bt_has_no_side_prefix);
    RUN_TEST(test_bt_reserved_slots_fall_back);
    RUN_TEST(test_label_truncates_instead_of_overflowing);

    return UNITY_END();
}
