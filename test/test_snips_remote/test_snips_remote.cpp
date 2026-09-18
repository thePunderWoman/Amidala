// test_snips_remote.cpp
// Tests for include/snips_remote.h / src/snips_remote.cpp -- SnipsRemote's
// button dispatch, alt/mute handling, connect/failsafe lifecycle, and
// downlink-echo derivation (issue #204 phases 1-2).
//
// SnipsRemote's real methods need a fully-constructed AmidalaController
// (fDriver->params, isAltHeld/setAltHeld/noteButtonUp/etc), which pulls in
// the whole firmware -- same situation test_xbee_remote.cpp is in with
// DriveController/DomeController (whose real notify()/process() bodies are
// stubbed for linking, not exercised, in that file). SnipsRemote isn't
// templated on its driver type, so its real onUplinkReceived()/
// checkFailsafe()/buildDownlinkPacket() (src/snips_remote.cpp) can't be
// compiled against a fake here either -- TestSnipsRemote below is a
// hand-written mirror of those bodies against a FakeDriver, same "must be
// kept in sync by hand" approach as test_double_press.cpp's FakeDblPress
// mirrors AmidalaController's double-press logic. The long-press timer and
// alt/dispatch decision inside it are NOT re-mirrored, though -- those call
// the real button_dispatch.h functions (no BLE/controller dependency), so
// only the per-side numbering/alt-range/mute-range/echo-derivation glue
// that's genuinely local to SnipsRemote needs to be kept in sync by hand.

#include "arduino_mock.h"
#include "snips_packet.h"
#include "snips_buttons.h"
#include "snips_remote.h"
#include "button_dispatch.h"
#include "button_actions.h"
#include <unity.h>
#include <string.h>
#include <stdio.h>

void setUp(void) { mock_millis_value = 0; }
void tearDown(void) {}

// ---- Fake AmidalaController -------------------------------------------------
// Mirrors just the surface SnipsRemote's methods use: params.altbtn/
// mutebutton/volumewheel/altvolumewheel/driveSpeedPct/B[], fAudio's live
// volume readback, and the button-dispatch/alt/mute methods.

struct FakeAudio {
    // Returns a value that encodes which wheel was asked for, so tests can
    // confirm the right wheel selector was passed through without needing
    // to replicate AmidalaAudio's real channel-routing semantics.
    uint8_t getEffectiveVolume(uint8_t wheel) const { return (uint8_t)(40 + wheel); }
};

struct FakeParams {
    int altbtn = 0;
    int mutebutton = 0;
    uint8_t volumewheel = 0;
    uint8_t altvolumewheel = 0;
    uint8_t driveSpeedPct = 100;
    ButtonAction B[24] = {};
};

struct FakeDriver {
    FakeParams params;
    FakeAudio fAudio;
    bool altHeld = false;

    int buttonUpCount[25]   = {0};  // 1-based, index 0 unused
    int altButtonCount[25]  = {0};
    int longButtonCount[25] = {0};
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
// the exact same orchestration logic (verbatim copy of
// src/snips_remote.cpp's bodies) the same way test_double_press.cpp's
// FakeDblPress mirrors AmidalaController's double-press logic. Button
// numbering itself is NOT re-mirrored, though -- SnipsRemote::
// buttonNumberFor()/ownsButtonNumber()/bitIndexForButtonNumber() are public
// static pure functions (include/snips_remote.h) with no AmidalaController
// dependency, so this calls the real ones directly. That was a deliberate
// design choice made after an early version of this mirror duplicated the
// numbering scheme and drifted out of sync with a fix to the real one
// (issue #204 phase 2 review) -- fewer hand-mirrored pieces, less to drift.
class TestSnipsRemote {
public:
    using Side = SnipsRemote::Side;
    TestSnipsRemote(FakeDriver *driver, Side side) : fDriver(driver), fSide(side) {}

    uint32_t addr = 0;
    uint16_t addr16 = 0;
    bool isConnected() const { return fConnected; }
    bool failsafe() const { return !fConnected; }
    uint8_t triggerPercent = 0;
    int8_t stickXPercent = 0;
    int8_t stickYPercent = 0;
    uint8_t batteryPercent = 0;
    Snips::ChargeState chargeState = Snips::ChargeState::kDone;

