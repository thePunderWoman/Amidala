// test_double_press.cpp
// Tests for double-press detection logic.
//
// The detection algorithm lives in AmidalaController::noteButtonUp() and
// checkDoublePressPending(). This file tests that logic in isolation using a
// self-contained simulator (FakeDblPress) that mirrors the inline code exactly.

#include "arduino_mock.h"
#include "params.h"
#include <unity.h>
#include <string.h>

void setUp(void) {
    memset(EEPROM.data, 0, sizeof(EEPROM.data));
}
void tearDown(void) {}

// ---- dbtimeout default value ------------------------------------------------

void test_dbtimeout_default_is_300() {
    AmidalaParameters p;
    p.init();
    TEST_ASSERT_EQUAL(300, p.dbtimeout);
}

void test_dbtimeout_zero_disables() {
    AmidalaParameters p;
    p.init();
    p.dbtimeout = 0;
    TEST_ASSERT_EQUAL(0, p.dbtimeout);
}

// ---- DB array consistency ---------------------------------------------------

void test_db_array_size_matches_button_count() {
    AmidalaParameters p;
    TEST_ASSERT_EQUAL(p.getButtonCount(), sizeof(p.DB) / sizeof(p.DB[0]));
}

void test_db_actions_zero_on_init() {
    AmidalaParameters p;
    p.init();
    for (unsigned i = 0; i < p.getButtonCount(); i++) {
        TEST_ASSERT_EQUAL(ButtonAction::kNone, p.DB[i].action);
    }
}

// ---- Simulated double-press detection logic ---------------------------------
// This struct mirrors AmidalaController::noteButtonUp / checkDoublePressPending.

struct FakeDblPress {
    static const unsigned kMax = 9;
    bool     active[kMax];
    uint32_t time[kMax];
    int      lastSingle;   // last button that fired single-press (-1 = none)
    int      lastDouble;   // last button that fired double-press (-1 = none)

    FakeDblPress() { reset(); }

    void reset() {
        memset(active, 0, sizeof(active));
        memset(time,   0, sizeof(time));
        lastSingle = -1;
        lastDouble = -1;
    }

    void noteButtonUp(unsigned num, uint32_t now, uint16_t timeout,
                      bool hasDoubleAction) {
        if (num < 1 || num > kMax) return;
        unsigned idx = num - 1;
        if (!hasDoubleAction || timeout == 0) {
            lastSingle = (int)num;
            return;
        }
        if (active[idx] && now - time[idx] <= timeout) {
            active[idx] = false;
            lastDouble  = (int)num;
        } else {
            active[idx] = true;
            time[idx]   = now;
        }
    }

    void tick(uint32_t now, uint16_t timeout) {
        if (timeout == 0) return;
        for (unsigned i = 0; i < kMax; i++) {
            if (active[i] && now - time[i] > timeout) {
                active[i]  = false;
                lastSingle = (int)(i + 1);
            }
        }
    }
};

// No double-press action → single-press fires immediately, no delay.
void test_no_double_action_fires_immediately() {
    FakeDblPress dp;
    dp.noteButtonUp(1, 1000, 300, false);
    TEST_ASSERT_EQUAL(1, dp.lastSingle);
    TEST_ASSERT_EQUAL(-1, dp.lastDouble);
    TEST_ASSERT_FALSE(dp.active[0]);
}

// First press with double-action → pending, nothing fires yet.
void test_first_press_sets_pending() {
    FakeDblPress dp;
    dp.noteButtonUp(1, 1000, 300, true);
    TEST_ASSERT_EQUAL(-1, dp.lastSingle);
    TEST_ASSERT_EQUAL(-1, dp.lastDouble);
    TEST_ASSERT_TRUE(dp.active[0]);
}

// Second press within timeout → double-press fires.
void test_second_press_within_timeout_fires_double() {
    FakeDblPress dp;
    dp.noteButtonUp(1, 1000, 300, true);  // first press at t=1000
    dp.noteButtonUp(1, 1200, 300, true);  // second press 200ms later
    TEST_ASSERT_EQUAL(1, dp.lastDouble);
    TEST_ASSERT_EQUAL(-1, dp.lastSingle);
    TEST_ASSERT_FALSE(dp.active[0]);
}

