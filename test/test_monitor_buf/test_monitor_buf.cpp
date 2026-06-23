// test_monitor_buf.cpp
// Regression tests for the serial monitor circular buffer (monitor_buf.h).
//
// Bug: sMonHead and sMonCount were uint8_t.  With MON_LINES=256, appending
// the 256th entry incremented sMonCount from 255 to 0 (uint8_t overflow),
// so the poll loop ran 0 iterations and the monitor appeared empty forever.
//
// Fix: changed sMonHead and sMonCount to uint16_t.  These tests reproduce
// the overflow scenario and verify correct behaviour at and past the boundary.

#include "arduino_mock.h"
#define MONITOR_BUF_OWNER
#include "monitor_buf.h"
#include <unity.h>

static void resetBuf() {
    memset(sMonBuf, 0, sizeof(sMonBuf));
    sMonHead  = 0;
    sMonCount = 0;
    sMonSeq   = 0;
}

void setUp(void)    { resetBuf(); }
void tearDown(void) {}

// ---- Basic append -----------------------------------------------------------

void test_single_append_stores_text_and_class() {
    monAppend("hello", 't');
    TEST_ASSERT_EQUAL_UINT16(1, sMonCount);
    TEST_ASSERT_EQUAL_STRING("hello", sMonBuf[0].text);
    TEST_ASSERT_EQUAL_CHAR('t', sMonBuf[0].cls);
}

void test_append_increments_seq() {
    uint32_t before = sMonSeq;
    monAppend("x");
    TEST_ASSERT_EQUAL_UINT32(before + 1, sMonSeq);
}

void test_append_default_class_is_info() {
    monAppend("msg");
    TEST_ASSERT_EQUAL_CHAR('i', sMonBuf[0].cls);
}

void test_text_truncated_to_line_len() {
    char big[MON_LINE_LEN + 32];
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    monAppend(big);
    TEST_ASSERT_EQUAL_INT(MON_LINE_LEN - 1, (int)strlen(sMonBuf[0].text));
}

// ---- Fill to capacity -------------------------------------------------------

void test_count_stops_at_MON_LINES() {
    for (int i = 0; i < MON_LINES + 10; i++) monAppend("x");
    TEST_ASSERT_EQUAL_UINT16(MON_LINES, sMonCount);
}

void test_count_does_not_overflow_at_256() {
    // This is the exact regression: filling 256 entries must leave sMonCount
    // at 256 (MON_LINES), NOT wrap to 0 via uint8_t overflow.
    for (int i = 0; i < MON_LINES; i++) monAppend("x");
    TEST_ASSERT_EQUAL_UINT16(MON_LINES, sMonCount);
    // One more entry must not corrupt the count.
    monAppend("y");
    TEST_ASSERT_EQUAL_UINT16(MON_LINES, sMonCount);
}

void test_seq_continues_past_256_entries() {
    for (int i = 0; i < MON_LINES + 5; i++) monAppend("x");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(MON_LINES + 5), sMonSeq);
}

// ---- Ring wrap — oldest entries overwritten ---------------------------------

void test_head_wraps_after_MON_LINES_entries() {
    for (int i = 0; i < MON_LINES; i++) monAppend("x");
    TEST_ASSERT_EQUAL_UINT16(0, sMonHead);
}

void test_oldest_entry_overwritten_when_full() {
    // Fill the buffer then write a distinctive entry.
    for (int i = 0; i < MON_LINES; i++) {
        char buf[8]; snprintf(buf, sizeof(buf), "e%d", i);
        monAppend(buf);
    }
    // Now the buffer is full; head is back at 0.
    // Writing one more overwrites slot 0 with "NEW".
    monAppend("NEW");
    // Slot 0 should now be "NEW" (head has advanced to 1).
    TEST_ASSERT_EQUAL_STRING("NEW", sMonBuf[0].text);
    TEST_ASSERT_EQUAL_UINT16(1, sMonHead);
}

void test_read_order_after_wrap() {
    // Fill past capacity so the ring wraps.
    // After MON_LINES+1 appends: sMonHead=1, oldest entry is at slot 1.
    for (int i = 0; i < MON_LINES + 1; i++) {
        char buf[16]; snprintf(buf, sizeof(buf), "line%d", i);
        monAppend(buf);
    }
    // The oldest visible entry (index 0 in readout order) is at sMonHead.
    // sMonHead = 1; slot 1 should hold "line1".
    uint16_t start = sMonHead;
    TEST_ASSERT_EQUAL_STRING("line1", sMonBuf[start].text);
    // The newest entry is at slot 0: "line256" (i=256).
    TEST_ASSERT_EQUAL_STRING("line256", sMonBuf[0].text);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_single_append_stores_text_and_class);
    RUN_TEST(test_append_increments_seq);
    RUN_TEST(test_append_default_class_is_info);
    RUN_TEST(test_text_truncated_to_line_len);
    RUN_TEST(test_count_stops_at_MON_LINES);
    RUN_TEST(test_count_does_not_overflow_at_256);
    RUN_TEST(test_seq_continues_past_256_entries);
    RUN_TEST(test_head_wraps_after_MON_LINES_entries);
    RUN_TEST(test_oldest_entry_overwritten_when_full);
    RUN_TEST(test_read_order_after_wrap);

    return UNITY_END();
}