    void onUplinkReceived(const Snips::UplinkPacket &pkt, uint16_t senderAddr16 = 0) {
        uint32_t now = millis();
        addr16 = senderAddr16;
        if (fDriver) {
            int altbtn = fDriver->params.altbtn;
            bool altbtnInRange = altbtn > 0 && SnipsRemote::ownsButtonNumber(fSide, (unsigned)altbtn);
            if (altbtnInRange) {
                unsigned altBit = SnipsRemote::bitIndexForButtonNumber(fSide, (unsigned)altbtn);
                fDriver->setAltHeld((pkt.buttonMask & (1u << altBit)) != 0);
            }
            bool altHeld = fDriver->isAltHeld();

            int muteBtn = fDriver->params.mutebutton;
            bool muteBtnInRange = muteBtn > 0 && SnipsRemote::ownsButtonNumber(fSide, (unsigned)muteBtn);
            unsigned muteBit = muteBtnInRange
                ? SnipsRemote::bitIndexForButtonNumber(fSide, (unsigned)muteBtn)
                : SnipsButtons::kCount;

            for (unsigned i = 0; i < SnipsButtons::kCount; i++) {
                bool prevPressed = (fPrevButtonMask & (1u << i)) != 0;
                bool pressed = (pkt.buttonMask & (1u << i)) != 0;
                bool down = !prevPressed && pressed;
                bool up = prevPressed && !pressed;
                auto r = fLongPress[i].update(down, up, pressed, now, DEFAULT_LONG_PRESS_MS);
                unsigned num = SnipsRemote::buttonNumberFor(fSide, i);
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
        int altbtn = fDriver->params.altbtn;
        if (altbtn > 0 && SnipsRemote::ownsButtonNumber(fSide, (unsigned)altbtn))
            fDriver->setAltHeld(false);
        int muteBtn = fDriver->params.mutebutton;
        if (muteBtn > 0 && SnipsRemote::ownsButtonNumber(fSide, (unsigned)muteBtn))
            fDriver->resetMutePressTimer();
    }

    void fillPairFields(const ButtonAction &action, char *outLabel, char *outValue) const {
        outLabel[0] = '\0';
        outValue[0] = '\0';
        if (!fDriver) return;
        switch (action.action) {
        case ButtonAction::kVolumeStep: {
            uint8_t wheel = (action.volstep.target && fDriver->params.altvolumewheel != 0)
                                ? fDriver->params.altvolumewheel
                                : fDriver->params.volumewheel;
            uint8_t value = fDriver->fAudio.getEffectiveVolume(wheel);
            snprintf(outLabel, 9, "%s", action.volstep.target ? "AltVol" : "Vol");
            snprintf(outValue, 9, "%u", value);
            break;
        }
        case ButtonAction::kThrottleStep:
            snprintf(outLabel, 9, "%s", "Drive");
            snprintf(outValue, 9, "%u", fDriver->params.driveSpeedPct);
            break;
        default:
            break;
        }
    }

    Snips::DownlinkPacket buildDownlinkPacket() const {
        Snips::DownlinkPacket pkt;
        pkt.handedness = (fSide == SnipsRemote::kRight) ? Snips::DownlinkPacket::Handedness::kRight
                                                         : Snips::DownlinkPacket::Handedness::kLeft;
        if (!fDriver) return pkt;
        unsigned leftUpButton = SnipsRemote::buttonNumberFor(fSide, SnipsButtons::kLeftUp);
        unsigned rightUpButton = SnipsRemote::buttonNumberFor(fSide, SnipsButtons::kRightUp);
        fillPairFields(fDriver->params.B[leftUpButton - 1], pkt.leftLabel, pkt.leftValue);
        fillPairFields(fDriver->params.B[rightUpButton - 1], pkt.rightLabel, pkt.rightValue);
        return pkt;
    }

private:
    FakeDriver *fDriver;
    Side fSide;
    bool fConnected = false;
    uint32_t fLastPacket = 0;
    uint16_t fPrevButtonMask = 0;
    ButtonLongPress fLongPress[SnipsButtons::kCount];
};

// ---- Right-side (buttons 1-12) numbering and dispatch -----------------------

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

void test_right_stateful_buttons_fire_21_through_24() {
    // Right's stateful buttons (LeftUp/LeftDown/RightUp/RightDown) are
    // deliberately pushed to 21-24, not 9-12 -- see
    // SnipsRemote::buttonNumberFor()'s comment: 9 is dome-stick "square"
    // under CONTROLLER_TYPE_XBEE/_BLUETOOTH, and B[]/LB[]/AB[]/DB[] are
    // shared storage, so a default landing there would silently change
    // stock XBee/Bluetooth behavior.
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kLeftUp);
    r.onUplinkReceived(pkt);
    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[21]);

    pkt.buttonMask = (1u << SnipsButtons::kRightDown);
    r.onUplinkReceived(pkt);
    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[24]);
}

