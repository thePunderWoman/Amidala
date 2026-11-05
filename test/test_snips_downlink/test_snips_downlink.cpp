// test_snips_downlink.cpp
// Tests for include/snips_downlink.h -- the pure downlink builder,
// volume-wheel resolution and sequence tracker behind
// SnipsRemote::buildDownlinkPacket().

#include "arduino_mock.h"
#include "snips_downlink.h"
#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// ---- build() ----------------------------------------------------------------

void test_build_defaults_are_unknown_and_left() {
    SnipsDownlink::DroidStatus status;
    Snips::DownlinkPacket pkt = SnipsDownlink::build(status);
    TEST_ASSERT_TRUE(Snips::DownlinkPacket::Handedness::kLeft == pkt.handedness);
    TEST_ASSERT_EQUAL_UINT8(Snips::DownlinkPacket::kUnknown, pkt.volumePercent);
    TEST_ASSERT_EQUAL_UINT8(Snips::DownlinkPacket::kUnknown, pkt.throttlePercent);
    TEST_ASSERT_EQUAL_UINT8(Snips::DownlinkPacket::kUnknown, pkt.droidBatteryPercent);
    TEST_ASSERT_EQUAL_UINT8(Snips::DownlinkPacket::kUnknown, pkt.sequenceProgressPercent);
    TEST_ASSERT_EQUAL_UINT8(0, pkt.flags);
    TEST_ASSERT_EQUAL_STRING("", pkt.sequenceName);
}

void test_build_copies_values_and_clamps_percentages() {
    SnipsDownlink::DroidStatus status;
    status.rightHand = true;
    status.volumePercent = 0;
    status.throttlePercent = 250;
    status.droidBatteryPercent = 74;
    status.sequenceProgressPercent = 40;
    Snips::DownlinkPacket pkt = SnipsDownlink::build(status);
    TEST_ASSERT_TRUE(Snips::DownlinkPacket::Handedness::kRight == pkt.handedness);
    TEST_ASSERT_EQUAL_UINT8(0, pkt.volumePercent);
    TEST_ASSERT_EQUAL_UINT8(100, pkt.throttlePercent);
    TEST_ASSERT_EQUAL_UINT8(74, pkt.droidBatteryPercent);
    TEST_ASSERT_EQUAL_UINT8(40, pkt.sequenceProgressPercent);
}

void test_build_sets_each_flag() {
    SnipsDownlink::DroidStatus status;
    status.estop = true;
    TEST_ASSERT_EQUAL_UINT8(Snips::DownlinkPacket::kFlagEstop, SnipsDownlink::build(status).flags);
    status = SnipsDownlink::DroidStatus();
    status.motorsDisabled = true;
    TEST_ASSERT_EQUAL_UINT8(Snips::DownlinkPacket::kFlagMotorsDisabled,
                            SnipsDownlink::build(status).flags);
    status = SnipsDownlink::DroidStatus();
    status.muse = true;
    TEST_ASSERT_EQUAL_UINT8(Snips::DownlinkPacket::kFlagMuse, SnipsDownlink::build(status).flags);
    status = SnipsDownlink::DroidStatus();
    status.autoDome = true;
    TEST_ASSERT_EQUAL_UINT8(Snips::DownlinkPacket::kFlagAutoDome, SnipsDownlink::build(status).flags);
}

void test_build_truncates_long_sequence_name() {
    SnipsDownlink::DroidStatus status;
    status.sequenceId = 7;
    status.sequenceName = "A Very Long Sequence Name";
    Snips::DownlinkPacket pkt = SnipsDownlink::build(status);
    TEST_ASSERT_EQUAL_UINT8(7, pkt.sequenceId);
    TEST_ASSERT_EQUAL_STRING("A Very Long Seq", pkt.sequenceName);
}

void test_build_treats_null_sequence_name_as_empty() {
    SnipsDownlink::DroidStatus status;
    status.sequenceName = nullptr;
    TEST_ASSERT_EQUAL_STRING("", SnipsDownlink::build(status).sequenceName);
}

// ---- volumeWheelFor() ---------------------------------------------------------

void test_volume_wheel_defaults_to_main_wheel() {
    ButtonAction none = {};
    TEST_ASSERT_EQUAL_UINT8(2, SnipsDownlink::volumeWheelFor(none, none, 2, 3));
}

