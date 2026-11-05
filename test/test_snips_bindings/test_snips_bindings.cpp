// test_snips_bindings.cpp
// Tests for include/snips_bindings.h -- Snips analog inputs (trigger, tilt,
// rotation) mapped to drive/dome speed, dome position, servos and buttons.

#include "arduino_mock.h"
#include "snips_bindings.h"
#include <unity.h>
#include <string.h>

using namespace SnipsBindings;

void setUp(void) {}
void tearDown(void) {}

static Binding make(uint8_t side, Source src, Target tgt, int16_t lo, int16_t hi,
                    uint8_t arg = 0, bool inv = false) {
    Binding b;
    b.used = true; b.side = side; b.source = src; b.target = tgt;
    b.inMin = lo; b.inMax = hi; b.arg = arg; b.invert = inv;
    return b;
}

static Snips::UplinkPacket imuPacket() {
    Snips::UplinkPacket pkt;
    pkt.imuFlags = Snips::UplinkPacket::kImuTiltValid | Snips::UplinkPacket::kImuRatesValid;
    return pkt;
}

// ---- sources and ranges --------------------------------------------------------

void test_source_values() {
    Snips::UplinkPacket pkt = imuPacket();
    pkt.triggerPercent = 40;
    pkt.pitchDeg = -20;
    pkt.rollDeg = 35;
    pkt.rateDeciDps[2] = -905;
    float v;
    TEST_ASSERT_TRUE(sourceValue(Source::kTrigger, pkt, &v)); TEST_ASSERT_EQUAL_FLOAT(40, v);
    TEST_ASSERT_TRUE(sourceValue(Source::kPitch, pkt, &v));   TEST_ASSERT_EQUAL_FLOAT(-20, v);
    TEST_ASSERT_TRUE(sourceValue(Source::kRoll, pkt, &v));    TEST_ASSERT_EQUAL_FLOAT(35, v);
    TEST_ASSERT_TRUE(sourceValue(Source::kRateZ, pkt, &v));   TEST_ASSERT_FLOAT_WITHIN(0.001f, -90.5f, v);
}

void test_imu_sources_absent_without_imu_flags() {
    Snips::UplinkPacket pkt;  // older controller firmware, or no IMU
    float v;
    TEST_ASSERT_TRUE(sourceValue(Source::kTrigger, pkt, &v));
    TEST_ASSERT_FALSE(sourceValue(Source::kPitch, pkt, &v));
    TEST_ASSERT_FALSE(sourceValue(Source::kRateX, pkt, &v));
    pkt.imuFlags = Snips::UplinkPacket::kImuTiltValid;
    TEST_ASSERT_TRUE(sourceValue(Source::kRoll, pkt, &v));
    TEST_ASSERT_FALSE(sourceValue(Source::kRateY, pkt, &v));
}

void test_normalize_clamps_and_inverts() {
    Binding b = make(0, Source::kPitch, Target::kServo, -45, 45);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, normalize(b, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, normalize(b, -90));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, normalize(b, 90));
    b.invert = true;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, normalize(b, -45));
    b.inMin = b.inMax = 10;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, normalize(b, 10));  // zero width reads 0, inverted
}

void test_default_ranges() {
    int16_t lo, hi;
    defaultRange(Source::kTrigger, &lo, &hi); TEST_ASSERT_EQUAL(0, lo);   TEST_ASSERT_EQUAL(100, hi);
    defaultRange(Source::kRoll, &lo, &hi);    TEST_ASSERT_EQUAL(-45, lo); TEST_ASSERT_EQUAL(45, hi);
    defaultRange(Source::kRateZ, &lo, &hi);   TEST_ASSERT_EQUAL(-180, lo); TEST_ASSERT_EQUAL(180, hi);
}

// ---- config line -------------------------------------------------------------