void test_no_stateful_button_number_ever_falls_in_1_to_9() {
    // Regression test for the bug this numbering scheme exists to avoid:
    // CONTROLLER_TYPE_XBEE/_BLUETOOTH give buttons 1-9 a real, live meaning
    // (drive/dome stick presses) against the SAME shared B[]/LB[]/AB[]/DB[]
    // storage -- a stateful button (which gets a compiled-in non-kNone
    // default, see params.h's init()) landing there would silently change
    // stock XBee/Bluetooth behavior.
    for (unsigned bit = SnipsButtons::kLeftUp; bit < SnipsButtons::kCount; bit++) {
        unsigned rightNum = SnipsRemote::buttonNumberFor(SnipsRemote::kRight, bit);
        unsigned leftNum  = SnipsRemote::buttonNumberFor(SnipsRemote::kLeft, bit);
        TEST_ASSERT_FALSE(rightNum >= 1 && rightNum <= 9);
        TEST_ASSERT_FALSE(leftNum >= 1 && leftNum <= 9);
    }
}

// ---- Left-side numbering (fire-and-forget 9-16, stateful 17-20) -------------

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

void test_left_rightdown_fires_button20() {
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kLeft);

    Snips::UplinkPacket pkt;
    pkt.buttonMask = (1u << SnipsButtons::kRightDown);
    r.onUplinkReceived(pkt);
    pkt.buttonMask = 0;
    r.onUplinkReceived(pkt);
    TEST_ASSERT_EQUAL(1, driver.buttonUpCount[20]);
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
    pkt.buttonMask = (1u << SnipsButtons::kBumper);  // left's bit 6 -> button 15
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

// ---- Telemetry, addr16, and connect/failsafe lifecycle -------------------------

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
    r.onUplinkReceived(pkt, 0xBEEF);

    TEST_ASSERT_TRUE(r.isConnected());
    TEST_ASSERT_FALSE(r.failsafe());
    TEST_ASSERT_EQUAL(42, r.triggerPercent);
    TEST_ASSERT_EQUAL(-10, r.stickXPercent);
    TEST_ASSERT_EQUAL(20, r.stickYPercent);
    TEST_ASSERT_EQUAL(77, r.batteryPercent);
    TEST_ASSERT_TRUE(Snips::ChargeState::kCharging == r.chargeState);
    TEST_ASSERT_EQUAL_HEX16(0xBEEF, r.addr16);
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

// ---- buildDownlinkPacket() / fillPairFields() (issue #204 phase 2) ----------

void test_downlink_handedness_matches_side() {
    FakeDriver driver;
    TestSnipsRemote right(&driver, TestSnipsRemote::Side::kRight);
    TestSnipsRemote left(&driver, TestSnipsRemote::Side::kLeft);

    TEST_ASSERT_TRUE(Snips::DownlinkPacket::Handedness::kRight == right.buildDownlinkPacket().handedness);
    TEST_ASSERT_TRUE(Snips::DownlinkPacket::Handedness::kLeft == left.buildDownlinkPacket().handedness);
}

void test_downlink_unassigned_pair_is_blank() {
    FakeDriver driver;  // params.B[] all default-constructed to kNone
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::DownlinkPacket pkt = r.buildDownlinkPacket();
    TEST_ASSERT_EQUAL_STRING("", pkt.leftLabel);
    TEST_ASSERT_EQUAL_STRING("", pkt.leftValue);
    TEST_ASSERT_EQUAL_STRING("", pkt.rightLabel);
    TEST_ASSERT_EQUAL_STRING("", pkt.rightValue);
}

void test_downlink_volumestep_shows_vol_label_and_live_value() {
    FakeDriver driver;
    driver.params.volumewheel = 2;
    // Right controller's LeftUp is button 21 (index 20).
    driver.params.B[20].action = ButtonAction::kVolumeStep;
    driver.params.B[20].volstep.dir = 1;
    driver.params.B[20].volstep.target = 0;  // plain
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::DownlinkPacket pkt = r.buildDownlinkPacket();
    TEST_ASSERT_EQUAL_STRING("Vol", pkt.leftLabel);
    TEST_ASSERT_EQUAL_STRING("42", pkt.leftValue);  // FakeAudio: 40 + wheel(2)
}

void test_downlink_alt_volumestep_shows_altvol_label_and_routes_altvolumewheel() {
    FakeDriver driver;
    driver.params.altvolumewheel = 3;
    driver.params.B[20].action = ButtonAction::kVolumeStep;
    driver.params.B[20].volstep.target = 1;  // alt
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::DownlinkPacket pkt = r.buildDownlinkPacket();
    TEST_ASSERT_EQUAL_STRING("AltVol", pkt.leftLabel);
    TEST_ASSERT_EQUAL_STRING("43", pkt.leftValue);  // FakeAudio: 40 + wheel(3)
}

