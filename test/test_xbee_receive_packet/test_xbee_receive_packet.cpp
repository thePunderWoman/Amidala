// test_xbee_receive_packet.cpp
// Tests for include/xbee_receive_packet.h -- parsing the XBee Receive Packet
// (0x90) frame body that carries a Snips controller's custom uplink payload
// (issue #204), as distinct from the IO Data Sample frames (0x92/0x82,
// see xbee_io_sample.h) the existing XBee pocket remote uses.

#include "xbee_receive_packet.h"
#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// Builds a well-formed 0x90 frame body: type(1) addr64(8) addr16(2)
// options(1) payload(N).
static void buildFrame(uint8_t *buf, uint32_t addrLsb, const uint8_t *payload,
                        uint16_t payloadLen, uint16_t addr16 = 0) {
    buf[0] = 0x90;
    buf[1] = 0x00; buf[2] = 0x00; buf[3] = 0x00; buf[4] = 0x00;  // addr64 high 32 (unused)
    buf[5] = (uint8_t)(addrLsb >> 24);
    buf[6] = (uint8_t)(addrLsb >> 16);
    buf[7] = (uint8_t)(addrLsb >> 8);
    buf[8] = (uint8_t)(addrLsb);
    buf[9] = (uint8_t)(addr16 >> 8); buf[10] = (uint8_t)(addr16);
    buf[11] = 0x01;                 // options
    memcpy(buf + 12, payload, payloadLen);
}

void test_valid_frame_extracts_addr_and_payload() {
    uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    uint8_t buf[12 + sizeof(payload)];
    buildFrame(buf, 0x12345678, payload, sizeof(payload), 0xBEEF);

    XBeeReceivePacket out;
    TEST_ASSERT_TRUE(xbeeParseReceivePacket(buf, sizeof(buf), &out));
    TEST_ASSERT_EQUAL_HEX32(0x12345678, out.addrLsb);
    TEST_ASSERT_EQUAL_HEX16(0xBEEF, out.addr16);
    TEST_ASSERT_EQUAL(sizeof(payload), out.payloadLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out.payload, sizeof(payload));
}

void test_zero_length_payload_accepted() {
    uint8_t buf[12];
    buildFrame(buf, 0xDEADBEEF, nullptr, 0);

    XBeeReceivePacket out;
    TEST_ASSERT_TRUE(xbeeParseReceivePacket(buf, 12, &out));
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, out.addrLsb);
    TEST_ASSERT_EQUAL(0, out.payloadLength);
}

void test_wrong_frame_type_rejected() {
    uint8_t payload[] = {0x01};
    uint8_t buf[12 + sizeof(payload)];
    buildFrame(buf, 0x11111111, payload, sizeof(payload));
    buf[0] = 0x92;  // IO Data Sample, not Receive Packet

    XBeeReceivePacket out;
    TEST_ASSERT_FALSE(xbeeParseReceivePacket(buf, sizeof(buf), &out));
}

void test_too_short_frame_rejected() {
    uint8_t buf[11] = {0x90};  // one byte short of the 12-byte fixed header
    XBeeReceivePacket out;
    TEST_ASSERT_FALSE(xbeeParseReceivePacket(buf, sizeof(buf), &out));
}

void test_zero_length_buffer_rejected_without_reading_it() {
    // length must be checked before buf[0] is ever touched -- regression
    // test for a fix where the frame-type check ran first.
    uint8_t buf[1];
    XBeeReceivePacket out;
    TEST_ASSERT_FALSE(xbeeParseReceivePacket(buf, 0, &out));
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_valid_frame_extracts_addr_and_payload);
    RUN_TEST(test_zero_length_payload_accepted);
    RUN_TEST(test_wrong_frame_type_rejected);
    RUN_TEST(test_too_short_frame_rejected);
    RUN_TEST(test_zero_length_buffer_rejected_without_reading_it);

    return UNITY_END();
}
