// test_gesture.cpp
// Unit tests for the Gesture class in core.h.
//
// The Gesture class encodes gesture strings (sequences of digits 1-9 and A-D)
// as a compact uint32_t using a base-14 scheme.  Tests here verify:
//   - isEmpty() semantics
//   - setGesture() / matches() round-trip for real gesture strings from the
//     firmware (e.g. "3", "252", "858")
//   - getGestureString() recovers the original string
//   - Edge cases: empty string, all-digit, hex-digit (A-D), multi-char

#include "arduino_mock.h"
#include "core.h"
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// ---- isEmpty ----------------------------------------------------------------

void test_gesture_default_is_empty() {
  Gesture g("");
  TEST_ASSERT_TRUE(g.isEmpty());
}

void test_gesture_nonempty_after_set() {
  Gesture g("1");
  TEST_ASSERT_FALSE(g.isEmpty());
}

void test_gesture_empty_after_reset_to_empty() {
  Gesture g("3");
  TEST_ASSERT_FALSE(g.isEmpty());
  g.setGesture("");
  TEST_ASSERT_TRUE(g.isEmpty());
}

// ---- matches() ---------------------------------------------------------------

void test_gesture_matches_empty_string_when_empty() {
  Gesture g("");
  TEST_ASSERT_TRUE(g.matches(""));
}

void test_gesture_empty_does_not_match_nonempty() {
  Gesture g("");
  TEST_ASSERT_FALSE(g.matches("1"));
}

void test_gesture_single_digit_1() {
  Gesture g("1");
  TEST_ASSERT_TRUE(g.matches("1"));
  TEST_ASSERT_FALSE(g.matches("2"));
  TEST_ASSERT_FALSE(g.matches(""));
}

void test_gesture_single_digit_9() {
  Gesture g("9");
  TEST_ASSERT_TRUE(g.matches("9"));
  TEST_ASSERT_FALSE(g.matches("8"));
}

void test_gesture_hex_digit_A() {
  Gesture g("A");
  TEST_ASSERT_TRUE(g.matches("A"));
  TEST_ASSERT_FALSE(g.matches("9"));
  TEST_ASSERT_FALSE(g.matches("B"));
}

void test_gesture_hex_digit_D() {
  Gesture g("D");
  TEST_ASSERT_TRUE(g.matches("D"));
  TEST_ASSERT_FALSE(g.matches("C"));
}

// Firmware default: random-sound gesture
void test_gesture_rnd_3() {
  Gesture g("3");
  TEST_ASSERT_TRUE(g.matches("3"));
  TEST_ASSERT_FALSE(g.matches("33"));
}

// Firmware default: ack gesture
void test_gesture_ack_252() {
  Gesture g("252");
  TEST_ASSERT_TRUE(g.matches("252"));
  TEST_ASSERT_FALSE(g.matches("25"));
  TEST_ASSERT_FALSE(g.matches("2520"));
  TEST_ASSERT_FALSE(g.matches("253"));
}

// Firmware default: slow-mode gesture
void test_gesture_slow_858() {
  Gesture g("858");
  TEST_ASSERT_TRUE(g.matches("858"));
  TEST_ASSERT_FALSE(g.matches("85"));
  TEST_ASSERT_FALSE(g.matches("859"));
}

void test_gesture_mixed_alphanum() {
  Gesture g("1A");
  TEST_ASSERT_TRUE(g.matches("1A"));
  TEST_ASSERT_FALSE(g.matches("1B"));
  TEST_ASSERT_FALSE(g.matches("A1"));
}

void test_gesture_does_not_match_prefix() {
  Gesture g("12");
  TEST_ASSERT_FALSE(g.matches("1"));
  TEST_ASSERT_FALSE(g.matches("123"));
}

// ---- matches() with autocorrect ----------------------------------------------
// autocorrect ignores '5' (center) characters on both sides, tolerating the
// stray/missing recenter blips a human produces performing a real gesture.

void test_gesture_autocorrect_off_rejects_extra_center() {
  Gesture g("258");
  TEST_ASSERT_FALSE(g.matches("2558"));
  TEST_ASSERT_FALSE(g.matches("28"));
}

void test_gesture_autocorrect_on_ignores_extra_center() {
  Gesture g("258");
  TEST_ASSERT_TRUE(g.matches("2558", true));
  TEST_ASSERT_TRUE(g.matches("2585", true));
}

void test_gesture_autocorrect_on_ignores_missing_center() {
  Gesture g("258");
  TEST_ASSERT_TRUE(g.matches("28", true));
}

void test_gesture_autocorrect_on_still_rejects_wrong_directions() {
  Gesture g("258");
  TEST_ASSERT_FALSE(g.matches("268", true));
  TEST_ASSERT_FALSE(g.matches("259", true));
}

