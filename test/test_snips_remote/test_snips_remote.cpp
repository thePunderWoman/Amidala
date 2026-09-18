// test_snips_remote.cpp
// Tests for include/snips_remote.h / src/snips_remote.cpp -- SnipsRemote's
// button dispatch, alt/mute handling, and connect/failsafe lifecycle
// (issue #204).
//
// SnipsRemote's real methods need a fully-constructed AmidalaController
// (fDriver->params, isAltHeld/setAltHeld/noteButtonUp/etc), which pulls in
// the whole firmware -- same situation test_xbee_remote.cpp is in with
// DriveController/DomeController (whose real notify()/process() bodies are
// stubbed for linking, not exercised, in that file). SnipsRemote isn't
// templated on its driver type, so its real onUplinkReceived()/
// checkFailsafe() (src/snips_remote.cpp) can't be compiled against a fake
// here either -- TestSnipsRemote below is a hand-written mirror of those
// two bodies against a FakeDriver, same "must be kept in sync by hand"
// approach as test_double_press.cpp's FakeDblPress mirrors
// AmidalaController's double-press logic. The long-press timer and alt/
// dispatch decision inside it are NOT re-mirrored, though -- those call the
// real button_dispatch.h functions (no BLE/controller dependency), so only
// the per-side numbering/alt-range/mute-range glue that's genuinely local
// to SnipsRemote needs to be kept in sync by hand.

#include "arduino_mock.h"
#include "snips_packet.h"
#include "snips_buttons.h"
#include "button_dispatch.h"
#include <unity.h>
#include <string.h>

void setUp(void) { mock_millis_value = 0; }
void tearDown(void) {}

// ---- Fake AmidalaController -------------------------------------------------
// Mirrors just the surface SnipsRemote::onUplinkReceived()/checkFailsafe()
// use: params.altbtn/mutebutton and the button-dispatch/alt/mute methods.

struct FakeParams {
    int altbtn = 0;
    int mutebutton = 0;
};

struct FakeDriver {
    FakeParams params;
    bool altHeld = false;

    int buttonUpCount[17]   = {0};
    int altButtonCount[17]  = {0};
    int longButtonCount[17] = {0};
    int muteBtnUpCount = 0;
    int resetMuteTimerCount = 0;

    bool isAltHeld() const { return altHeld; }
    void setAltHeld(bool held) { altHeld = held; }
    void noteButtonUp(unsigned num)      { buttonUpCount[num]++; }
    void processAltButton(unsigned num)  { altButtonCount[num]++; }
    void processLongButton(unsigned num) { longButtonCount[num]++; }
    void noteMuteBtnUp()          { muteBtnUpCount++; }
    void resetMutePressTimer()    { resetMuteTimerCount++; }
};

// A trimmed, test-local mirror of SnipsRemote, parameterized on FakeDriver
// instead of a forward-declared AmidalaController -- the real class can't
// be templated (its .cpp needs the real controller.h), so this exercises
// the exact same logic (verbatim copy of src/snips_remote.cpp's bodies) the
// same way test_double_press.cpp's FakeDblPress mirrors AmidalaController's
// double-press logic: a deliberate, documented "must stay in sync" mirror
// for the one piece that can't be reached directly natively.
class TestSnipsRemote {
public:
    enum Side { kRight, kLeft };
    TestSnipsRemote(FakeDriver *driver, Side side) : fDriver(driver), fSide(side) {}

    uint32_t addr = 0;
    bool isConnected() const { return fConnected; }
    bool failsafe() const { return !fConnected; }
    uint8_t triggerPercent = 0;
    int8_t stickXPercent = 0;
    int8_t stickYPercent = 0;
    uint8_t batteryPercent = 0;
    Snips::ChargeState chargeState = Snips::ChargeState::kDone;

