// test_button_actions.cpp
// Tests for include/button_actions.h:
//   ButtonAction  — action type constants, union field access, printDescription
//   GestureAction — gesture + action pairing, printDescription
//   SerialString  — struct layout

#include "arduino_mock.h"
#include "button_actions.h"
#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// ---- Action type constants --------------------------------------------------

void test_action_type_constants() {
    TEST_ASSERT_EQUAL(0, ButtonAction::kNone);
    TEST_ASSERT_EQUAL(1, ButtonAction::kSound);
    TEST_ASSERT_EQUAL(2, ButtonAction::kServo);
    TEST_ASSERT_EQUAL(3, ButtonAction::kDigitalOut);
    TEST_ASSERT_EQUAL(4, ButtonAction::kI2CCmd);
    TEST_ASSERT_EQUAL(5, ButtonAction::kSerialStr);
    TEST_ASSERT_EQUAL(6, ButtonAction::kI2CStr);
    TEST_ASSERT_EQUAL(7, ButtonAction::kHCREmote);
    TEST_ASSERT_EQUAL(8, ButtonAction::kHCRMuse);
    TEST_ASSERT_EQUAL(9, ButtonAction::kDomeCmd);
}

// ---- DomeCmdType sub-command constants --------------------------------------

void test_dome_cmd_type_constants() {
    TEST_ASSERT_EQUAL(0, ButtonAction::kDomeRand);
    TEST_ASSERT_EQUAL(1, ButtonAction::kDomeStop);
    TEST_ASSERT_EQUAL(2, ButtonAction::kDomeFront);
    TEST_ASSERT_EQUAL(3, ButtonAction::kDomeHome);
    TEST_ASSERT_EQUAL(4, ButtonAction::kDomeCalibrate);
    TEST_ASSERT_EQUAL(5, ButtonAction::kDomeGotoAbs);
    TEST_ASSERT_EQUAL(6, ButtonAction::kDomeRelPos);
    TEST_ASSERT_EQUAL(7, ButtonAction::kDomeRelNeg);
    TEST_ASSERT_EQUAL(8, ButtonAction::kDomeAbsStick);
}

// ---- HCR emotion constants (provided by header in native builds) ------------

void test_hcr_emotion_constants() {
    TEST_ASSERT_EQUAL(0, HAPPY);
    TEST_ASSERT_EQUAL(1, SAD);
    TEST_ASSERT_EQUAL(2, MAD);
    TEST_ASSERT_EQUAL(3, SCARED);
    TEST_ASSERT_EQUAL(4, OVERLOAD);
    TEST_ASSERT_EQUAL(0, EMOTE_MODERATE);
    TEST_ASSERT_EQUAL(1, EMOTE_STRONG);
}

// ---- Union field access -----------------------------------------------------

void test_zeroed_button_action_is_none() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    TEST_ASSERT_EQUAL(ButtonAction::kNone, b.action);
}

void test_sound_fields_accessible() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action = ButtonAction::kSound;
    b.sound.soundbank = 3;
    b.sound.sound     = 7;
    b.serialid        = 2;
    TEST_ASSERT_EQUAL(3, b.sound.soundbank);
    TEST_ASSERT_EQUAL(7, b.sound.sound);
    TEST_ASSERT_EQUAL(2, b.serialid);
}

void test_servo_fields_accessible() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action     = ButtonAction::kServo;
    b.servo.num  = 4;
    b.servo.pos  = 90;
    TEST_ASSERT_EQUAL(4,  b.servo.num);
    TEST_ASSERT_EQUAL(90, b.servo.pos);
}

void test_dout_fields_accessible() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action     = ButtonAction::kDigitalOut;
    b.dout.num   = 2;
    b.dout.state = 1;
    TEST_ASSERT_EQUAL(2, b.dout.num);
    TEST_ASSERT_EQUAL(1, b.dout.state);
}

void test_emote_fields_accessible() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action       = ButtonAction::kHCREmote;
    b.emote.emotion = SAD;
    b.emote.level   = EMOTE_STRONG;
    TEST_ASSERT_EQUAL(SAD,          b.emote.emotion);
    TEST_ASSERT_EQUAL(EMOTE_STRONG, b.emote.level);
}

void test_dome_fields_accessible() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action        = ButtonAction::kDomeCmd;
    b.dome.subcmd   = ButtonAction::kDomeGotoAbs;
    b.dome.arg      = 90;
    TEST_ASSERT_EQUAL(ButtonAction::kDomeGotoAbs, b.dome.subcmd);
    TEST_ASSERT_EQUAL(90, b.dome.arg);
}

