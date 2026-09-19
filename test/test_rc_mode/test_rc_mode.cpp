// test_rc_mode.cpp
// Unit tests for AmidalaController::animate()'s CONTROLLER_TYPE_RC branch
// (src/controller.cpp) -- controller.cpp depends on heavy Reeltwo/Arduino
// classes and isn't compiled natively (test_build_src=no, see CLAUDE.md), so
// this mirrors just the RC-mode gating/dispatch logic by hand, same
// convention as test_bt_gamepad.cpp's dispatch mirror. Must stay in sync
// with src/controller.cpp's RC branch.
//
// What this guards against (both real bugs caught by code review before
// this ever shipped): update() must run every tick regardless of decode()
// success -- it's what drives DriveController/DomeController::notify()'s
// lastPacket-lag safety-stop, so freezing it on a failed decode would leave
// a lost RC signal undetected forever. lastPacket must only refresh on an
// actual successful decode, so a lost signal grows stale exactly like a
// lost XBee link would. The "RC Enabled" log + PPMDecoder::init() must fire
// on every distinct transition into RC mode (including re-entering after
// switching away live, no reboot), not just once at boot.

#include "arduino_mock.h"
#include "params.h"
#include <unity.h>

void setUp(void) { mock_millis_value = 0; }
void tearDown(void) {}

struct FakeRemote {
    int type = 0;
    uint32_t lastPacket = 0;
    int x = 0, y = 0, w1 = 0;
    int updateCount = 0;
    void update() { updateCount++; }
};

struct FakeDecoder {
    bool nextResult = false;
    int initCount = 0;
    int ch[6] = {0, 0, 0, 0, 0, 0};
    void init() { initCount++; }
    bool decode() { return nextResult; }
    int channel(int idx, int, int, int) { return ch[idx]; }
};

static const int kFakeRC = 99;  // stand-in for XBeePocketRemote::kRC

// Mirror of AmidalaController::animate()'s CONTROLLER_TYPE_RC handling
// (src/controller.cpp) -- the transition-edge log/init check plus the RC
// branch body itself.
struct RCModeMirror {
    int lastControllerType = -1;
    int logCount = 0;

    void tick(int controllertype, FakeRemote& r0, FakeRemote& r1, FakeDecoder& decoder) {
        if (controllertype == CONTROLLER_TYPE_RC && lastControllerType != CONTROLLER_TYPE_RC) {
            logCount++;
            decoder.init();
        }
        lastControllerType = controllertype;

        if (controllertype != CONTROLLER_TYPE_RC) return;

        r0.type = kFakeRC;
        r1.type = kFakeRC;
        if (decoder.decode()) {
            r0.lastPacket = millis();
            r0.x  = decoder.channel(0, 0, 1024, 512);
            r0.y  = decoder.channel(1, 0, 1024, 512);
            r0.w1 = decoder.channel(4, 0, 1024, 0);

            r1.lastPacket = millis();
            r1.x  = decoder.channel(2, 0, 1024, 512);
            r1.y  = decoder.channel(3, 0, 1024, 512);
            r1.w1 = decoder.channel(5, 0, 1024, 0);
        }
        r0.update();
        r1.update();
    }
};

void test_update_runs_every_tick_even_when_decode_fails() {
    RCModeMirror rc;
    FakeRemote r0, r1;
    FakeDecoder decoder;
    decoder.nextResult = false;

    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);
    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);

    TEST_ASSERT_EQUAL(2, r0.updateCount);
    TEST_ASSERT_EQUAL(2, r1.updateCount);
}

void test_lastpacket_only_refreshes_on_successful_decode() {
    RCModeMirror rc;
    FakeRemote r0, r1;
    FakeDecoder decoder;

    decoder.nextResult = true;
    mock_millis_value = 1000;
    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);
    TEST_ASSERT_EQUAL(1000, r0.lastPacket);
    TEST_ASSERT_EQUAL(1000, r1.lastPacket);

    // Signal lost -- lastPacket must NOT advance even though time passes and
    // update() keeps running (this is what lets DriveController/
    // DomeController::notify()'s lag check ever see the signal as stale).
    decoder.nextResult = false;
    mock_millis_value = 6000;
    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);
    TEST_ASSERT_EQUAL(1000, r0.lastPacket);
    TEST_ASSERT_EQUAL(1000, r1.lastPacket);
    TEST_ASSERT_EQUAL(2, r0.updateCount);  // still ran despite the failed decode
}

