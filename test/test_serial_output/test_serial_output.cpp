// test_serial_output.cpp
// Tests for writeEolTo() and sendSerialStringTo() in serial_output.h.
// These free functions contain all the EOL logic that AmidalaController
// delegates to — tested here in isolation via MockStream.

#include "arduino_mock.h"
#include "serial_output.h"
#include <unity.h>
#include <string.h>

void setUp()    {}
void tearDown() {}

// ---------------------------------------------------------------------------
// writeEolTo
// ---------------------------------------------------------------------------

void test_writeEol_lf_writes_single_lf() {
    MockStream s;
    writeEolTo(s, 10);
    TEST_ASSERT_EQUAL(1, (int)s.outLen);
    TEST_ASSERT_EQUAL('\n', s.outBuf[0]);
}

void test_writeEol_cr_writes_single_cr() {
    MockStream s;
    writeEolTo(s, 13);
    TEST_ASSERT_EQUAL(1, (int)s.outLen);
    TEST_ASSERT_EQUAL('\r', s.outBuf[0]);
}

void test_writeEol_crlf_sentinel_writes_cr_then_lf() {
    MockStream s;
    writeEolTo(s, 0); // 0 = CRLF sentinel
    TEST_ASSERT_EQUAL(2, (int)s.outLen);
    TEST_ASSERT_EQUAL('\r', s.outBuf[0]);
    TEST_ASSERT_EQUAL('\n', s.outBuf[1]);
}

void test_writeEol_arbitrary_byte_writes_that_byte() {
    MockStream s;
    writeEolTo(s, '|');
    TEST_ASSERT_EQUAL(1, (int)s.outLen);
    TEST_ASSERT_EQUAL('|', s.outBuf[0]);
}

// ---------------------------------------------------------------------------
// sendSerialStringTo
// ---------------------------------------------------------------------------

void test_sendSerial_appends_eol_after_string() {
    MockStream s;
    sendSerialStringTo(s, "ABC", ':', 10);
    TEST_ASSERT_EQUAL_STRING("ABC\n", s.outBuf);
}

void test_sendSerial_replaces_delimiter_with_eol() {
    MockStream s;
    // Colon delimiter, LF EOL: "A:B" → "A\nB\n"
    sendSerialStringTo(s, "A:B", ':', 10);
    TEST_ASSERT_EQUAL_STRING("A\nB\n", s.outBuf);
}

void test_sendSerial_delimiter_with_crlf_eol() {
    MockStream s;
    // Colon delimiter, CRLF (0) EOL: "A:B" → "A\r\nB\r\n"
    sendSerialStringTo(s, "A:B", ':', 0);
    TEST_ASSERT_EQUAL(6, (int)s.outLen);
    TEST_ASSERT_EQUAL('A',  s.outBuf[0]);
    TEST_ASSERT_EQUAL('\r', s.outBuf[1]);
    TEST_ASSERT_EQUAL('\n', s.outBuf[2]);
    TEST_ASSERT_EQUAL('B',  s.outBuf[3]);
    TEST_ASSERT_EQUAL('\r', s.outBuf[4]);
    TEST_ASSERT_EQUAL('\n', s.outBuf[5]);
}

void test_sendSerial_empty_string_writes_only_eol() {
    MockStream s;
    sendSerialStringTo(s, "", ':', 13);
    TEST_ASSERT_EQUAL(1, (int)s.outLen);
    TEST_ASSERT_EQUAL('\r', s.outBuf[0]);
}

void test_sendSerial_multiple_delimiters_each_become_eol() {
    MockStream s;
    // "A:B:C" with LF → "A\nB\nC\n"
    sendSerialStringTo(s, "A:B:C", ':', 10);
    TEST_ASSERT_EQUAL_STRING("A\nB\nC\n", s.outBuf);
}

void test_sendSerial_no_delimiter_in_string_just_appends_eol() {
    MockStream s;
    sendSerialStringTo(s, "HELLO", ':', 10);
    TEST_ASSERT_EQUAL_STRING("HELLO\n", s.outBuf);
}

void test_sendSerial_crlf_eol_at_end_of_plain_string() {
    MockStream s;
    // "HI" with CRLF → "HI\r\n"
    sendSerialStringTo(s, "HI", ':', 0);
    TEST_ASSERT_EQUAL(4, (int)s.outLen);
    TEST_ASSERT_EQUAL('H',  s.outBuf[0]);
    TEST_ASSERT_EQUAL('I',  s.outBuf[1]);
    TEST_ASSERT_EQUAL('\r', s.outBuf[2]);
    TEST_ASSERT_EQUAL('\n', s.outBuf[3]);
}