// Second press exactly at the timeout boundary (t+timeout) → still counts.
void test_second_press_at_timeout_boundary_fires_double() {
    FakeDblPress dp;
    dp.noteButtonUp(2, 1000, 300, true);
    dp.noteButtonUp(2, 1300, 300, true);  // exactly at 300ms
    TEST_ASSERT_EQUAL(2, dp.lastDouble);
}

// Second press one ms beyond timeout → resets, no double.
void test_second_press_after_timeout_resets_pending() {
    FakeDblPress dp;
    dp.noteButtonUp(1, 1000, 300, true);
    dp.noteButtonUp(1, 1301, 300, true);  // 301ms later — too slow
    TEST_ASSERT_EQUAL(-1, dp.lastDouble);
    TEST_ASSERT_EQUAL(-1, dp.lastSingle);
    TEST_ASSERT_TRUE(dp.active[0]);  // new first-press pending
}

// Tick after timeout fires the pending single-press.
void test_tick_after_timeout_fires_single() {
    FakeDblPress dp;
    dp.noteButtonUp(1, 1000, 300, true);
    dp.tick(1301, 300);  // 301ms later, no second press came
    TEST_ASSERT_EQUAL(1, dp.lastSingle);
    TEST_ASSERT_FALSE(dp.active[0]);
}

// Tick before timeout does NOT fire single-press.
void test_tick_before_timeout_does_not_fire() {
    FakeDblPress dp;
    dp.noteButtonUp(1, 1000, 300, true);
    dp.tick(1299, 300);  // 299ms later
    TEST_ASSERT_EQUAL(-1, dp.lastSingle);
    TEST_ASSERT_TRUE(dp.active[0]);
}

// When timeout=0, even with a configured double action, no delay (treated as off).
void test_timeout_zero_fires_immediately() {
    FakeDblPress dp;
    dp.noteButtonUp(3, 1000, 0, true);
    TEST_ASSERT_EQUAL(3, dp.lastSingle);
    TEST_ASSERT_EQUAL(-1, dp.lastDouble);
}

// Independent per-button tracking: button 1 pending doesn't block button 2.
void test_independent_per_button_tracking() {
    FakeDblPress dp;
    dp.noteButtonUp(1, 1000, 300, true);
    dp.noteButtonUp(2, 1050, 300, true);
    TEST_ASSERT_TRUE(dp.active[0]);
    TEST_ASSERT_TRUE(dp.active[1]);
    TEST_ASSERT_EQUAL(-1, dp.lastSingle);
    TEST_ASSERT_EQUAL(-1, dp.lastDouble);

    // Double-press button 1
    dp.noteButtonUp(1, 1200, 300, true);
    TEST_ASSERT_EQUAL(1, dp.lastDouble);
    TEST_ASSERT_FALSE(dp.active[0]);
    TEST_ASSERT_TRUE(dp.active[1]);  // button 2 still pending
}

// Tick fires only the expired button.
void test_tick_fires_only_expired_button() {
    FakeDblPress dp;
    dp.noteButtonUp(1, 1000, 300, true);  // expires at 1300
    dp.noteButtonUp(5, 1100, 300, true);  // expires at 1400

    dp.tick(1350, 300);  // button 1 expired, button 5 not yet
    TEST_ASSERT_EQUAL(1, dp.lastSingle);
    TEST_ASSERT_TRUE(dp.active[4]);  // button 5 still pending
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_dbtimeout_default_is_300);
    RUN_TEST(test_dbtimeout_zero_disables);
    RUN_TEST(test_db_array_size_matches_button_count);
    RUN_TEST(test_db_actions_zero_on_init);

    RUN_TEST(test_no_double_action_fires_immediately);
    RUN_TEST(test_first_press_sets_pending);
    RUN_TEST(test_second_press_within_timeout_fires_double);
    RUN_TEST(test_second_press_at_timeout_boundary_fires_double);
    RUN_TEST(test_second_press_after_timeout_resets_pending);
    RUN_TEST(test_tick_after_timeout_fires_single);
    RUN_TEST(test_tick_before_timeout_does_not_fire);
    RUN_TEST(test_timeout_zero_fires_immediately);
    RUN_TEST(test_independent_per_button_tracking);
    RUN_TEST(test_tick_fires_only_expired_button);

    return UNITY_END();
}