    void onUplinkReceived(const Snips::UplinkPacket &pkt) {
        uint32_t now = millis();
        if (fDriver) {
            unsigned rangeMin = (fSide == kRight) ? 1 : 9;
            unsigned rangeMax = rangeMin + SnipsButtons::kCount - 1;

            int altbtn = fDriver->params.altbtn;
            bool altbtnInRange = (altbtn >= (int)rangeMin && altbtn <= (int)rangeMax);
            if (altbtnInRange) {
                unsigned altBit = (unsigned)altbtn - rangeMin;
                fDriver->setAltHeld((pkt.buttonMask & (1u << altBit)) != 0);
            }
            bool altHeld = fDriver->isAltHeld();

            int muteBtn = fDriver->params.mutebutton;
            bool muteBtnInRange = (muteBtn >= (int)rangeMin && muteBtn <= (int)rangeMax);
            unsigned muteBit = muteBtnInRange ? (unsigned)muteBtn - rangeMin : SnipsButtons::kCount;

            for (unsigned i = 0; i < SnipsButtons::kCount; i++) {
                bool prevPressed = (fPrevButtonMask & (1u << i)) != 0;
                bool pressed = (pkt.buttonMask & (1u << i)) != 0;
                bool down = !prevPressed && pressed;
                bool up = prevPressed && !pressed;
                auto r = fLongPress[i].update(down, up, pressed, now, DEFAULT_LONG_PRESS_MS);
                unsigned num = buttonNumber(i);
                dispatchButtonPress(*fDriver, num, r.up, r.longUp, altbtn, altHeld);
                if (muteBtnInRange && i == muteBit && r.up) {
                    fDriver->noteMuteBtnUp();
                }
            }
        }
        fPrevButtonMask = pkt.buttonMask;
        triggerPercent = pkt.triggerPercent;
        stickXPercent = pkt.stickXPercent;
        stickYPercent = pkt.stickYPercent;
        batteryPercent = pkt.batteryPercent;
        chargeState = pkt.chargeState;
        fConnected = true;
        fLastPacket = now;
    }

    void checkFailsafe(uint32_t fst) {
        if (!fConnected) return;
        if (fLastPacket + fst >= millis()) return;
        fConnected = false;
        if (!fDriver) return;
        unsigned rangeMin = (fSide == kRight) ? 1 : 9;
        unsigned rangeMax = rangeMin + SnipsButtons::kCount - 1;
        int altbtn = fDriver->params.altbtn;
        if (altbtn >= (int)rangeMin && altbtn <= (int)rangeMax)
            fDriver->setAltHeld(false);
        int muteBtn = fDriver->params.mutebutton;
        if (muteBtn >= (int)rangeMin && muteBtn <= (int)rangeMax)
            fDriver->resetMutePressTimer();
    }

private:
    unsigned buttonNumber(unsigned bitIndex) const {
        return (fSide == kRight ? 1 : 9) + bitIndex;
    }
    FakeDriver *fDriver;
    Side fSide;
    bool fConnected = false;
    uint32_t fLastPacket = 0;
    uint16_t fPrevButtonMask = 0;
    ButtonLongPress fLongPress[SnipsButtons::kCount];
};

// ---- Right-side (buttons 1-8) numbering and dispatch -------------------------

void test_right_macro1_press_release_fires_button1() {
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kMacro1);
    r.onUplinkReceived(pkt);  // down: no dispatch yet
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[1]);

    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);  // up: button 1 fires
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[1]);
}

void test_right_stickclick_fires_button8() {
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kStickClick);
    r.onUplinkReceived(pkt);
    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[8]);
}

// ---- Left-side (buttons 9-16) numbering --------------------------------------

void test_left_macro1_fires_button9() {
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kLeft);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kMacro1);
    r.onUplinkReceived(pkt);
    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[9]);
}

void test_left_stickclick_fires_button16() {
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kLeft);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kStickClick);
    r.onUplinkReceived(pkt);
    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[16]);
}

// ---- Alt modifier -------------------------------------------------------------

void test_alt_button_in_range_suppresses_own_press_and_sets_held() {
    FakeDriver driver;
    driver.params.altbtn = 7;  // right controller's Bumper
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kBumper);
    r.onUplinkReceived(pkt);
    TEST_ASSERT_TRUE(driver.isAltHeld());

    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[7]);
    TEST_ASSERT_EQUAL(0, driver.altButtonCount[7]);
}

void test_alt_button_out_of_range_for_this_side_is_ignored() {
    // altbtn=7 belongs to the RIGHT controller -- the LEFT instance must not
    // treat any of its own bits as the alt modifier.
    FakeDriver driver;
    driver.params.altbtn = 7;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kLeft);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kBumper);  // left's bit 7 -> button 15
    r.onUplinkReceived(pkt);
    TEST_ASSERT_FALSE(driver.isAltHeld());
}

void test_press_while_alt_held_routes_to_alt_layer() {
    FakeDriver driver;
    driver.params.altbtn = 7;  // Bumper
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kBumper) | (1u << SnipsButtons::kMacro2);
    r.onUplinkReceived(pkt);  // hold alt, press macro2

    pkt.buttonMask = (1u << SnipsButtons::kBumper);  // release macro2, alt still held
    r.onUplinkReceived(pkt);

    TEST_ASSERT_EQUAL(1, driver.altButtonCount[2]);
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[2]);
}

// ---- Long press ---------------------------------------------------------------