void test_union_members_share_storage() {
    // serialid moved out of the union (now a top-level uint16_t).
    // Most union members are now 2 bytes; i2cstr is 1 byte (only needs target).
    ButtonAction b;
    TEST_ASSERT_EQUAL(sizeof(b.sound), sizeof(b.servo));
    TEST_ASSERT_EQUAL(sizeof(b.sound), sizeof(b.dout));
    TEST_ASSERT_EQUAL(sizeof(b.sound), sizeof(b.i2ccmd));
    TEST_ASSERT_EQUAL(sizeof(b.sound), sizeof(b.serial));
    TEST_ASSERT_EQUAL(sizeof(b.sound), sizeof(b.emote));
    TEST_ASSERT_EQUAL(sizeof(b.sound), sizeof(b.dome));
    TEST_ASSERT_EQUAL(2, sizeof(b.sound));
    TEST_ASSERT_EQUAL(2, sizeof(b.serialid));  // top-level uint16_t
}

// ---- printDescription -------------------------------------------------------

void test_print_none_produces_only_newline() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action = ButtonAction::kNone;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_EQUAL_STRING("\n", out.buf);
}

void test_print_sound_bank_only() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action          = ButtonAction::kSound;
    b.sound.soundbank = 2;
    b.sound.sound     = 0;  // no specific file
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Sound Bank #"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "2"));
    // When sound==0 no comma-file is appended
    TEST_ASSERT_NULL(strstr(out.buf, ","));
}

void test_print_sound_with_specific_file() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action          = ButtonAction::kSound;
    b.sound.soundbank = 1;
    b.sound.sound     = 5;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Sound Bank #"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, ","));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "5"));
}

void test_print_servo() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action    = ButtonAction::kServo;
    b.servo.num = 3;
    b.servo.pos = 90;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Servo #"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "On Pos="));
}

void test_print_dout_no() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action     = ButtonAction::kDigitalOut;
    b.dout.num   = 1;
    b.dout.state = 0;  // Normally Open
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "DOut #"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "NO"));
}

void test_print_dout_nc() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action     = ButtonAction::kDigitalOut;
    b.dout.num   = 2;
    b.dout.state = 1;  // Normally Closed
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "NC"));
}

void test_print_dout_mon() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action     = ButtonAction::kDigitalOut;
    b.dout.num   = 3;
    b.dout.state = 2;  // Momentary
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "MON"));
}

void test_print_i2c_cmd() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action        = ButtonAction::kI2CCmd;
    b.i2ccmd.target = 99;
    b.i2ccmd.cmd    = 42;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "i2c Dest #"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Cmd="));
}

void test_print_ser_str() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action   = ButtonAction::kSerialStr;
    b.serialid = 5;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Serial Str #"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "5"));
}

void test_print_i2c_str() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action         = ButtonAction::kI2CStr;
    b.i2cstr.target  = 10;
    b.serialid       = 3;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Aux I2C Str #"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Dest "));
}

void test_print_hcr_emote_happy_moderate() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action        = ButtonAction::kHCREmote;
    b.emote.emotion = HAPPY;
    b.emote.level   = EMOTE_MODERATE;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "HCR "));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Happy"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Moderate"));
}

void test_print_hcr_emote_sad_strong() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action        = ButtonAction::kHCREmote;
    b.emote.emotion = SAD;
    b.emote.level   = EMOTE_STRONG;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Sad"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Strong"));
}

void test_print_hcr_emote_overload_has_no_level() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action        = ButtonAction::kHCREmote;
    b.emote.emotion = OVERLOAD;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Overload"));
    // OVERLOAD has no Moderate/Strong suffix
    TEST_ASSERT_NULL(strstr(out.buf, "Moderate"));
    TEST_ASSERT_NULL(strstr(out.buf, "Strong"));
}

void test_print_hcr_muse() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action = ButtonAction::kHCRMuse;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "HCR Muse Toggle"));
}

void test_print_appends_serial_string_for_non_serial_actions() {
    // When serial.serialstr != 0 and action is not kSerialStr/kHCREmote/kHCRMuse,
    // ", Serial #<n>" is appended.
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action          = ButtonAction::kSound;
    b.sound.soundbank = 1;
    b.serialid        = 3;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, ", Serial #"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "3"));
}

void test_print_dome_rand() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action      = ButtonAction::kDomeCmd;
    b.dome.subcmd = ButtonAction::kDomeRand;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Dome: "));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Random Mode"));
}

void test_print_dome_stop() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action      = ButtonAction::kDomeCmd;
    b.dome.subcmd = ButtonAction::kDomeStop;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Dome: "));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Stop"));
}

void test_print_dome_goto_abs() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action      = ButtonAction::kDomeCmd;
    b.dome.subcmd = ButtonAction::kDomeGotoAbs;
    b.dome.arg    = 90;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Dome: "));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Goto "));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "90"));
}

void test_print_dome_rel_pos() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action      = ButtonAction::kDomeCmd;
    b.dome.subcmd = ButtonAction::kDomeRelPos;
    b.dome.arg    = 45;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Relative +"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "45"));
}

void test_print_dome_rel_neg() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action      = ButtonAction::kDomeCmd;
    b.dome.subcmd = ButtonAction::kDomeRelNeg;
    b.dome.arg    = 45;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Relative -"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "45"));
}

