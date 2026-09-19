// test_xbee_at_command.cpp
// Tests for include/xbee_at_command.h -- building the Local AT Command
// Request (0x08) frame and parsing the Local AT Command Response (0x88)
// (issue #213: reading/writing the XBee's own PAN ID / coordinator role).

#include "xbee_at_command.h"
#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// ---- buildRequest -----------------------------------------------------------

void test_request_frame_type_and_frame_id() {
    uint8_t buf[32];
    uint16_t n = XBeeATCommand::buildRequest(buf, sizeof(buf), 0x42, "ID", nullptr, 0);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_HEX8(0x08, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, buf[1]);
}

void test_request_command_bytes_are_ascii() {
    uint8_t buf[32];
    XBeeATCommand::buildRequest(buf, sizeof(buf), 1, "CE", nullptr, 0);
    TEST_ASSERT_EQUAL_HEX8('C', buf[2]);
    TEST_ASSERT_EQUAL_HEX8('E', buf[3]);
}

void test_request_query_has_no_parameter_bytes() {
    uint8_t buf[32];
    uint16_t n = XBeeATCommand::buildRequest(buf, sizeof(buf), 1, "ID", nullptr, 0);
    TEST_ASSERT_EQUAL(XBeeATCommand::kRequestHeaderLength, n);
}

