// test_xbee_frame_checksum.cpp
// Unit tests for xbeeChecksumValid() (include/xbee_frame_checksum.h).
//
// Regression coverage for issue #185: src/xbee_spi.cpp's frame reader read
// and discarded the trailing XBee API checksum byte without ever verifying
// it, so a mis-framed read (e.g. from the module actually being in AP=2
// escaped mode, which this parser doesn't handle) could be accepted and
// parsed as if it were a real IO sample frame instead of being detected and
// dropped.

#include "xbee_frame_checksum.h"
#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

// Per the XBee API spec: checksum = 0xFF - (sum of all frame bytes, mod 256).
void test_valid_checksum_accepted() {
    const uint8_t frame[] = {0x01, 0x02, 0x03};
    uint8_t checksum = (uint8_t)(0xFF - (0x01 + 0x02 + 0x03));
    TEST_ASSERT_TRUE(xbeeChecksumValid(frame, sizeof(frame), checksum));
}

void test_wrong_checksum_rejected() {
    const uint8_t frame[] = {0x01, 0x02, 0x03};
    uint8_t correct = (uint8_t)(0xFF - (0x01 + 0x02 + 0x03));
    TEST_ASSERT_FALSE(xbeeChecksumValid(frame, sizeof(frame), (uint8_t)(correct + 1)));
}

// A single flipped data byte -- the kind of corruption a mis-framed read
// (see issue #185's AP=2 hypothesis) would introduce -- must be caught.
void test_single_byte_corruption_rejected() {
    const uint8_t good[] = {0x92, 0x01, 0x02, 0x03};
    uint8_t checksum = (uint8_t)(0xFF - (0x92 + 0x01 + 0x02 + 0x03));
    TEST_ASSERT_TRUE(xbeeChecksumValid(good, sizeof(good), checksum));

    const uint8_t corrupted[] = {0x92, 0x01, 0x99, 0x03};  // one byte flipped
    TEST_ASSERT_FALSE(xbeeChecksumValid(corrupted, sizeof(corrupted), checksum));
}

void test_empty_frame_checksum() {
    TEST_ASSERT_TRUE(xbeeChecksumValid(nullptr, 0, 0xFF));
    TEST_ASSERT_FALSE(xbeeChecksumValid(nullptr, 0, 0x00));
}

// Checksum arithmetic must wrap at 256, not overflow into a wider type.
void test_checksum_wraps_at_256() {
    const uint8_t frame[] = {0xFF, 0xFF, 0xFF};  // sum = 0x2FD, low byte 0xFD
    uint8_t checksum = (uint8_t)(0xFF - 0xFD);
    TEST_ASSERT_TRUE(xbeeChecksumValid(frame, sizeof(frame), checksum));
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_valid_checksum_accepted);
    RUN_TEST(test_wrong_checksum_rejected);
    RUN_TEST(test_single_byte_corruption_rejected);
    RUN_TEST(test_empty_frame_checksum);
    RUN_TEST(test_checksum_wraps_at_256);

    return UNITY_END();
}