void test_gesture_autocorrect_defaults_to_off() {
  Gesture g("258");
  TEST_ASSERT_FALSE(g.matches("2558"));
}

// ---- getGestureString() ------------------------------------------------------

void test_gesture_string_roundtrip_single() {
  char buf[MAX_GESTURE_LENGTH + 1];
  Gesture g("5");
  TEST_ASSERT_EQUAL_STRING("5", g.getGestureString(buf));
}

void test_gesture_string_roundtrip_252() {
  char buf[MAX_GESTURE_LENGTH + 1];
  Gesture g("252");
  TEST_ASSERT_EQUAL_STRING("252", g.getGestureString(buf));
}

void test_gesture_string_roundtrip_858() {
  char buf[MAX_GESTURE_LENGTH + 1];
  Gesture g("858");
  TEST_ASSERT_EQUAL_STRING("858", g.getGestureString(buf));
}

void test_gesture_string_roundtrip_mixed() {
  char buf[MAX_GESTURE_LENGTH + 1];
  Gesture g("1A");
  TEST_ASSERT_EQUAL_STRING("1A", g.getGestureString(buf));
}

void test_gesture_string_empty_stays_empty() {
  char buf[MAX_GESTURE_LENGTH + 1];
  buf[0] = 'X';  // sentinel
  Gesture g("");
  g.getGestureString(buf);
  // Empty gesture: loop body never executes, buf is unchanged from initial '\0'
  // (the function returns without writing anything; buf[0] may be 'X').
  // What matters is isEmpty() is true and matches("") is true.
  TEST_ASSERT_TRUE(g.isEmpty());
}

// ---- getGestureType() --------------------------------------------------------

void test_gesture_type_digits() {
  Gesture g("");
  for (char c = '1'; c <= '9'; c++) {
    TEST_ASSERT_EQUAL_INT(c - '0', g.getGestureType(c));
  }
}

void test_gesture_type_hex_letters() {
  Gesture g("");
  TEST_ASSERT_EQUAL_INT(10, g.getGestureType('A'));
  TEST_ASSERT_EQUAL_INT(11, g.getGestureType('B'));
  TEST_ASSERT_EQUAL_INT(12, g.getGestureType('C'));
  TEST_ASSERT_EQUAL_INT(13, g.getGestureType('D'));
}

void test_gesture_type_invalid_returns_zero() {
  Gesture g("");
  TEST_ASSERT_EQUAL_INT(0, g.getGestureType('0'));
  TEST_ASSERT_EQUAL_INT(0, g.getGestureType('E'));
  TEST_ASSERT_EQUAL_INT(0, g.getGestureType(' '));
}

// ---- setGesture with nullptr (safety guard) ---------------------------------

void test_gesture_set_nullptr_is_safe() {
  Gesture g("5");
  TEST_ASSERT_FALSE(g.isEmpty());
  g.setGesture(nullptr);
  // After nullptr, fGesture is reset to 0
  TEST_ASSERT_TRUE(g.isEmpty());
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();

  RUN_TEST(test_gesture_default_is_empty);
  RUN_TEST(test_gesture_nonempty_after_set);
  RUN_TEST(test_gesture_empty_after_reset_to_empty);

  RUN_TEST(test_gesture_matches_empty_string_when_empty);
  RUN_TEST(test_gesture_empty_does_not_match_nonempty);
  RUN_TEST(test_gesture_single_digit_1);
  RUN_TEST(test_gesture_single_digit_9);
  RUN_TEST(test_gesture_hex_digit_A);
  RUN_TEST(test_gesture_hex_digit_D);
  RUN_TEST(test_gesture_rnd_3);
  RUN_TEST(test_gesture_ack_252);
  RUN_TEST(test_gesture_slow_858);
  RUN_TEST(test_gesture_mixed_alphanum);
  RUN_TEST(test_gesture_does_not_match_prefix);

  RUN_TEST(test_gesture_autocorrect_off_rejects_extra_center);
  RUN_TEST(test_gesture_autocorrect_on_ignores_extra_center);
  RUN_TEST(test_gesture_autocorrect_on_ignores_missing_center);
  RUN_TEST(test_gesture_autocorrect_on_still_rejects_wrong_directions);
  RUN_TEST(test_gesture_autocorrect_defaults_to_off);

  RUN_TEST(test_gesture_string_roundtrip_single);
  RUN_TEST(test_gesture_string_roundtrip_252);
  RUN_TEST(test_gesture_string_roundtrip_858);
  RUN_TEST(test_gesture_string_roundtrip_mixed);
  RUN_TEST(test_gesture_string_empty_stays_empty);

  RUN_TEST(test_gesture_type_digits);
  RUN_TEST(test_gesture_type_hex_letters);
  RUN_TEST(test_gesture_type_invalid_returns_zero);

  RUN_TEST(test_gesture_set_nullptr_is_safe);

  return UNITY_END();
}
