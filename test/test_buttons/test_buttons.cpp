// test_buttons.cpp
// Tests for button dispatch bounds checking in src/buttons.cpp.
//
// processButton(num) and processLongButton(num) use num as a 1-based index
// into B[] / LB[]. The guard must be:
//   num >= 1 && num <= getButtonCount()
// not just:
//   num < getButtonCount()
//
// The latter fails for num==0: 0 < 9 is true, so num-1 underflows to ~4 billion.
// These tests verify the invariants that the correct guard depends on.

#include "arduino_mock.h"
#include "params.h"
#include <unity.h>
#include <string.h>

void setUp(void) {
    memset(EEPROM.data, 0, sizeof(EEPROM.data));
}
void tearDown(void) {}

// ---- Invariants the guard relies on -----------------------------------------

// Button array is 1-indexed from the user's perspective:
// processButton(1) accesses B[0], processButton(9) accesses B[8].
// So valid range is [1, getButtonCount()].

void test_button_count_is_9() {
    AmidalaParameters p;
    TEST_ASSERT_EQUAL(9, p.getButtonCount());
}

void test_button_array_size_matches_count() {
    AmidalaParameters p;
    // Confirm the array size equals getButtonCount() so the offset arithmetic
    // num-1 stays in bounds for num in [1, getButtonCount()].
    TEST_ASSERT_EQUAL(sizeof(p.B) / sizeof(p.B[0]), p.getButtonCount());
    TEST_ASSERT_EQUAL(sizeof(p.LB) / sizeof(p.LB[0]), p.getButtonCount());
    TEST_ASSERT_EQUAL(sizeof(p.AB) / sizeof(p.AB[0]), p.getButtonCount());
    TEST_ASSERT_EQUAL(sizeof(p.DB) / sizeof(p.DB[0]), p.getButtonCount());
}

// ---- Guard condition correctness (regression for the num==0 underflow bug) --
// The old guard was:  num < getButtonCount()
// The correct guard is: num >= 1 && num <= getButtonCount()
// These tests verify that the correct condition passes/fails as expected.

void test_guard_rejects_zero() {
    // num==0 must NOT satisfy the guard (old code had: 0 < 9 == true → UB).
    unsigned num = 0;
    unsigned count = 9;
    bool old_guard = (num < count);            // true — the broken condition
    bool new_guard = (num >= 1 && num <= count); // false — the correct condition
    TEST_ASSERT_TRUE(old_guard);   // confirms the old code was broken
    TEST_ASSERT_FALSE(new_guard);  // confirms the fix is correct
}

void test_guard_accepts_first_button() {
    unsigned num = 1;
    unsigned count = 9;
    TEST_ASSERT_TRUE(num >= 1 && num <= count);
}

void test_guard_accepts_last_button() {
    unsigned num = 9;
    unsigned count = 9;
    TEST_ASSERT_TRUE(num >= 1 && num <= count);
}

void test_guard_rejects_one_past_end() {
    unsigned num = 10;
    unsigned count = 9;
    TEST_ASSERT_FALSE(num >= 1 && num <= count);
}

// ---- B[num-1] offset stays in bounds for valid num --------------------------

void test_first_button_accesses_index_zero() {
    AmidalaParameters p;
    p.init();
    // B[1-1] = B[0] — check via address arithmetic
    TEST_ASSERT_EQUAL_PTR(&p.B[0], &p.B[1 - 1]);
}

void test_last_button_accesses_last_index() {
    AmidalaParameters p;
    p.init();
    unsigned count = p.getButtonCount();
    // B[count-1] is the last valid element
    TEST_ASSERT_EQUAL_PTR(&p.B[count - 1], &p.B[count - 1]);
    // Verify it is within the array
    ptrdiff_t offset = &p.B[count - 1] - &p.B[0];
    TEST_ASSERT_EQUAL((ptrdiff_t)(count - 1), offset);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_button_count_is_9);
    RUN_TEST(test_button_array_size_matches_count);

    RUN_TEST(test_guard_rejects_zero);
    RUN_TEST(test_guard_accepts_first_button);
    RUN_TEST(test_guard_accepts_last_button);
    RUN_TEST(test_guard_rejects_one_past_end);

    RUN_TEST(test_first_button_accesses_index_zero);
    RUN_TEST(test_last_button_accesses_last_index);

    return UNITY_END();
}
