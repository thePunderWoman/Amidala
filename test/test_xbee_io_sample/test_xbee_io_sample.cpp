// test_xbee_io_sample.cpp
// Unit tests for xbeeParseIOSample() (include/xbee_io_sample.h).
//
// Regression/feature coverage for issue #187: the receive parser only
// recognized ZigBee's 0x92 IO Data Sample frame type, which would silently
// drop every packet from a remote flashed to the 802.15.4 function set
// (frame type 0x82) during the planned migration off ZigBee.

#include "xbee_io_sample.h"
#include <unity.h>
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

// ---- ZigBee (0x92) --------------------------------------------------------
// Layout: type(1) addr64(8) addr16(2) options(1) numSamples(1)
//         digitalMask(2) analogMask(1) [digital(2)] [analog(2 each)]

void test_zigbee_full_frame_parses() {
    uint8_t buf[] = {
        0x92,
        0x00, 0x13, 0xA2, 0x00, 0x41, 0x92, 0x34, 0x56,  // addr64 (low32 = 0x41923456)
        0xFF, 0xFE,                                       // addr16 (unused)
        0x01,                                              // options
        0x01,                                              // numSamples
        0x0C, 0x70,                                        // digitalMask (bits 4,5,6,10,11)
        0x0F,                                               // analogMask (bits 0-3)
        0x0C, 0x70,                                         // digital samples (all high = unpressed)
        0x02, 0x00,                                         // analog0 = 512
        0x02, 0x00,                                         // analog1 = 512
        0x00, 0x64,                                         // analog2 = 100
        0x00, 0xC8,                                         // analog3 = 200
    };
    XBeeIOSample sample;
    TEST_ASSERT_TRUE(xbeeParseIOSample(buf, sizeof(buf), &sample));
    TEST_ASSERT_EQUAL_HEX32(0x41923456, sample.addrLsb);
    TEST_ASSERT_EQUAL_HEX16(0x0C70, sample.digitalMask);
    TEST_ASSERT_EQUAL_HEX8(0x0F, sample.analogMask);
    TEST_ASSERT_EQUAL_HEX16(0x0C70, sample.digitalSamples);
    TEST_ASSERT_EQUAL(512, sample.analog[0]);
    TEST_ASSERT_EQUAL(512, sample.analog[1]);
    TEST_ASSERT_EQUAL(100, sample.analog[2]);
    TEST_ASSERT_EQUAL(200, sample.analog[3]);
}

void test_zigbee_frame_too_short_rejected() {
    // 15 bytes -- one short of the 16-byte minimum (type..analogMask).
    uint8_t buf[] = {
        0x92, 0,0,0,0,0,0,0,0, 0xFF,0xFE, 0x01, 0x01, 0x0C, 0x70,
    };
    XBeeIOSample sample;
    TEST_ASSERT_FALSE(xbeeParseIOSample(buf, sizeof(buf), &sample));
}

// ---- 802.15.4 (0x82) -------------------------------------------------------
// Layout: type(1) addr64(8) rssi(1) options(1) numSamples(1)
//         digitalMask(2) analogMask(1) [digital(2)] [analog(2 each)]
// Same address offset as ZigBee -- params.xbr/xbl matching is unaffected.

void test_802154_full_frame_parses() {
    uint8_t buf[] = {
        0x82,
        0x00, 0x13, 0xA2, 0x00, 0x41, 0x92, 0x34, 0x56,  // addr64 (low32 = 0x41923456)
        0x28,                                              // RSSI
        0x01,                                              // options
        0x01,                                              // numSamples
        0x0C, 0x70,                                        // digitalMask (bits 4,5,6,10,11)
        0x0F,                                               // analogMask (bits 0-3)
        0x08, 0x70,                                         // digital samples (bit4=l3 pressed)
        0x02, 0x00,                                         // analog0 = 512
        0x02, 0x00,                                         // analog1 = 512
        0x00, 0x64,                                         // analog2 = 100
        0x00, 0xC8,                                         // analog3 = 200
    };
    XBeeIOSample sample;
    TEST_ASSERT_TRUE(xbeeParseIOSample(buf, sizeof(buf), &sample));
    TEST_ASSERT_EQUAL_HEX32(0x41923456, sample.addrLsb);
    TEST_ASSERT_EQUAL_HEX16(0x0C70, sample.digitalMask);
    TEST_ASSERT_EQUAL_HEX8(0x0F, sample.analogMask);
    TEST_ASSERT_EQUAL_HEX16(0x0870, sample.digitalSamples);
    TEST_ASSERT_EQUAL(512, sample.analog[0]);
    TEST_ASSERT_EQUAL(512, sample.analog[1]);
    TEST_ASSERT_EQUAL(100, sample.analog[2]);
    TEST_ASSERT_EQUAL(200, sample.analog[3]);
}