void test_downlink_alt_volumestep_falls_through_to_volumewheel_when_altvolumewheel_zero() {
    // Regression test: altvolumewheel==0 (the default) means
    // AmidalaAudio::setAltVolumeNoResponse() routes via params.volumewheel
    // instead (see its own fallthrough) -- the echo-back read must resolve
    // the same channel, not read wheel 0 ("global"/fSavedVolV) while the
    // write actually landed on whatever volumewheel points at.
    FakeDriver driver;
    driver.params.volumewheel = 2;      // chA
    driver.params.altvolumewheel = 0;   // fall through to volumewheel
    driver.params.B[20].action = ButtonAction::kVolumeStep;
    driver.params.B[20].volstep.target = 1;  // alt
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::DownlinkPacket pkt = r.buildDownlinkPacket();
    TEST_ASSERT_EQUAL_STRING("AltVol", pkt.leftLabel);
    TEST_ASSERT_EQUAL_STRING("42", pkt.leftValue);  // FakeAudio: 40 + wheel(2), NOT wheel(0)
}

void test_downlink_throttlestep_shows_drive_label_and_speed_pct() {
    FakeDriver driver;
    driver.params.driveSpeedPct = 65;
    // Right controller's RightUp is button 23 (index 22).
    driver.params.B[22].action = ButtonAction::kThrottleStep;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);

    Snips::DownlinkPacket pkt = r.buildDownlinkPacket();
    TEST_ASSERT_EQUAL_STRING("Drive", pkt.rightLabel);
    TEST_ASSERT_EQUAL_STRING("65", pkt.rightValue);
}

void test_downlink_reflects_config_change_without_a_button_press() {
    // The display is recomputed fresh every call from current config +
    // live state -- no edge-tracking, so a config change alone (e.g. via
    // the web UI) shows up on the very next downlink with no button press
    // needed.
    FakeDriver driver;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kRight);
    TEST_ASSERT_EQUAL_STRING("", r.buildDownlinkPacket().leftLabel);

    driver.params.B[20].action = ButtonAction::kThrottleStep;
    driver.params.driveSpeedPct = 80;
    Snips::DownlinkPacket pkt = r.buildDownlinkPacket();
    TEST_ASSERT_EQUAL_STRING("Drive", pkt.leftLabel);
    TEST_ASSERT_EQUAL_STRING("80", pkt.leftValue);
}

void test_downlink_left_controller_uses_its_own_button_range() {
    FakeDriver driver;
    driver.params.driveSpeedPct = 50;
    // Left controller's LeftUp is button 17 (index 16).
    driver.params.B[16].action = ButtonAction::kThrottleStep;
    TestSnipsRemote r(&driver, TestSnipsRemote::Side::kLeft);

    Snips::DownlinkPacket pkt = r.buildDownlinkPacket();
    TEST_ASSERT_EQUAL_STRING("Drive", pkt.leftLabel);
    TEST_ASSERT_EQUAL_STRING("50", pkt.leftValue);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_right_macro1_press_release_fires_button1);
    RUN_TEST(test_right_stickclick_fires_button8);
    RUN_TEST(test_right_stateful_buttons_fire_21_through_24);
    RUN_TEST(test_no_stateful_button_number_ever_falls_in_1_to_9);
    RUN_TEST(test_left_macro1_fires_button9);
    RUN_TEST(test_left_stickclick_fires_button16);
    RUN_TEST(test_left_rightdown_fires_button20);

    RUN_TEST(test_alt_button_in_range_suppresses_own_press_and_sets_held);
    RUN_TEST(test_alt_button_out_of_range_for_this_side_is_ignored);
    RUN_TEST(test_press_while_alt_held_routes_to_alt_layer);

    RUN_TEST(test_long_press_fires_after_threshold_and_suppresses_up);

    RUN_TEST(test_mute_button_notifies_and_still_fires_normal_press);

    RUN_TEST(test_uplink_updates_telemetry_and_connects);
    RUN_TEST(test_checkfailsafe_disconnects_after_timeout);
    RUN_TEST(test_checkfailsafe_clears_alt_and_mute_state_on_disconnect);
    RUN_TEST(test_checkfailsafe_leaves_other_sides_alt_state_alone);

    RUN_TEST(test_downlink_handedness_matches_side);
    RUN_TEST(test_downlink_unassigned_pair_is_blank);
    RUN_TEST(test_downlink_volumestep_shows_vol_label_and_live_value);
    RUN_TEST(test_downlink_alt_volumestep_shows_altvol_label_and_routes_altvolumewheel);
    RUN_TEST(test_downlink_alt_volumestep_falls_through_to_volumewheel_when_altvolumewheel_zero);
    RUN_TEST(test_downlink_throttlestep_shows_drive_label_and_speed_pct);
    RUN_TEST(test_downlink_reflects_config_change_without_a_button_press);
    RUN_TEST(test_downlink_left_controller_uses_its_own_button_range);

    return UNITY_END();
}
