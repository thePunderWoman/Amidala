// test_snips_drive.cpp
// Tests for include/snips_drive.h -- how a Snips controller's uplink is fed
// into the drive/dome remote objects (XBeePocketRemote) that TankDrive and
// DomeDrive read.

#include "arduino_mock.h"
#include "snips_drive.h"
#include <unity.h>

void setUp(void) { mock_millis_value = 0; }
void tearDown(void) {}

// Counts the lifecycle callbacks update() makes.
class CountingRemote : public XBeePocketRemote {
public:
    int connects = 0, disconnects = 0, notifies = 0;
    void onConnect() override { connects++; }
    void onDisconnect() override { disconnects++; }
    void notify() override { notifies++; }
};

static Snips::UplinkPacket stick(int8_t x, int8_t y) {
    Snips::UplinkPacket pkt;
    pkt.stickXPercent = x;
    pkt.stickYPercent = y;
    return pkt;
}

// ---- raw value inverse ------------------------------------------------------

void test_raw_for_stick_round_trips_every_value() {
    XBeePocketRemote r;
    for (int v = -128; v <= 127; v++) {
        r.x = SnipsDrive::rawForStick(v);
        r.update();
        TEST_ASSERT_EQUAL_INT(v, r.state.analog.stick.lx);
    }
}

void test_raw_for_stick_clamps() {
    TEST_ASSERT_EQUAL_UINT16(SnipsDrive::rawForStick(127), SnipsDrive::rawForStick(500));
    TEST_ASSERT_EQUAL_UINT16(SnipsDrive::rawForStick(-128), SnipsDrive::rawForStick(-500));
}

// ---- stick direction --------------------------------------------------------

void test_centered_stick_reads_zero() {
    XBeePocketRemote r;
    SnipsDrive::applyUplink(stick(0, 0), false, 0, &r);
    r.update();
    TEST_ASSERT_EQUAL_INT(0, r.state.analog.stick.lx);
    TEST_ASSERT_EQUAL_INT(0, r.state.analog.stick.ly);
}

void test_forward_is_negative_y_and_right_is_positive_x() {
    // The controller reports +Y up and +X physically left; the drive code
    // expects the PS3/HID convention (forward -Y, right +X).
    XBeePocketRemote r;
    SnipsDrive::applyUplink(stick(-100, 100), false, 0, &r);
    r.update();
    TEST_ASSERT_EQUAL_INT(127, r.state.analog.stick.lx);
    TEST_ASSERT_EQUAL_INT(-127, r.state.analog.stick.ly);

    SnipsDrive::applyUplink(stick(50, -50), false, 0, &r);
    r.update();
    TEST_ASSERT_EQUAL_INT(-63, r.state.analog.stick.lx);
    TEST_ASSERT_EQUAL_INT(63, r.state.analog.stick.ly);
}

// ---- wheels and buttons -----------------------------------------------------

void test_wheels_read_zero_so_volume_and_boost_are_untouched() {
    // XBeePocketRemote's default wheel value (0) maps to 255, which the dome
    // remote would apply as 100% volume and the tank drive as full boost.
    XBeePocketRemote r;
    SnipsDrive::applyUplink(stick(0, 0), true, 0, &r);
    r.update();
    TEST_ASSERT_EQUAL_UINT8(0, r.state.analog.button.l1);
    TEST_ASSERT_EQUAL_UINT8(0, r.state.analog.button.l2);
}

void test_only_dome_side_passes_stick_click_as_l3() {
    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kStickClick) | (1u << SnipsButtons::kMacro1);

    XBeePocketRemote dome;
    SnipsDrive::applyUplink(pkt, true, 0, &dome);
    TEST_ASSERT_TRUE(dome.button[4]);
    for (int i = 0; i < 4; i++) TEST_ASSERT_FALSE(dome.button[i]);

    XBeePocketRemote drive;
    SnipsDrive::applyUplink(pkt, false, 0, &drive);
    for (int i = 0; i < 5; i++) TEST_ASSERT_FALSE(drive.button[i]);
}

void test_apply_marks_live_and_stamps_time() {
    XBeePocketRemote r;
    TEST_ASSERT_TRUE(r.failsafe());
    SnipsDrive::applyUplink(stick(0, 0), false, 4321, &r);
    TEST_ASSERT_TRUE(XBeePocketRemote::kXBee == r.type);
    TEST_ASSERT_EQUAL_UINT32(4321, r.lastPacket);
}

// ---- tick lifecycle ---------------------------------------------------------

void test_tick_does_nothing_before_first_uplink() {
    CountingRemote r;
    SnipsDrive::tick(&r, 500, 1000);
    TEST_ASSERT_EQUAL(0, r.connects);
    TEST_ASSERT_EQUAL(0, r.notifies);
}

void test_tick_connects_then_notifies_while_fresh() {
    CountingRemote r;
    SnipsDrive::applyUplink(stick(0, 0), false, 1000, &r);
    SnipsDrive::tick(&r, 500, 1000);
    TEST_ASSERT_EQUAL(1, r.connects);
    TEST_ASSERT_EQUAL(1, r.notifies);
    SnipsDrive::tick(&r, 500, 1400);
    TEST_ASSERT_EQUAL(2, r.notifies);
    TEST_ASSERT_EQUAL(0, r.disconnects);
}

void test_tick_disconnects_once_after_failsafe_timeout() {
    CountingRemote r;
    SnipsDrive::applyUplink(stick(0, 0), false, 1000, &r);
    SnipsDrive::tick(&r, 500, 1000);
    SnipsDrive::tick(&r, 500, 1501);
    TEST_ASSERT_EQUAL(1, r.disconnects);
    TEST_ASSERT_TRUE(r.failsafe());
    SnipsDrive::tick(&r, 500, 2000);
    TEST_ASSERT_EQUAL(1, r.disconnects);
}

void test_shut_down_disconnects_on_next_tick() {
    CountingRemote r;
    SnipsDrive::applyUplink(stick(0, 0), false, 1000, &r);
    SnipsDrive::tick(&r, 500, 1000);
    SnipsDrive::markShutDown(&r);
    SnipsDrive::tick(&r, 500, 1010);  // well inside the failsafe timeout
    TEST_ASSERT_EQUAL(1, r.disconnects);
}

void test_tick_handles_millis_wrap() {
    CountingRemote r;
    SnipsDrive::applyUplink(stick(0, 0), false, 0xFFFFFF00u, &r);
    SnipsDrive::tick(&r, 500, 0xFFFFFF00u);
    SnipsDrive::tick(&r, 500, 0x00000010u);  // 272 ms later, across the wrap
    TEST_ASSERT_EQUAL(0, r.disconnects);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_raw_for_stick_round_trips_every_value);
    RUN_TEST(test_raw_for_stick_clamps);
    RUN_TEST(test_centered_stick_reads_zero);
    RUN_TEST(test_forward_is_negative_y_and_right_is_positive_x);
    RUN_TEST(test_wheels_read_zero_so_volume_and_boost_are_untouched);
    RUN_TEST(test_only_dome_side_passes_stick_click_as_l3);
    RUN_TEST(test_apply_marks_live_and_stamps_time);
    RUN_TEST(test_tick_does_nothing_before_first_uplink);
    RUN_TEST(test_tick_connects_then_notifies_while_fresh);
    RUN_TEST(test_tick_disconnects_once_after_failsafe_timeout);
    RUN_TEST(test_shut_down_disconnects_on_next_tick);
    RUN_TEST(test_tick_handles_millis_wrap);
    return UNITY_END();
}