void test_parse_and_format_round_trip() {
    Binding b;
    TEST_ASSERT_TRUE(parseBinding("1,5,3,0,-180,180,1", &b));
    TEST_ASSERT_TRUE(b.used);
    TEST_ASSERT_EQUAL(1, b.side);
    TEST_ASSERT_TRUE(Source::kRateZ == b.source);
    TEST_ASSERT_TRUE(Target::kDomePosition == b.target);
    TEST_ASSERT_EQUAL(-180, b.inMin);
    TEST_ASSERT_EQUAL(180, b.inMax);
    TEST_ASSERT_TRUE(b.invert);
    char buf[48];
    formatBinding(b, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1,5,3,0,-180,180,1", buf);
}

void test_parse_rejects_bad_lines() {
    Binding b;
    b.arg = 77;
    TEST_ASSERT_FALSE(parseBinding("", &b));
    TEST_ASSERT_FALSE(parseBinding("0,0,1,0,0,100", &b));      // short
    TEST_ASSERT_FALSE(parseBinding("2,0,1,0,0,100,0", &b));    // side
    TEST_ASSERT_FALSE(parseBinding("0,6,1,0,0,100,0", &b));    // source
    TEST_ASSERT_FALSE(parseBinding("0,0,6,0,0,100,0", &b));    // target
    TEST_ASSERT_FALSE(parseBinding("0,0,4,300,0,100,0", &b));  // arg
    TEST_ASSERT_FALSE(parseBinding("0,0,1,0,0,40000,0", &b));  // range
    TEST_ASSERT_EQUAL(77, b.arg);  // untouched on failure
}

void test_virtual_button_numbers() {
    TEST_ASSERT_EQUAL(25, buttonFor(0));
    TEST_ASSERT_EQUAL(30, buttonFor(kMaxBindings - 1));
}

// ---- evaluation -----------------------------------------------------------------

void test_accelerator_and_dome_speed_follow_trigger() {
    Binding bindings[kMaxBindings];
    bindings[0] = make(0, Source::kTrigger, Target::kDriveAccel, 0, 100);
    bindings[1] = make(1, Source::kTrigger, Target::kDomeSpeed, 0, 100);
    Evaluator eval;
    Snips::UplinkPacket pkt;
    pkt.triggerPercent = 25;

    Outputs right;
    eval.evaluate(0, pkt, 0, bindings, &right);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, right.driveScale);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, right.domeScale);  // left's binding

    Outputs left;
    eval.evaluate(1, pkt, 0, bindings, &left);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, left.driveScale);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, left.domeScale);
}

void test_unused_and_none_bindings_do_nothing() {
    Binding bindings[kMaxBindings];
    bindings[0] = make(0, Source::kTrigger, Target::kNone, 0, 100);
    bindings[1] = make(0, Source::kTrigger, Target::kDriveAccel, 0, 100);
    bindings[1].used = false;
    Evaluator eval;
    Outputs out;
    eval.evaluate(0, Snips::UplinkPacket(), 0, bindings, &out);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, out.driveScale);
    TEST_ASSERT_EQUAL(0, out.servoCount);
    TEST_ASSERT_EQUAL(0, out.buttonCount);
}

void test_servo_moves_across_range() {
    Binding bindings[kMaxBindings];
    bindings[2] = make(0, Source::kRoll, Target::kServo, -45, 45, 3);
    bindings[3] = make(0, Source::kRoll, Target::kServo, -45, 45, 0);  // no channel
    Evaluator eval;
    Snips::UplinkPacket pkt = imuPacket();
    pkt.rollDeg = 45;
    Outputs out;
    eval.evaluate(0, pkt, 0, bindings, &out);
    TEST_ASSERT_EQUAL(1, out.servoCount);
    TEST_ASSERT_EQUAL(3, out.servos[0].channel);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, out.servos[0].pos);
}

void test_tilt_dome_position_sweeps_plus_minus_arg() {
    Binding bindings[kMaxBindings];
    bindings[0] = make(1, Source::kRoll, Target::kDomePosition, -45, 45, 90);
    Evaluator eval;
    Snips::UplinkPacket pkt = imuPacket();
    pkt.rollDeg = -45;
    Outputs out;
    eval.evaluate(1, pkt, 0, bindings, &out);
    TEST_ASSERT_TRUE(out.hasDomeAngle);
    TEST_ASSERT_EQUAL_FLOAT(-90.0f, out.domeAngle);
}

void test_rate_dome_position_integrates_turn() {
    Binding bindings[kMaxBindings];
    bindings[0] = make(1, Source::kRateZ, Target::kDomePosition, -180, 180);
    Evaluator eval;
    Snips::UplinkPacket pkt = imuPacket();
    pkt.rateDeciDps[2] = 900;  // 90 deg/s

    Outputs first;
    eval.evaluate(1, pkt, 1000, bindings, &first);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, first.domeTurn);  // nothing to integrate yet
    TEST_ASSERT_FALSE(first.hasDomeAngle);

    Outputs second;
    eval.evaluate(1, pkt, 1100, bindings, &second);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.0f, second.domeTurn);

    bindings[0].invert = true;
    Outputs inverted;
    eval.evaluate(1, pkt, 1200, bindings, &inverted);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -9.0f, inverted.domeTurn);
}

