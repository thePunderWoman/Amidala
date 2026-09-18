// test_xbee_transmit_frame.cpp
// Tests for include/xbee_transmit_frame.h -- building the Transmit Request
// (0x10) frame Amidala sends to reply to a Snips controller (issue #204
// phase 2).

#include "xbee_transmit_frame.h"
#include "xbee_frame_checksum.h"
#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_frame_type_and_frame_id() {
    uint8_t payload[] = {0x01, 0x02};
    uint8_t buf[32];
    uint16_t n = XBeeTransmitFrame::buildTransmitRequest(buf, sizeof(buf), payload, sizeof(payload), 0x1234);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_HEX8(0x10, buf[0]);  // frame type
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);  // frameId = 0 (no TX status requested)
}

void test_dest64_is_all_ones_unknown_address() {
    uint8_t payload[] = {0xAA};
    uint8_t buf[32];
    XBeeTransmitFrame::buildTransmitRequest(buf, sizeof(buf), payload, sizeof(payload), 0x0001);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf[2 + i]);
    }
}

void test_dest16_encoded_big_endian() {
    uint8_t payload[] = {0xAA};
    uint8_t buf[32];
    XBeeTransmitFrame::buildTransmitRequest(buf, sizeof(buf), payload, sizeof(payload), 0xBEEF);
    TEST_ASSERT_EQUAL_HEX8(0xBE, buf[10]);
    TEST_ASSERT_EQUAL_HEX8(0xEF, buf[11]);
}

void test_broadcast_radius_and_options_zero() {
    uint8_t payload[] = {0xAA};
    uint8_t buf[32];
    XBeeTransmitFrame::buildTransmitRequest(buf, sizeof(buf), payload, sizeof(payload), 0x0001);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[12]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[13]);
}

void test_payload_copied_after_header() {
    uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t buf[32];
    uint16_t n = XBeeTransmitFrame::buildTransmitRequest(buf, sizeof(buf), payload, sizeof(payload), 0x0001);
    TEST_ASSERT_EQUAL(XBeeTransmitFrame::kHeaderLength + sizeof(payload), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, buf + XBeeTransmitFrame::kHeaderLength, sizeof(payload));
}

void test_zero_length_payload_ok() {
    uint8_t buf[32];
    uint16_t n = XBeeTransmitFrame::buildTransmitRequest(buf, sizeof(buf), nullptr, 0, 0x0001);
    TEST_ASSERT_EQUAL(XBeeTransmitFrame::kHeaderLength, n);
}

void test_rejects_undersized_output_buffer() {
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uint8_t buf[XBeeTransmitFrame::kHeaderLength + 2];  // 1 byte short
    uint16_t n = XBeeTransmitFrame::buildTransmitRequest(buf, sizeof(buf), payload, sizeof(payload), 0x0001);
    TEST_ASSERT_EQUAL(0, n);
}

void test_checksum_computed_over_built_frame_validates() {
    // End-to-end sanity: xbeeComputeChecksum() over this frame's bytes
    // should validate cleanly against xbeeChecksumValid(), the same as a
    // real received frame would.
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t buf[32];
    uint16_t n = XBeeTransmitFrame::buildTransmitRequest(buf, sizeof(buf), payload, sizeof(payload), 0x4242);
    uint8_t checksum = xbeeComputeChecksum(buf, n);
    TEST_ASSERT_TRUE(xbeeChecksumValid(buf, n, checksum));
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_frame_type_and_frame_id);
    RUN_TEST(test_dest64_is_all_ones_unknown_address);
    RUN_TEST(test_dest16_encoded_big_endian);
    RUN_TEST(test_broadcast_radius_and_options_zero);
    RUN_TEST(test_payload_copied_after_header);
    RUN_TEST(test_zero_length_payload_ok);
    RUN_TEST(test_rejects_undersized_output_buffer);
    RUN_TEST(test_checksum_computed_over_built_frame_validates);

    return UNITY_END();
}
