// test_monitor_drain.cpp
// Regression tests for the serial-port drain state machine (monitor_drain.h).
//
// Bug: seedLabel() wrote a port's label prefix ("S0: ") into its line buffer
// on every AmidalaWiFiAP::handle() tick (every loop() iteration -- far more
// often than 100ms). monDrainSerial()'s silence-flush only checked pos > 0,
// which was already true right after a label seed even with zero real bytes
// read, and lastMs started at 0 so "millis() - lastMs" looked like a long
// silence immediately. Result: an idle port (no real UART traffic at all)
// flooded the monitor with bare "S0: " lines forever, on every tick.
//
// Fix: track the label length and only ever flush when real content has
// been appended beyond the seeded label (monDrainHasContent()).

#include "arduino_mock.h"
#define MONITOR_BUF_OWNER
#include "monitor_buf.h"
#include "monitor_drain.h"
#include <unity.h>

static void resetBuf() {
    memset(sMonBuf, 0, sizeof(MonLine) * MON_LINES);
    sMonHead  = 0;
    sMonCount = 0;
    sMonSeq   = 0;
}

void setUp(void)    { resetBuf(); }
void tearDown(void) {}

// ---- The bug: idle port must never flush just its own label -----------------

void test_idle_port_does_not_flush_on_repeated_seed_and_silence_check() {
    MonDrainState s;
    monDrainInit(s, "S0: ");

    // Simulate many handle() ticks on an idle port: seed, then check silence,
    // with no bytes ever read and time advancing well past the 100ms window.
    uint32_t now = 0;
    for (int tick = 0; tick < 50; tick++) {
        monDrainSeedLabel(s);
        now += 200; // always > 100ms of "silence" since lastMs starts at 0
        monDrainSilenceCheck(s, now);
    }

    TEST_ASSERT_EQUAL_UINT16(0, sMonCount);
    TEST_ASSERT_EQUAL_UINT32(0, sMonSeq);
}

void test_idle_port_at_boot_does_not_flush_immediately() {
    // lastMs starts at 0; millis() is already > 100 almost immediately after
    // boot, so a naive "pos > 0" check would fire on the very first tick.
    MonDrainState s;
    monDrainInit(s, "S1: ");
    monDrainSeedLabel(s);
    monDrainSilenceCheck(s, 5000); // 5s after boot, no data ever received
    TEST_ASSERT_EQUAL_UINT16(0, sMonCount);
}

// ---- Real data still flushes correctly ---------------------------------------

void test_real_line_flushes_on_newline_with_label_prefix() {
    MonDrainState s;
    monDrainInit(s, "S0: ");
    monDrainSeedLabel(s);
    const char* msg = "hello";
    for (const char* c = msg; *c; c++) monDrainByte(s, (uint8_t)*c, 1000);
    monDrainByte(s, '\n', 1001);

    TEST_ASSERT_EQUAL_UINT16(1, sMonCount);
    TEST_ASSERT_EQUAL_STRING("S0: hello", sMonBuf[0].text);
    TEST_ASSERT_EQUAL_CHAR('r', sMonBuf[0].cls);
}

void test_real_partial_line_flushes_after_silence_timeout() {
    MonDrainState s;
    monDrainInit(s, "S2: ");
    monDrainSeedLabel(s);
    const char* msg = "partial";
    for (const char* c = msg; *c; c++) monDrainByte(s, (uint8_t)*c, 1000);

    // Not yet 100ms of silence -- must not flush.
    monDrainSilenceCheck(s, 1050);
    TEST_ASSERT_EQUAL_UINT16(0, sMonCount);

    // Past 100ms of silence since the last real byte -- must flush now.
    monDrainSilenceCheck(s, 1101);
    TEST_ASSERT_EQUAL_UINT16(1, sMonCount);
    TEST_ASSERT_EQUAL_STRING("S2: partial", sMonBuf[0].text);
}

void test_label_reseeded_after_flush_for_next_line() {
    MonDrainState s;
    monDrainInit(s, "S0: ");
    monDrainSeedLabel(s);
    monDrainByte(s, 'a', 1000);
    monDrainByte(s, '\n', 1000);
    TEST_ASSERT_EQUAL_UINT16(1, sMonCount);

    // Next tick reseeds the (now-empty) buffer -- should NOT itself flush.
    monDrainSeedLabel(s);
    monDrainSilenceCheck(s, 1300); // > 100ms since last real byte at t=1000
    TEST_ASSERT_EQUAL_UINT16(1, sMonCount); // still just the one real line

    // But a genuine second line still flushes correctly.
    monDrainByte(s, 'b', 1300);
    monDrainByte(s, '\n', 1300);
    TEST_ASSERT_EQUAL_UINT16(2, sMonCount);
    TEST_ASSERT_EQUAL_STRING("S0: a", sMonBuf[0].text);
    TEST_ASSERT_EQUAL_STRING("S0: b", sMonBuf[1].text);
}

void test_line_buffer_full_flushes_even_without_newline() {
    MonDrainState s;
    monDrainInit(s, "S0: ");
    monDrainSeedLabel(s);
    for (int i = 0; i < MON_LINE_LEN + 10; i++) {
        monDrainByte(s, 'x', 1000);
    }
    TEST_ASSERT_TRUE(sMonCount >= 1);
}

// ---- Configurable flush class (console log tee) ------------------------------
//
// monDrainFlush() used to hardcode class 'r' (rx) on every flush. That's
// correct for the S0/S1/S2 serial-port taps, but wrong for a source that
// isn't a tx/rx wire exchange at all -- e.g. AmidalaConsole's own console
// output, teed into the monitor as "LOG: ...". Tagging it 'r' would gate it
// behind web/monitor.html's RX toggle (default off) even with its own LOG
// filter button on, so it'd default to invisible. monDrainInit() now takes
// an optional class, defaulting to 'r' so existing S0/S1/S2 callers are
// unaffected.

void test_default_class_is_rx_for_backward_compatibility() {
    MonDrainState s;
    monDrainInit(s, "S0: ");
    monDrainSeedLabel(s);
    monDrainByte(s, 'x', 1000);
    monDrainByte(s, '\n', 1000);
    TEST_ASSERT_EQUAL_CHAR('r', sMonBuf[0].cls);
}

void test_custom_class_used_for_non_rx_source() {
    MonDrainState s;
    monDrainInit(s, "LOG: ", 'i');
    monDrainSeedLabel(s);
    const char* msg = "boot ok";
    for (const char* c = msg; *c; c++) monDrainByte(s, (uint8_t)*c, 1000);
    monDrainByte(s, '\n', 1000);
    TEST_ASSERT_EQUAL_CHAR('i', sMonBuf[0].cls);
    TEST_ASSERT_EQUAL_STRING("LOG: boot ok", sMonBuf[0].text);
}

// ---- main ---------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_idle_port_does_not_flush_on_repeated_seed_and_silence_check);
    RUN_TEST(test_idle_port_at_boot_does_not_flush_immediately);
    RUN_TEST(test_real_line_flushes_on_newline_with_label_prefix);
    RUN_TEST(test_real_partial_line_flushes_after_silence_timeout);
    RUN_TEST(test_label_reseeded_after_flush_for_next_line);
    RUN_TEST(test_line_buffer_full_flushes_even_without_newline);
    RUN_TEST(test_default_class_is_rx_for_backward_compatibility);
    RUN_TEST(test_custom_class_used_for_non_rx_source);

    return UNITY_END();
}