void test_rate_integration_skips_long_gaps() {
    Binding bindings[kMaxBindings];
    bindings[0] = make(1, Source::kRateZ, Target::kDomePosition, -180, 180);
    Evaluator eval;
    Snips::UplinkPacket pkt = imuPacket();
    pkt.rateDeciDps[2] = 900;
    Outputs a, b;
    eval.evaluate(1, pkt, 1000, bindings, &a);
    eval.evaluate(1, pkt, 1000 + kMaxIntegrateGapMs + 1, bindings, &b);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, b.domeTurn);
}

void test_button_press_with_hysteresis() {
    Binding bindings[kMaxBindings];
    bindings[4] = make(0, Source::kTrigger, Target::kButton, 0, 100);
    Evaluator eval;
    Snips::UplinkPacket pkt;
    uint32_t t = 0;
    auto step = [&](uint8_t trigger) {
        pkt.triggerPercent = trigger;
        Outputs out;
        eval.evaluate(0, pkt, t += 50, bindings, &out);
        return out;
    };
    TEST_ASSERT_EQUAL(0, step(55).buttonCount);  // not past 60% yet
    TEST_ASSERT_EQUAL(0, step(70).buttonCount);  // pressed: fires on release
    TEST_ASSERT_EQUAL(0, step(45).buttonCount);  // still above 40%
    Outputs released = step(30);
    TEST_ASSERT_EQUAL(1, released.buttonCount);
    TEST_ASSERT_EQUAL(29, released.buttons[0].num);
    TEST_ASSERT_TRUE(released.buttons[0].up);
    TEST_ASSERT_FALSE(released.buttons[0].longUp);
}

void test_button_long_press() {
    Binding bindings[kMaxBindings];
    bindings[0] = make(0, Source::kTrigger, Target::kButton, 0, 100);
    Evaluator eval;
    Snips::UplinkPacket pkt;
    pkt.triggerPercent = 100;
    Outputs down;
    eval.evaluate(0, pkt, 1000, bindings, &down);
    Outputs held;
    eval.evaluate(0, pkt, 1000 + DEFAULT_LONG_PRESS_MS + 1, bindings, &held);
    TEST_ASSERT_EQUAL(1, held.buttonCount);
    TEST_ASSERT_TRUE(held.buttons[0].longUp);
    pkt.triggerPercent = 0;
    Outputs up;
    eval.evaluate(0, pkt, 1000 + DEFAULT_LONG_PRESS_MS + 50, bindings, &up);
    TEST_ASSERT_EQUAL(0, up.buttonCount);  // the long press replaced the short one
}

void test_reset_forgets_held_button() {
    Binding bindings[kMaxBindings];
    bindings[0] = make(0, Source::kTrigger, Target::kButton, 0, 100);
    Evaluator eval;
    Snips::UplinkPacket pkt;
    pkt.triggerPercent = 100;
    Outputs out;
    eval.evaluate(0, pkt, 1000, bindings, &out);
    eval.reset(0, bindings);  // link lost while held
    pkt.triggerPercent = 0;
    Outputs after;
    eval.evaluate(0, pkt, 2000, bindings, &after);
    TEST_ASSERT_EQUAL(0, after.buttonCount);
}

void test_highest_accelerator_wins_on_one_side() {
    Binding bindings[kMaxBindings];
    bindings[0] = make(0, Source::kTrigger, Target::kDriveAccel, 0, 100);
    bindings[1] = make(0, Source::kPitch, Target::kDriveAccel, 0, 45);
    Evaluator eval;
    Snips::UplinkPacket pkt = imuPacket();
    pkt.triggerPercent = 20;
    pkt.pitchDeg = 45;
    Outputs out;
    eval.evaluate(0, pkt, 0, bindings, &out);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, out.driveScale);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_source_values);
    RUN_TEST(test_imu_sources_absent_without_imu_flags);
    RUN_TEST(test_normalize_clamps_and_inverts);
    RUN_TEST(test_default_ranges);
    RUN_TEST(test_parse_and_format_round_trip);
    RUN_TEST(test_parse_rejects_bad_lines);
    RUN_TEST(test_virtual_button_numbers);
    RUN_TEST(test_accelerator_and_dome_speed_follow_trigger);
    RUN_TEST(test_unused_and_none_bindings_do_nothing);
    RUN_TEST(test_servo_moves_across_range);
    RUN_TEST(test_tilt_dome_position_sweeps_plus_minus_arg);
    RUN_TEST(test_rate_dome_position_integrates_turn);
    RUN_TEST(test_rate_integration_skips_long_gaps);
    RUN_TEST(test_button_press_with_hysteresis);
    RUN_TEST(test_button_long_press);
    RUN_TEST(test_reset_forgets_held_button);
    RUN_TEST(test_highest_accelerator_wins_on_one_side);
    return UNITY_END();
}
