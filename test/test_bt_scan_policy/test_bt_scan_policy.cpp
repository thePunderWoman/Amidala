// test_bt_scan_policy.cpp
// Unit tests for BTScanPolicy (include/bt_scan_policy.h).
//
// Regression coverage for the WiFi-connect crash caused by BTGamepad
// scanning for "any" HID device forever whenever no target was paired
// (which is exactly what happened when btcontrolleron defaulted to true
// with an empty btaddr) -- the continuous BLE scan/reconnect cycle
// collided with WiFi's internal timer allocation and crashed the board.

#include "bt_scan_policy.h"
#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Reconnect-to-known-target backoff

void test_reconnect_due_immediately_on_first_call() {
    BTScanPolicy p;
    TEST_ASSERT_TRUE(p.reconnectDue(1000));
}

void test_reconnect_not_due_before_interval_elapses() {
    BTScanPolicy p;
    TEST_ASSERT_TRUE(p.reconnectDue(0));
    TEST_ASSERT_FALSE(p.reconnectDue(5000)); // < 10s base interval
}

void test_reconnect_due_again_after_interval_elapses() {
    BTScanPolicy p;
    TEST_ASSERT_TRUE(p.reconnectDue(0));
    TEST_ASSERT_TRUE(p.reconnectDue(10000));
}

void test_reconnect_backoff_doubles_and_caps() {
    BTScanPolicy p;
    TEST_ASSERT_EQUAL_UINT32(10000, p.reconnectIntervalMs());
    p.onReconnectFailed();
    TEST_ASSERT_EQUAL_UINT32(20000, p.reconnectIntervalMs());
    p.onReconnectFailed();
    TEST_ASSERT_EQUAL_UINT32(40000, p.reconnectIntervalMs());
    p.onReconnectFailed();
    TEST_ASSERT_EQUAL_UINT32(60000, p.reconnectIntervalMs()); // capped, not 80000
    p.onReconnectFailed();
    TEST_ASSERT_EQUAL_UINT32(60000, p.reconnectIntervalMs()); // stays capped
}

void test_reconnect_due_respects_backed_off_interval() {
    BTScanPolicy p;
    TEST_ASSERT_TRUE(p.reconnectDue(0));
    p.onReconnectFailed(); // interval now 20000
    TEST_ASSERT_FALSE(p.reconnectDue(15000));
    TEST_ASSERT_TRUE(p.reconnectDue(20000));
}

void test_onPaired_resets_backoff() {
    BTScanPolicy p;
    p.onReconnectFailed();
    p.onReconnectFailed();
    TEST_ASSERT_EQUAL_UINT32(40000, p.reconnectIntervalMs());
    p.onPaired();
    TEST_ASSERT_EQUAL_UINT32(10000, p.reconnectIntervalMs());
}

// ---------------------------------------------------------------------------
// Explicit pairing mode is bounded by a timeout (never scans forever)

void test_pairing_active_immediately_after_begin() {
    BTScanPolicy p;
    p.beginPairing(1000);
    TEST_ASSERT_TRUE(p.isPairing());
    TEST_ASSERT_TRUE(p.pairingActive(1000));
}

void test_pairing_active_before_timeout() {
    BTScanPolicy p;
    p.beginPairing(0);
    TEST_ASSERT_TRUE(p.pairingActive(29000)); // < 30s timeout
}

void test_pairing_times_out_and_clears_pairing_flag() {
    BTScanPolicy p;
    p.beginPairing(0);
    TEST_ASSERT_FALSE(p.pairingActive(30001)); // > 30s timeout
    TEST_ASSERT_FALSE(p.isPairing());
}

void test_onPaired_ends_pairing_mode() {
    BTScanPolicy p;
    p.beginPairing(0);
    TEST_ASSERT_TRUE(p.isPairing());
    p.onPaired();
    TEST_ASSERT_FALSE(p.isPairing());
}

// ---------------------------------------------------------------------------
// reset() returns to a fresh, non-pairing, base-interval state

void test_reset_clears_pairing_and_backoff() {
    BTScanPolicy p;
    p.beginPairing(0);
    p.onReconnectFailed();
    p.onReconnectFailed();
    p.reset();
    TEST_ASSERT_FALSE(p.isPairing());
    TEST_ASSERT_EQUAL_UINT32(10000, p.reconnectIntervalMs());
    TEST_ASSERT_TRUE(p.reconnectDue(0)); // due immediately again
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_reconnect_due_immediately_on_first_call);
    RUN_TEST(test_reconnect_not_due_before_interval_elapses);
    RUN_TEST(test_reconnect_due_again_after_interval_elapses);
    RUN_TEST(test_reconnect_backoff_doubles_and_caps);
    RUN_TEST(test_reconnect_due_respects_backed_off_interval);
    RUN_TEST(test_onPaired_resets_backoff);

    RUN_TEST(test_pairing_active_immediately_after_begin);
    RUN_TEST(test_pairing_active_before_timeout);
    RUN_TEST(test_pairing_times_out_and_clears_pairing_flag);
    RUN_TEST(test_onPaired_ends_pairing_mode);

    RUN_TEST(test_reset_clears_pairing_and_backoff);

    return UNITY_END();
}
