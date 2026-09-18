// test_snips_packet.cpp
// Round-trip encode/decode tests for include/snips_packet.h -- Amidala's
// wire format for Snips Controllers uplink/downlink packets (issue #204).
// Byte-position assertions are explicit so a future correction against real
// hardware (see snips_packet.h's header comment) is a one-file diff.

#include "snips_packet.h"
// env:native's test_build_src=no means only files under this test/ directory
// get built -- pull the real implementation in directly, same pattern as
// test_dome_drive_roboclaw.cpp.
#include "../../src/snips_packet.cpp"
#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// ---- Uplink: byte layout ----------------------------------------------------

void test_uplink_encode_byte_positions() {
    Snips::UplinkPacket pkt;
    pkt.buttonMask = 0x1234;
    pkt.triggerPercent = 42;
    pkt.stickXPercent = -50;
    pkt.stickYPercent = 77;
    pkt.batteryPercent = 88;
    pkt.chargeState = Snips::ChargeState::kCharging;
    pkt.flags = Snips::UplinkPacket::kFlagShuttingDown;

    uint8_t buf[Snips::kUplinkEncodedSize];
    size_t n = Snips::encodeUplink(pkt, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(Snips::kUplinkEncodedSize, n);

    TEST_ASSERT_EQUAL_HEX8(0x12, buf[0]);  // buttonMask high byte
    TEST_ASSERT_EQUAL_HEX8(0x34, buf[1]);  // buttonMask low byte
    TEST_ASSERT_EQUAL_UINT8(42, buf[2]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)-50, buf[3]);
    TEST_ASSERT_EQUAL_UINT8(77, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(88, buf[5]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)Snips::ChargeState::kCharging, buf[6]);
    TEST_ASSERT_EQUAL_UINT8(Snips::UplinkPacket::kFlagShuttingDown, buf[7]);
}

void test_uplink_round_trip_all_button_mask_bits() {
    Snips::UplinkPacket pkt;
    pkt.buttonMask = 0xFFFF;

    uint8_t buf[Snips::kUplinkEncodedSize];
    TEST_ASSERT_EQUAL(Snips::kUplinkEncodedSize, Snips::encodeUplink(pkt, buf, sizeof(buf)));

    Snips::UplinkPacket out;
    TEST_ASSERT_TRUE(Snips::decodeUplink(buf, sizeof(buf), &out));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, out.buttonMask);
}

void test_uplink_round_trip_negative_stick_values() {
    Snips::UplinkPacket pkt;
    pkt.stickXPercent = -100;
    pkt.stickYPercent = -1;

    uint8_t buf[Snips::kUplinkEncodedSize];
    Snips::encodeUplink(pkt, buf, sizeof(buf));

    Snips::UplinkPacket out;
    TEST_ASSERT_TRUE(Snips::decodeUplink(buf, sizeof(buf), &out));
    TEST_ASSERT_EQUAL_INT8(-100, out.stickXPercent);
    TEST_ASSERT_EQUAL_INT8(-1, out.stickYPercent);
}

void test_uplink_round_trip_each_charge_state() {
    Snips::ChargeState states[] = {
        Snips::ChargeState::kDone,
        Snips::ChargeState::kCharging,
        Snips::ChargeState::kRecoverableFault,
        Snips::ChargeState::kLatchedFault,
    };
    for (auto s : states) {
        Snips::UplinkPacket pkt;
        pkt.chargeState = s;
        uint8_t buf[Snips::kUplinkEncodedSize];
        Snips::encodeUplink(pkt, buf, sizeof(buf));
        Snips::UplinkPacket out;
        TEST_ASSERT_TRUE(Snips::decodeUplink(buf, sizeof(buf), &out));
        TEST_ASSERT_TRUE(s == out.chargeState);
    }
}

void test_uplink_encode_rejects_short_buffer() {
    Snips::UplinkPacket pkt;
    uint8_t buf[Snips::kUplinkEncodedSize - 1];
    TEST_ASSERT_EQUAL(0, Snips::encodeUplink(pkt, buf, sizeof(buf)));
}

void test_uplink_decode_rejects_short_buffer() {
    uint8_t buf[Snips::kUplinkEncodedSize - 1] = {};
    Snips::UplinkPacket out;
    TEST_ASSERT_FALSE(Snips::decodeUplink(buf, sizeof(buf), &out));
}

void test_uplink_decode_accepts_oversized_buffer() {
    // A longer buffer than strictly needed (e.g. a bigger SPI read) must
    // still decode correctly from its first kUplinkEncodedSize bytes.
    Snips::UplinkPacket pkt;
    pkt.batteryPercent = 55;
    uint8_t buf[Snips::kUplinkEncodedSize + 10] = {};
    Snips::encodeUplink(pkt, buf, Snips::kUplinkEncodedSize);

    Snips::UplinkPacket out;
    TEST_ASSERT_TRUE(Snips::decodeUplink(buf, sizeof(buf), &out));
    TEST_ASSERT_EQUAL_UINT8(55, out.batteryPercent);
}

// ---- Downlink: byte layout and string field handling ------------------------

void test_downlink_encode_byte_positions() {
    Snips::DownlinkPacket pkt;
    pkt.handedness = Snips::DownlinkPacket::Handedness::kRight;
    strcpy(pkt.leftLabel, "Vol");
    strcpy(pkt.leftValue, "50");
    strcpy(pkt.rightLabel, "Thr");
    strcpy(pkt.rightValue, "100");

    uint8_t buf[Snips::kDownlinkEncodedSize];
    size_t n = Snips::encodeDownlink(pkt, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(Snips::kDownlinkEncodedSize, n);
    TEST_ASSERT_EQUAL(33, Snips::kDownlinkEncodedSize);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)Snips::DownlinkPacket::Handedness::kRight, buf[0]);
    TEST_ASSERT_EQUAL_UINT8('V', buf[1]);
    TEST_ASSERT_EQUAL_UINT8('o', buf[2]);
    TEST_ASSERT_EQUAL_UINT8('l', buf[3]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[4]);  // zero-padded remainder of leftLabel
}