void test_request_set_includes_parameter_bytes() {
    uint8_t param[] = {0x41, 0x33};  // "4133" as a 2-byte value
    uint8_t buf[32];
    uint16_t n = XBeeATCommand::buildRequest(buf, sizeof(buf), 1, "ID", param, sizeof(param));
    TEST_ASSERT_EQUAL(XBeeATCommand::kRequestHeaderLength + sizeof(param), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(param, buf + XBeeATCommand::kRequestHeaderLength, sizeof(param));
}

void test_request_frame_id_zero_is_not_special_cased_by_builder() {
    // The header comment warns callers never to pass 0 (it means "no
    // response wanted" by Digi convention) -- but the builder itself is a
    // pure byte-layout function with no opinion on that; verify it doesn't
    // silently do anything surprising with 0 either way.
    uint8_t buf[32];
    uint16_t n = XBeeATCommand::buildRequest(buf, sizeof(buf), 0, "ID", nullptr, 0);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_HEX8(0, buf[1]);
}

void test_request_rejects_undersized_output_buffer() {
    uint8_t param[] = {0x01, 0x02, 0x03};
    uint8_t buf[XBeeATCommand::kRequestHeaderLength + 2];  // 1 byte short
    uint16_t n = XBeeATCommand::buildRequest(buf, sizeof(buf), 1, "ID", param, sizeof(param));
    TEST_ASSERT_EQUAL(0, n);
}

// ---- parseResponse ------------------------------------------------------------

static void buildResponseFrame(uint8_t *buf, uint8_t frameId, const char cmd[2],
                                uint8_t status, const uint8_t *value, uint8_t valueLen) {
    buf[0] = 0x88;
    buf[1] = frameId;
    buf[2] = (uint8_t)cmd[0];
    buf[3] = (uint8_t)cmd[1];
    buf[4] = status;
    if (valueLen) memcpy(buf + 5, value, valueLen);
}

void test_response_parses_frame_id_and_command() {
    uint8_t buf[16];
    buildResponseFrame(buf, 0x42, "ID", XBeeATCommand::kOk, nullptr, 0);

    XBeeATCommand::Response out;
    TEST_ASSERT_TRUE(XBeeATCommand::parseResponse(buf, 5, &out));
    TEST_ASSERT_EQUAL_HEX8(0x42, out.frameId);
    TEST_ASSERT_EQUAL('I', out.command[0]);
    TEST_ASSERT_EQUAL('D', out.command[1]);
    TEST_ASSERT_EQUAL_HEX8(XBeeATCommand::kOk, out.status);
    TEST_ASSERT_EQUAL(0, out.valueLength);
}

void test_response_extracts_query_value() {
    uint8_t value[] = {0x00, 0x00, 0x41, 0x33};
    uint8_t buf[16];
    buildResponseFrame(buf, 1, "ID", XBeeATCommand::kOk, value, sizeof(value));

    XBeeATCommand::Response out;
    TEST_ASSERT_TRUE(XBeeATCommand::parseResponse(buf, 5 + sizeof(value), &out));
    TEST_ASSERT_EQUAL(sizeof(value), out.valueLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(value, out.value, sizeof(value));
}

void test_response_reports_error_status() {
    uint8_t buf[16];
    buildResponseFrame(buf, 1, "CE", XBeeATCommand::kInvalidParameter, nullptr, 0);

    XBeeATCommand::Response out;
    TEST_ASSERT_TRUE(XBeeATCommand::parseResponse(buf, 5, &out));
    TEST_ASSERT_EQUAL_HEX8(XBeeATCommand::kInvalidParameter, out.status);
}

void test_response_wrong_frame_type_rejected() {
    uint8_t buf[16];
    buildResponseFrame(buf, 1, "ID", XBeeATCommand::kOk, nullptr, 0);
    buf[0] = 0x8B;  // Transmit Status, not AT Command Response

    XBeeATCommand::Response out;
    TEST_ASSERT_FALSE(XBeeATCommand::parseResponse(buf, 5, &out));
}

void test_response_too_short_frame_rejected() {
    uint8_t buf[4] = {0x88, 0x01, 'I', 'D'};  // missing the status byte
    XBeeATCommand::Response out;
    TEST_ASSERT_FALSE(XBeeATCommand::parseResponse(buf, 4, &out));
}

void test_response_zero_length_buffer_rejected_without_reading_it() {
    // Length must be checked before buf[0] is ever touched -- same
    // regression class as xbee_receive_packet.h's own test for this.
    uint8_t buf[1];
    XBeeATCommand::Response out;
    TEST_ASSERT_FALSE(XBeeATCommand::parseResponse(buf, 0, &out));
}

// ---- valueToHexString / hexStringToValue -------------------------------------
// Regression coverage for a real bug caught in code review: naively
// zero-padding every byte to 2 hex digits made a single-byte value like
// CE=1 (0x01) render as "01", which then failed a strict `=== '1'` check
// in the web UI and made the Connectivity page always report "not a
// coordinator" even when the module genuinely was one.

void test_value_to_hex_single_small_byte_is_not_zero_padded() {
    uint8_t bytes[] = {0x01};
    char out[17];
    uint8_t n = XBeeATCommand::valueToHexString(bytes, sizeof(bytes), out, sizeof(out));
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL_STRING("1", out);
}

void test_value_to_hex_multi_byte_matches_real_panid() {
    uint8_t bytes[] = {0x41, 0x33};
    char out[17];
    XBeeATCommand::valueToHexString(bytes, sizeof(bytes), out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("4133", out);
}

void test_value_to_hex_small_leading_byte_with_more_bytes_after() {
    // Regression: a small leading byte followed by more bytes must not gain
    // a spurious extra zero -- 0x0102 must read "102", not "0102".
    uint8_t bytes[] = {0x01, 0x02};
    char out[17];
    XBeeATCommand::valueToHexString(bytes, sizeof(bytes), out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("102", out);
}

void test_value_to_hex_all_zero_reads_as_single_zero() {
    uint8_t bytes[8] = {0};
    char out[17];
    uint8_t n = XBeeATCommand::valueToHexString(bytes, sizeof(bytes), out, sizeof(out));
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL_STRING("0", out);
}

void test_hex_to_value_short_string_is_minimal_length() {
    uint8_t out[8];
    uint8_t n = XBeeATCommand::hexStringToValue("4133", out, sizeof(out));
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_HEX8(0x41, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x33, out[1]);
}

void test_hex_to_value_single_digit() {
    uint8_t out[8];
    uint8_t n = XBeeATCommand::hexStringToValue("1", out, sizeof(out));
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL_HEX8(0x01, out[0]);
}

// Regression: an outCap that exactly fits the digits but leaves no room for
// the NUL terminator must fail cleanly, not write one byte past the end
// (caught by ASan against an earlier version of this function).
void test_value_to_hex_rejects_buffer_with_no_room_for_terminator() {
    uint8_t bytes[] = {0xAB};
    char out[2];  // fits "AB" exactly, but not the terminator after it
    uint8_t n = XBeeATCommand::valueToHexString(bytes, sizeof(bytes), out, sizeof(out));
    TEST_ASSERT_EQUAL(0, n);
}

void test_value_to_hex_exact_fit_including_terminator_succeeds() {
    uint8_t bytes[] = {0xAB};
    char out[3];  // "AB" + NUL, exactly
    uint8_t n = XBeeATCommand::valueToHexString(bytes, sizeof(bytes), out, sizeof(out));
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_STRING("AB", out);
}

void test_value_and_hex_round_trip() {
    uint8_t original[] = {0x41, 0x33};
    char hex[17];
    XBeeATCommand::valueToHexString(original, sizeof(original), hex, sizeof(hex));

    uint8_t roundtripped[8];
    uint8_t n = XBeeATCommand::hexStringToValue(hex, roundtripped, sizeof(roundtripped));
    TEST_ASSERT_EQUAL(sizeof(original), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original, roundtripped, sizeof(original));
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_request_frame_type_and_frame_id);
    RUN_TEST(test_request_command_bytes_are_ascii);
    RUN_TEST(test_request_query_has_no_parameter_bytes);
    RUN_TEST(test_request_set_includes_parameter_bytes);
    RUN_TEST(test_request_frame_id_zero_is_not_special_cased_by_builder);
    RUN_TEST(test_request_rejects_undersized_output_buffer);

    RUN_TEST(test_response_parses_frame_id_and_command);
    RUN_TEST(test_response_extracts_query_value);
    RUN_TEST(test_response_reports_error_status);
    RUN_TEST(test_response_wrong_frame_type_rejected);
    RUN_TEST(test_response_too_short_frame_rejected);
    RUN_TEST(test_response_zero_length_buffer_rejected_without_reading_it);

    RUN_TEST(test_value_to_hex_single_small_byte_is_not_zero_padded);
    RUN_TEST(test_value_to_hex_multi_byte_matches_real_panid);
    RUN_TEST(test_value_to_hex_small_leading_byte_with_more_bytes_after);
    RUN_TEST(test_value_to_hex_all_zero_reads_as_single_zero);
    RUN_TEST(test_value_to_hex_rejects_buffer_with_no_room_for_terminator);
    RUN_TEST(test_value_to_hex_exact_fit_including_terminator_succeeds);
    RUN_TEST(test_hex_to_value_short_string_is_minimal_length);
    RUN_TEST(test_hex_to_value_single_digit);
    RUN_TEST(test_value_and_hex_round_trip);

    return UNITY_END();
}