void test_print_dome_abstick() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action      = ButtonAction::kDomeCmd;
    b.dome.subcmd = ButtonAction::kDomeAbsStick;
    StringPrint out;
    b.printDescription(&out);
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Dome: "));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Absolute-Stick Toggle"));
}

void test_print_does_not_append_serial_for_serial_action() {
    // kSerialStr itself must not double-print ", Serial #".
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.action   = ButtonAction::kSerialStr;
    b.serialid = 2;
    StringPrint out;
    b.printDescription(&out);
    // Should have "Serial Str #2" but NOT ", Serial #2" appended again
    const char *first = strstr(out.buf, "Serial Str #");
    TEST_ASSERT_NOT_NULL(first);
    // There must be no second ", Serial #" occurrence
    TEST_ASSERT_NULL(strstr(first + 1, ", Serial #"));
}

// ---- GestureAction ----------------------------------------------------------

void test_gesture_action_empty_gesture_prints_nothing() {
    GestureAction g;
    memset(&g, 0, sizeof(g));
    // gesture is empty by default (fGesture == 0)
    StringPrint out;
    g.printDescription(&out);
    TEST_ASSERT_EQUAL_STRING("", out.buf);
}

void test_gesture_action_prints_gesture_then_action() {
    GestureAction g;
    memset(&g, 0, sizeof(g));
    g.gesture.setGesture("3");
    g.action.action          = ButtonAction::kSound;
    g.action.sound.soundbank = 1;
    StringPrint out;
    g.printDescription(&out);
    // Output must start with the gesture string
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "3"));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, ": "));
    TEST_ASSERT_NOT_NULL(strstr(out.buf, "Sound Bank #"));
}

// ---- SerialString -----------------------------------------------------------

void test_serial_string_capacity() {
    // str must hold long Uppity sequences (e.g. "#PS8:H:P50:W2:P85,35:A90,25:W2:A270,20,100:W2:P100,100:L5:R50:W4:H" = 67 chars)
    TEST_ASSERT_EQUAL(100, sizeof(SerialString::str));
    // name field unchanged
    TEST_ASSERT_EQUAL(32, sizeof(SerialString::name));
}

void test_serial_string_holds_long_uppity_sequence() {
    SerialString s;
    const char* longSeq = "#PS8:H:P50:W2:P85,35:A90,25:W2:A270,20,100:W2:P100,100:L5:R50:W4:H";
    TEST_ASSERT_TRUE_MESSAGE(strlen(longSeq) < sizeof(s.str),
        "Long Uppity sequence must fit in SerialString::str");
    strncpy(s.str, longSeq, sizeof(s.str) - 1);
    s.str[sizeof(s.str) - 1] = '\0';
    TEST_ASSERT_EQUAL_STRING(longSeq, s.str);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_action_type_constants);
    RUN_TEST(test_hcr_emotion_constants);
    RUN_TEST(test_dome_cmd_type_constants);

    RUN_TEST(test_zeroed_button_action_is_none);
    RUN_TEST(test_sound_fields_accessible);
    RUN_TEST(test_servo_fields_accessible);
    RUN_TEST(test_dout_fields_accessible);
    RUN_TEST(test_emote_fields_accessible);
    RUN_TEST(test_dome_fields_accessible);
    RUN_TEST(test_union_members_share_storage);

    RUN_TEST(test_print_none_produces_only_newline);
    RUN_TEST(test_print_sound_bank_only);
    RUN_TEST(test_print_sound_with_specific_file);
    RUN_TEST(test_print_servo);
    RUN_TEST(test_print_dout_no);
    RUN_TEST(test_print_dout_nc);
    RUN_TEST(test_print_dout_mon);
    RUN_TEST(test_print_i2c_cmd);
    RUN_TEST(test_print_ser_str);
    RUN_TEST(test_print_i2c_str);
    RUN_TEST(test_print_hcr_emote_happy_moderate);
    RUN_TEST(test_print_hcr_emote_sad_strong);
    RUN_TEST(test_print_hcr_emote_overload_has_no_level);
    RUN_TEST(test_print_hcr_muse);
    RUN_TEST(test_print_dome_rand);
    RUN_TEST(test_print_dome_stop);
    RUN_TEST(test_print_dome_goto_abs);
    RUN_TEST(test_print_dome_rel_pos);
    RUN_TEST(test_print_dome_rel_neg);
    RUN_TEST(test_print_dome_abstick);
    RUN_TEST(test_print_appends_serial_string_for_non_serial_actions);
    RUN_TEST(test_print_does_not_append_serial_for_serial_action);

    RUN_TEST(test_gesture_action_empty_gesture_prints_nothing);
    RUN_TEST(test_gesture_action_prints_gesture_then_action);

    RUN_TEST(test_serial_string_capacity);
    RUN_TEST(test_serial_string_holds_long_uppity_sequence);

    return UNITY_END();
}