void test_volume_wheel_follows_plain_and_alt_buttons() {
    ButtonAction none = {};
    ButtonAction plain = {};
    plain.action = ButtonAction::kVolumeStep;
    plain.volstep.target = 0;
    ButtonAction alt = {};
    alt.action = ButtonAction::kVolumeStep;
    alt.volstep.target = 1;

    TEST_ASSERT_EQUAL_UINT8(2, SnipsDownlink::volumeWheelFor(plain, none, 2, 3));
    TEST_ASSERT_EQUAL_UINT8(3, SnipsDownlink::volumeWheelFor(none, alt, 2, 3));
    TEST_ASSERT_EQUAL_UINT8(2, SnipsDownlink::volumeWheelFor(plain, alt, 2, 3));  // LeftUp first
}

void test_volume_wheel_alt_falls_through_when_alt_wheel_is_zero() {
    // Same fallthrough as AmidalaAudio::setAltVolumeNoResponse(): the screen
    // must read the channel the alt button actually writes.
    ButtonAction none = {};
    ButtonAction alt = {};
    alt.action = ButtonAction::kVolumeStep;
    alt.volstep.target = 1;
    TEST_ASSERT_EQUAL_UINT8(2, SnipsDownlink::volumeWheelFor(alt, none, 2, 0));
}

// ---- SequenceTracker --------------------------------------------------------

void test_sequence_tracker_starts_empty() {
    SnipsDownlink::SequenceTracker tracker;
    TEST_ASSERT_EQUAL_UINT8(0, tracker.id());
    TEST_ASSERT_EQUAL_STRING("", tracker.nameAt(0));
}

void test_sequence_tracker_sends_name_for_show_window() {
    SnipsDownlink::SequenceTracker tracker;
    tracker.start("Happy", 1000);
    TEST_ASSERT_EQUAL_UINT8(1, tracker.id());
    TEST_ASSERT_EQUAL_STRING("Happy", tracker.nameAt(1000));
    TEST_ASSERT_EQUAL_STRING("Happy",
                             tracker.nameAt(1000 + SnipsDownlink::SequenceTracker::kShowMs - 1));
    TEST_ASSERT_EQUAL_STRING("", tracker.nameAt(1000 + SnipsDownlink::SequenceTracker::kShowMs));
}

void test_sequence_tracker_restart_bumps_id_and_window() {
    SnipsDownlink::SequenceTracker tracker;
    tracker.start("Happy", 0);
    tracker.start("Happy", 8000);
    TEST_ASSERT_EQUAL_UINT8(2, tracker.id());
    TEST_ASSERT_EQUAL_STRING("Happy", tracker.nameAt(12000));
}

void test_sequence_tracker_id_wraps() {
    SnipsDownlink::SequenceTracker tracker;
    for (int i = 0; i < 256; i++) tracker.start("x", 0);
    TEST_ASSERT_EQUAL_UINT8(0, tracker.id());
}

void test_sequence_tracker_window_survives_millis_wrap() {
    SnipsDownlink::SequenceTracker tracker;
    tracker.start("Happy", 0xFFFFFF00u);
    TEST_ASSERT_EQUAL_STRING("Happy", tracker.nameAt(0x00000100u));
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_build_defaults_are_unknown_and_left);
    RUN_TEST(test_build_copies_values_and_clamps_percentages);
    RUN_TEST(test_build_sets_each_flag);
    RUN_TEST(test_build_truncates_long_sequence_name);
    RUN_TEST(test_build_treats_null_sequence_name_as_empty);
    RUN_TEST(test_volume_wheel_defaults_to_main_wheel);
    RUN_TEST(test_volume_wheel_follows_plain_and_alt_buttons);
    RUN_TEST(test_volume_wheel_alt_falls_through_when_alt_wheel_is_zero);
    RUN_TEST(test_sequence_tracker_starts_empty);
    RUN_TEST(test_sequence_tracker_sends_name_for_show_window);
    RUN_TEST(test_sequence_tracker_restart_bumps_id_and_window);
    RUN_TEST(test_sequence_tracker_id_wraps);
    RUN_TEST(test_sequence_tracker_window_survives_millis_wrap);
    return UNITY_END();
}