void test_802154_frame_too_short_rejected() {
    // 14 bytes -- one short of the 15-byte minimum (type..analogMask).
    uint8_t buf[] = {
        0x82, 0,0,0,0,0,0,0,0, 0x28, 0x01, 0x01, 0x0C, 0x70,
    };
    XBeeIOSample sample;
    TEST_ASSERT_FALSE(xbeeParseIOSample(buf, sizeof(buf), &sample));
}

// ---- Frame-type dispatch ---------------------------------------------------

void test_unrecognized_frame_type_rejected() {
    uint8_t buf[64] = {0x90};  // ZigBee Receive Packet -- not an IO sample
    XBeeIOSample sample;
    TEST_ASSERT_FALSE(xbeeParseIOSample(buf, sizeof(buf), &sample));
}

void test_802154_short_address_frame_type_not_handled() {
    // 0x83 (16-bit short address) is intentionally out of scope -- see
    // xbee_io_sample.h's header comment.
    uint8_t buf[64] = {0x83};
    XBeeIOSample sample;
    TEST_ASSERT_FALSE(xbeeParseIOSample(buf, sizeof(buf), &sample));
}

// ---- Truncation edge cases (shared logic, exercised via 0x92) -------------

void test_truncated_before_digital_samples_rejected() {
    // digitalMask is nonzero but the frame ends exactly at the mask --
    // there's no room for the 2 promised digital sample bytes.
    uint8_t buf[] = {
        0x92, 0,0,0,0,0,0,0,0, 0xFF,0xFE, 0x01, 0x01, 0x0C, 0x70, 0x0F,
    };
    XBeeIOSample sample;
    TEST_ASSERT_FALSE(xbeeParseIOSample(buf, sizeof(buf), &sample));
}

// A frame that runs out of room partway through the analog samples still
// parses -- channels read before the cutoff are populated, the rest keep
// their {512,512,0,0} defaults. This matches the pre-#187 behavior of the
// inline parser (analog truncation was never a hard rejection).
void test_truncated_mid_analog_keeps_partial_results() {
    uint8_t buf[] = {
        0x92, 0,0,0,0,0,0,0,0, 0xFF,0xFE, 0x01, 0x01,
        0x00, 0x00,        // digitalMask = 0 (no digital samples present)
        0x0F,               // analogMask (bits 0-3) -- but only analog0 fits
        0x00, 0x2A,          // analog0 = 42
        // frame ends here -- analog1-3 never arrive
    };
    XBeeIOSample sample;
    TEST_ASSERT_TRUE(xbeeParseIOSample(buf, sizeof(buf), &sample));
    TEST_ASSERT_EQUAL(0, sample.digitalSamples);
    TEST_ASSERT_EQUAL(42, sample.analog[0]);
    TEST_ASSERT_EQUAL(512, sample.analog[1]);  // default, never overwritten
    TEST_ASSERT_EQUAL(0, sample.analog[2]);
    TEST_ASSERT_EQUAL(0, sample.analog[3]);
}

void test_zero_masks_yield_defaults() {
    uint8_t buf[] = {
        0x92, 0,0,0,0,0,0,0,0, 0xFF,0xFE, 0x01, 0x01, 0x00, 0x00, 0x00,
    };
    XBeeIOSample sample;
    TEST_ASSERT_TRUE(xbeeParseIOSample(buf, sizeof(buf), &sample));
    TEST_ASSERT_EQUAL(0, sample.digitalMask);
    TEST_ASSERT_EQUAL(0, sample.analogMask);
    TEST_ASSERT_EQUAL(0, sample.digitalSamples);
    TEST_ASSERT_EQUAL(512, sample.analog[0]);
    TEST_ASSERT_EQUAL(512, sample.analog[1]);
    TEST_ASSERT_EQUAL(0, sample.analog[2]);
    TEST_ASSERT_EQUAL(0, sample.analog[3]);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_zigbee_full_frame_parses);
    RUN_TEST(test_zigbee_frame_too_short_rejected);
    RUN_TEST(test_802154_full_frame_parses);
    RUN_TEST(test_802154_frame_too_short_rejected);
    RUN_TEST(test_unrecognized_frame_type_rejected);
    RUN_TEST(test_802154_short_address_frame_type_not_handled);
    RUN_TEST(test_truncated_before_digital_samples_rejected);
    RUN_TEST(test_truncated_mid_analog_keeps_partial_results);
    RUN_TEST(test_zero_masks_yield_defaults);

    return UNITY_END();
}