void test_type_set_to_rc_every_tick_regardless_of_decode() {
    RCModeMirror rc;
    FakeRemote r0, r1;
    FakeDecoder decoder;
    decoder.nextResult = false;

    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);
    TEST_ASSERT_EQUAL(kFakeRC, r0.type);
    TEST_ASSERT_EQUAL(kFakeRC, r1.type);
}

void test_channels_mapped_to_correct_remote_and_field() {
    RCModeMirror rc;
    FakeRemote r0, r1;
    FakeDecoder decoder;
    decoder.nextResult = true;
    decoder.ch[0] = 111; decoder.ch[1] = 222; decoder.ch[4] = 333;  // remote 0
    decoder.ch[2] = 444; decoder.ch[3] = 555; decoder.ch[5] = 666;  // remote 1

    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);

    TEST_ASSERT_EQUAL(111, r0.x);
    TEST_ASSERT_EQUAL(222, r0.y);
    TEST_ASSERT_EQUAL(333, r0.w1);
    TEST_ASSERT_EQUAL(444, r1.x);
    TEST_ASSERT_EQUAL(555, r1.y);
    TEST_ASSERT_EQUAL(666, r1.w1);
}

void test_log_and_init_fire_once_on_first_entry() {
    RCModeMirror rc;
    FakeRemote r0, r1;
    FakeDecoder decoder;

    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);
    TEST_ASSERT_EQUAL(1, rc.logCount);
    TEST_ASSERT_EQUAL(1, decoder.initCount);
}

void test_log_and_init_do_not_repeat_while_staying_in_rc_mode() {
    RCModeMirror rc;
    FakeRemote r0, r1;
    FakeDecoder decoder;

    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);
    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);
    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);

    TEST_ASSERT_EQUAL(1, rc.logCount);
    TEST_ASSERT_EQUAL(1, decoder.initCount);
}

// Regression: a plain one-shot "already announced" flag (no reset path)
// would miss this -- re-entering RC mode after switching away live (no
// reboot) must announce and re-init again, not just once ever per boot.
void test_log_and_init_fire_again_on_reentry_after_leaving() {
    RCModeMirror rc;
    FakeRemote r0, r1;
    FakeDecoder decoder;

    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);       // enter RC
    rc.tick(CONTROLLER_TYPE_XBEE, r0, r1, decoder);      // leave RC
    rc.tick(CONTROLLER_TYPE_RC, r0, r1, decoder);       // re-enter RC

    TEST_ASSERT_EQUAL(2, rc.logCount);
    TEST_ASSERT_EQUAL(2, decoder.initCount);
}

void test_no_log_or_dispatch_when_not_in_rc_mode() {
    RCModeMirror rc;
    FakeRemote r0, r1;
    FakeDecoder decoder;
    decoder.nextResult = true;

    rc.tick(CONTROLLER_TYPE_XBEE, r0, r1, decoder);

    TEST_ASSERT_EQUAL(0, rc.logCount);
    TEST_ASSERT_EQUAL(0, decoder.initCount);
    TEST_ASSERT_EQUAL(0, r0.updateCount);
    TEST_ASSERT_EQUAL(0, r1.updateCount);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_update_runs_every_tick_even_when_decode_fails);
    RUN_TEST(test_lastpacket_only_refreshes_on_successful_decode);
    RUN_TEST(test_type_set_to_rc_every_tick_regardless_of_decode);
    RUN_TEST(test_channels_mapped_to_correct_remote_and_field);
    RUN_TEST(test_log_and_init_fire_once_on_first_entry);
    RUN_TEST(test_log_and_init_do_not_repeat_while_staying_in_rc_mode);
    RUN_TEST(test_log_and_init_fire_again_on_reentry_after_leaving);
    RUN_TEST(test_no_log_or_dispatch_when_not_in_rc_mode);

    return UNITY_END();
}