// ---------------------------------------------------------------------------
// splitOnDelimiter
// Regression coverage for the WCB mesh path: a compound sstr/gadget command
// like "DM:ALARM|:PL4:PP100:PR30:PW20:PH" must split into separate pieces
// the same way sendSerialStringTo() splits it into separate UART0 lines --
// WCB_Client::broadcast() has no delimiter handling of its own, so shipping
// the whole compound string as one unsplit blob desyncs both halves'
// intended receivers (wrong HCR clip name, truncated periscope sequence).
// ---------------------------------------------------------------------------

void test_split_dm_alarm_compound_command() {
    char segs[4][200];
    uint8_t n = splitOnDelimiter("DM:ALARM|:PL4:PP100:PR30:PW20:PH", '|',
                                  &segs[0][0], 200, 4);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_STRING("DM:ALARM", segs[0]);
    TEST_ASSERT_EQUAL_STRING(":PL4:PP100:PR30:PW20:PH", segs[1]);
}

void test_split_no_delimiter_returns_one_segment() {
    char segs[4][200];
    uint8_t n = splitOnDelimiter("HELLO", '|', &segs[0][0], 200, 4);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL_STRING("HELLO", segs[0]);
}

void test_split_three_segments() {
    char segs[4][200];
    uint8_t n = splitOnDelimiter("A:B:C", ':', &segs[0][0], 200, 4);
    TEST_ASSERT_EQUAL(3, n);
    TEST_ASSERT_EQUAL_STRING("A", segs[0]);
    TEST_ASSERT_EQUAL_STRING("B", segs[1]);
    TEST_ASSERT_EQUAL_STRING("C", segs[2]);
}

void test_split_empty_string_returns_zero_segments() {
    char segs[4][200];
    uint8_t n = splitOnDelimiter("", '|', &segs[0][0], 200, 4);
    TEST_ASSERT_EQUAL(0, n);
}

void test_split_all_delimiters_returns_zero_segments() {
    // Consecutive/leading/trailing delimiters produce only empty segments,
    // which are skipped entirely (unlike sendSerialStringTo(), which would
    // write blank lines -- broadcasting an empty command has no equivalent
    // and would just waste a mesh packet).
    char segs[4][200];
    uint8_t n = splitOnDelimiter("|||", '|', &segs[0][0], 200, 4);
    TEST_ASSERT_EQUAL(0, n);
}

void test_split_leading_and_trailing_delimiters_are_skipped() {
    char segs[4][200];
    uint8_t n = splitOnDelimiter("|A|", '|', &segs[0][0], 200, 4);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL_STRING("A", segs[0]);
}

void test_split_respects_maxSegments_cap() {
    char segs[2][200];
    // 3 delimited pieces requested but only 2 slots available.
    uint8_t n = splitOnDelimiter("A:B:C", ':', &segs[0][0], 200, 2);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_STRING("A", segs[0]);
    TEST_ASSERT_EQUAL_STRING("B", segs[1]);
}

void test_split_truncates_segment_longer_than_segLen() {
    char segs[2][8];
    uint8_t n = splitOnDelimiter("ABCDEFGHIJ", '|', &segs[0][0], 8, 2);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL_STRING("ABCDEFG", segs[0]); // 7 chars + null terminator
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_writeEol_lf_writes_single_lf);
    RUN_TEST(test_writeEol_cr_writes_single_cr);
    RUN_TEST(test_writeEol_crlf_sentinel_writes_cr_then_lf);
    RUN_TEST(test_writeEol_arbitrary_byte_writes_that_byte);

    RUN_TEST(test_sendSerial_appends_eol_after_string);
    RUN_TEST(test_sendSerial_replaces_delimiter_with_eol);
    RUN_TEST(test_sendSerial_delimiter_with_crlf_eol);
    RUN_TEST(test_sendSerial_empty_string_writes_only_eol);
    RUN_TEST(test_sendSerial_multiple_delimiters_each_become_eol);
    RUN_TEST(test_sendSerial_no_delimiter_in_string_just_appends_eol);
    RUN_TEST(test_sendSerial_crlf_eol_at_end_of_plain_string);

    RUN_TEST(test_split_dm_alarm_compound_command);
    RUN_TEST(test_split_no_delimiter_returns_one_segment);
    RUN_TEST(test_split_three_segments);
    RUN_TEST(test_split_empty_string_returns_zero_segments);
    RUN_TEST(test_split_all_delimiters_returns_zero_segments);
    RUN_TEST(test_split_leading_and_trailing_delimiters_are_skipped);
    RUN_TEST(test_split_respects_maxSegments_cap);
    RUN_TEST(test_split_truncates_segment_longer_than_segLen);

    return UNITY_END();
}