void test_downlink_round_trip_strings() {
    Snips::DownlinkPacket pkt;
    pkt.handedness = Snips::DownlinkPacket::Handedness::kLeft;
    strcpy(pkt.leftLabel, "Vol");
    strcpy(pkt.leftValue, "75");
    strcpy(pkt.rightLabel, "Thr");
    strcpy(pkt.rightValue, "20");

    uint8_t buf[Snips::kDownlinkEncodedSize];
    Snips::encodeDownlink(pkt, buf, sizeof(buf));

    Snips::DownlinkPacket out;
    TEST_ASSERT_TRUE(Snips::decodeDownlink(buf, sizeof(buf), &out));
    TEST_ASSERT_TRUE(Snips::DownlinkPacket::Handedness::kLeft == out.handedness);
    TEST_ASSERT_EQUAL_STRING("Vol", out.leftLabel);
    TEST_ASSERT_EQUAL_STRING("75", out.leftValue);
    TEST_ASSERT_EQUAL_STRING("Thr", out.rightLabel);
    TEST_ASSERT_EQUAL_STRING("20", out.rightValue);
}

void test_downlink_field_exactly_kFieldLength_has_no_null_padding_but_still_terminates() {
    // An 8-char string exactly fills the field with no room for a padding
    // zero -- decode must still null-terminate at kFieldLength.
    Snips::DownlinkPacket pkt;
    strcpy(pkt.leftLabel, "12345678");  // exactly kFieldLength (8) chars

    uint8_t buf[Snips::kDownlinkEncodedSize];
    Snips::encodeDownlink(pkt, buf, sizeof(buf));

    Snips::DownlinkPacket out;
    TEST_ASSERT_TRUE(Snips::decodeDownlink(buf, sizeof(buf), &out));
    TEST_ASSERT_EQUAL_STRING("12345678", out.leftLabel);
}

void test_downlink_field_without_null_terminator_does_not_overrun() {
    // DownlinkPacket's fields are char[kFieldLength+1] -- exercise the
    // defensive bound in encodeDownlink's field writer directly by filling
    // all kFieldLength+1 bytes with non-null data (no terminator at all)
    // and confirming only the first kFieldLength bytes are read.
    Snips::DownlinkPacket pkt;
    memset(pkt.rightLabel, 'X', sizeof(pkt.rightLabel));  // no null terminator anywhere

    uint8_t buf[Snips::kDownlinkEncodedSize];
    Snips::encodeDownlink(pkt, buf, sizeof(buf));

    Snips::DownlinkPacket out;
    TEST_ASSERT_TRUE(Snips::decodeDownlink(buf, sizeof(buf), &out));
    TEST_ASSERT_EQUAL_STRING("XXXXXXXX", out.rightLabel);  // exactly kFieldLength Xs
}

void test_downlink_empty_strings_round_trip() {
    Snips::DownlinkPacket pkt;  // all fields default-initialized empty
    uint8_t buf[Snips::kDownlinkEncodedSize];
    Snips::encodeDownlink(pkt, buf, sizeof(buf));

    Snips::DownlinkPacket out;
    TEST_ASSERT_TRUE(Snips::decodeDownlink(buf, sizeof(buf), &out));
    TEST_ASSERT_EQUAL_STRING("", out.leftLabel);
    TEST_ASSERT_EQUAL_STRING("", out.leftValue);
    TEST_ASSERT_EQUAL_STRING("", out.rightLabel);
    TEST_ASSERT_EQUAL_STRING("", out.rightValue);
}

void test_downlink_encode_rejects_short_buffer() {
    Snips::DownlinkPacket pkt;
    uint8_t buf[Snips::kDownlinkEncodedSize - 1];
    TEST_ASSERT_EQUAL(0, Snips::encodeDownlink(pkt, buf, sizeof(buf)));
}

void test_downlink_decode_rejects_short_buffer() {
    uint8_t buf[Snips::kDownlinkEncodedSize - 1] = {};
    Snips::DownlinkPacket out;
    TEST_ASSERT_FALSE(Snips::decodeDownlink(buf, sizeof(buf), &out));
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_uplink_encode_byte_positions);
    RUN_TEST(test_uplink_round_trip_all_button_mask_bits);
    RUN_TEST(test_uplink_round_trip_negative_stick_values);
    RUN_TEST(test_uplink_round_trip_each_charge_state);
    RUN_TEST(test_uplink_encode_rejects_short_buffer);
    RUN_TEST(test_uplink_decode_rejects_short_buffer);
    RUN_TEST(test_uplink_decode_accepts_oversized_buffer);

    RUN_TEST(test_downlink_encode_byte_positions);
    RUN_TEST(test_downlink_round_trip_strings);
    RUN_TEST(test_downlink_field_exactly_kFieldLength_has_no_null_padding_but_still_terminates);
    RUN_TEST(test_downlink_field_without_null_terminator_does_not_overrun);
    RUN_TEST(test_downlink_empty_strings_round_trip);
    RUN_TEST(test_downlink_encode_rejects_short_buffer);
    RUN_TEST(test_downlink_decode_rejects_short_buffer);

    return UNITY_END();
}