void test_long_press_fires_after_threshold_and_suppresses_up() {
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    mock_millis_value = 1;
    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kMacro3);
    r.onUplinkReceived(pkt);  // down at t=1

    mock_millis_value = 3002;
    r.onUplinkReceived(pkt);  // still held past threshold -- long-press fires
    TEST_ASSERT_EQUAL(1, driver.longButtonCount[3]);

    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);  // release after long-press
    TEST_ASSERT_EQUAL(0, driver.buttonUpCount[3]);  // suppressed
}

// ---- Mute (additive, not exclusive) --------------------------------------------

void test_mute_button_notifies_and_still_fires_normal_press() {
    FakeDriver driver;
    driver.params.mutebutton = 4;  // Macro4
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kMacro4);
    r.onUplinkReceived(pkt);
    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);

    TEST_ASSERT_EQUAL(1, driver.muteBtnUpCount);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[4]);
}

// ---- Telemetry and connect/failsafe lifecycle ----------------------------------

void test_uplink_updates_telemetry_and_connects() {
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);
    TEST_ASSERT_TRUE(r.failsafe());

    Snips::UplinkPacket pkt;
    pkt.triggerPercent = 42;
    pkt.stickXPercent = -10;
    pkt.stickYPercent = 20;
    pkt.batteryPercent = 77;
    pkt.chargeState = Snips::ChargeState::kCharging;
    r.onUplinkReceived(pkt);

    TEST_ASSERT_TRUE(r.isConnected());
    TEST_ASSERT_FALSE(r.failsafe());
    TEST_ASSERT_EQUAL(42, r.triggerPercent);
    TEST_ASSERT_EQUAL(-10, r.stickXPercent);
    TEST_ASSERT_EQUAL(20, r.stickYPercent);
    TEST_ASSERT_EQUAL(77, r.batteryPercent);
    TEST_ASSERT_TRUE(Snips::ChargeState::kCharging == r.chargeState);
}

void test_checkfailsafe_disconnects_after_timeout() {
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    mock_millis_value = 1000;
    Snips::UplinkPacket pkt;
    r.onUplinkReceived(pkt);
    TEST_ASSERT_TRUE(r.isConnected());

    mock_millis_value = 1000 + 500 + 1;  // past fst=500
    r.checkFailsafe(500);
    TEST_ASSERT_FALSE(r.isConnected());
}

void test_checkfailsafe_clears_alt_and_mute_state_on_disconnect() {
    FakeDriver driver;
    driver.params.altbtn = 7;
    driver.params.mutebutton = 8;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    mock_millis_value = 1000;
    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kBumper);  // hold alt
    r.onUplinkReceived(pkt);
    TEST_ASSERT_TRUE(driver.isAltHeld());

    mock_millis_value = 1000 + 500 + 1;
    r.checkFailsafe(500);
    TEST_ASSERT_FALSE(driver.isAltHeld());
    TEST_ASSERT_EQUAL(1, driver.resetMuteTimerCount);
}

void test_checkfailsafe_leaves_other_sides_alt_state_alone() {
    // altbtn=7 belongs to the RIGHT controller -- the LEFT instance
    // disconnecting must not touch alt state it doesn't own.
    FakeDriver driver;
    driver.params.altbtn = 7;
    TestSnipsRemote left(&driver, TestSnipsRemote::Side::kLeft);

    mock_millis_value = 1000;
    Snips::UplinkPacket pkt;
    left.onUplinkReceived(pkt);
    driver.altHeld = true;  // simulate the right side currently holding alt

    mock_millis_value = 1000 + 500 + 1;
    left.checkFailsafe(500);
    TEST_ASSERT_TRUE(driver.isAltHeld());  // untouched
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_right_macro1_press_release_fires_button1);
    RUN_TEST(test_right_stickclick_fires_button8);
    RUN_TEST(test_left_macro1_fires_button9);
    RUN_TEST(test_left_stickclick_fires_button16);

    RUN_TEST(test_alt_button_in_range_suppresses_own_press_and_sets_held);
    RUN_TEST(test_alt_button_out_of_range_for_this_side_is_ignored);
    RUN_TEST(test_press_while_alt_held_routes_to_alt_layer);

    RUN_TEST(test_long_press_fires_after_threshold_and_suppresses_up);

    RUN_TEST(test_mute_button_notifies_and_still_fires_normal_press);

    RUN_TEST(test_uplink_updates_telemetry_and_connects);
    RUN_TEST(test_checkfailsafe_disconnects_after_timeout);
    RUN_TEST(test_checkfailsafe_clears_alt_and_mute_state_on_disconnect);
    RUN_TEST(test_checkfailsafe_leaves_other_sides_alt_state_alone);

    return UNITY_END();
}
