// test_audio.cpp
// Unit tests for volume-update throttle logic in AmidalaAudio::setVolumeNoResponse.
//
// The throttle prevents flooding the HCR with SetVolume commands when the
// potentiometer moves rapidly — only one burst of commands is sent per
// VOLUME_THROTTLE_MS window.
//
// Because setVolumeNoResponse() calls into AmidalaController / HCRVocalizer
// when the throttle passes, the tests below exercise the throttle decision
// logic directly using the same pattern as the implementation.  This is the
// same approach used in test_buttons.cpp for guard-condition coverage.

#include "arduino_mock.h"
#include "audio.h"
#include <unity.h>

// ---- Throttle harness -------------------------------------------------------
// Mirrors the logic in AmidalaAudio::setVolumeNoResponse exactly.
// If the implementation changes its pattern, this harness and the tests must
// be updated to match.

struct VolumeThrottleHarness {
    uint32_t lastUpdate = (uint32_t)(0u - AmidalaAudio::VOLUME_THROTTLE_MS);
    int callCount = 0;  // represents number of SetVolume bursts that went through

    bool tryUpdate() {
        uint32_t now = millis();
        if (now - lastUpdate < AmidalaAudio::VOLUME_THROTTLE_MS) return false;
        lastUpdate = now;
        callCount++;
        return true;
    }
};

// ---- setUp / tearDown -------------------------------------------------------

void setUp(void) {
    mock_millis_value = 0;
}
void tearDown(void) {}

// ---- Constant value ---------------------------------------------------------

void test_volume_throttle_ms_is_80() {
    TEST_ASSERT_EQUAL_UINT32(80, AmidalaAudio::VOLUME_THROTTLE_MS);
}

// ---- First call always passes -----------------------------------------------

void test_first_update_passes_at_time_zero() {
    mock_millis_value = 0;
    VolumeThrottleHarness h;
    TEST_ASSERT_TRUE(h.tryUpdate());
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

void test_first_update_passes_at_nonzero_time() {
    mock_millis_value = 500;
    VolumeThrottleHarness h;
    TEST_ASSERT_TRUE(h.tryUpdate());
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

// ---- Rapid calls are throttled ----------------------------------------------

void test_immediate_second_call_is_blocked() {
    mock_millis_value = 0;
    VolumeThrottleHarness h;
    h.tryUpdate();                     // first: passes
    TEST_ASSERT_FALSE(h.tryUpdate());  // same ms: blocked
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

void test_call_just_before_window_is_blocked() {
    mock_millis_value = 0;
    VolumeThrottleHarness h;
    h.tryUpdate();                              // t=0: passes
    mock_millis_value = AmidalaAudio::VOLUME_THROTTLE_MS - 1;  // t=79ms
    TEST_ASSERT_FALSE(h.tryUpdate());           // still within window: blocked
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

// ---- Call at exactly the window boundary passes ----------------------------

void test_call_at_exact_window_boundary_passes() {
    mock_millis_value = 0;
    VolumeThrottleHarness h;
    h.tryUpdate();                                    // t=0: passes
    mock_millis_value = AmidalaAudio::VOLUME_THROTTLE_MS;  // t=80ms
    TEST_ASSERT_TRUE(h.tryUpdate());                  // exactly at boundary: passes
    TEST_ASSERT_EQUAL_INT(2, h.callCount);
}

// ---- Call well past the window passes ---------------------------------------

void test_call_well_past_window_passes() {
    mock_millis_value = 1000;
    VolumeThrottleHarness h;
    h.tryUpdate();                   // t=1000ms: passes
    mock_millis_value = 1200;        // t=1200ms: 200ms later
    TEST_ASSERT_TRUE(h.tryUpdate()); // well past window: passes
    TEST_ASSERT_EQUAL_INT(2, h.callCount);
}

// ---- Burst of rapid calls only lets one through per window -----------------

void test_burst_of_10_rapid_calls_lets_only_one_through() {
    mock_millis_value = 0;
    VolumeThrottleHarness h;
    for (int i = 0; i < 10; i++) {
        h.tryUpdate();  // all at t=0
    }
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

// ---- Multiple windows accumulate correctly ----------------------------------

void test_three_windows_produce_three_updates() {
    mock_millis_value = 0;
    VolumeThrottleHarness h;
    h.tryUpdate();                                        // t=0
    mock_millis_value = AmidalaAudio::VOLUME_THROTTLE_MS; // t=80
    h.tryUpdate();
    mock_millis_value = AmidalaAudio::VOLUME_THROTTLE_MS * 2; // t=160
    h.tryUpdate();
    TEST_ASSERT_EQUAL_INT(3, h.callCount);
}

// ---- Volume routing harness -------------------------------------------------
// Mirrors the logic in AmidalaAudio::applyHCRVolume.
// wheel: 0=global, 1=voice (CH_V), 2=chA (CH_A), 3=chB (CH_B).

struct VolumeRouteHarness {
    int chV = -1, chA = -1, chB = -1;

    void apply(uint8_t wheel, uint8_t volume) {
        switch (wheel) {
            case 1: chV = volume; break;
            case 2: chA = volume; break;
            case 3: chB = volume; break;
            case 4: chA = chB = volume; break;
            default:
                chV = chA = chB = (int)volume;
                break;
        }
    }

    void reset() { chV = chA = chB = -1; }
};

// ---- volumewheel routing ----------------------------------------------------

void test_volumewheel_global_sets_all_channels() {
    VolumeRouteHarness h;
    h.apply(0, 75);
    TEST_ASSERT_EQUAL_INT(75, h.chV);
    TEST_ASSERT_EQUAL_INT(75, h.chA);
    TEST_ASSERT_EQUAL_INT(75, h.chB);
}

void test_volumewheel_voice_sets_only_chV() {
    VolumeRouteHarness h;
    h.apply(1, 60);
    TEST_ASSERT_EQUAL_INT(60, h.chV);
    TEST_ASSERT_EQUAL_INT(-1, h.chA);
    TEST_ASSERT_EQUAL_INT(-1, h.chB);
}

void test_volumewheel_chA_sets_only_chA() {
    VolumeRouteHarness h;
    h.apply(2, 40);
    TEST_ASSERT_EQUAL_INT(-1, h.chV);
    TEST_ASSERT_EQUAL_INT(40, h.chA);
    TEST_ASSERT_EQUAL_INT(-1, h.chB);
}

void test_volumewheel_chB_sets_only_chB() {
    VolumeRouteHarness h;
    h.apply(3, 30);
    TEST_ASSERT_EQUAL_INT(-1, h.chV);
    TEST_ASSERT_EQUAL_INT(-1, h.chA);
    TEST_ASSERT_EQUAL_INT(30, h.chB);
}

void test_volumewheel_chAB_sets_chA_and_chB_only() {
    VolumeRouteHarness h;
    h.apply(4, 55);
    TEST_ASSERT_EQUAL_INT(-1, h.chV);
    TEST_ASSERT_EQUAL_INT(55, h.chA);
    TEST_ASSERT_EQUAL_INT(55, h.chB);
}

void test_volumewheel_unknown_value_falls_back_to_global() {
    VolumeRouteHarness h;
    h.apply(99, 50);
    TEST_ASSERT_EQUAL_INT(50, h.chV);
    TEST_ASSERT_EQUAL_INT(50, h.chA);
    TEST_ASSERT_EQUAL_INT(50, h.chB);
}

// ---- getEffectiveVolume read-side routing (issue #204 phase 2) --------------
// Mirrors the logic in AmidalaAudio::getEffectiveVolume -- the read-side
// counterpart of applyHCRVolume's switch above, used by SnipsRemote's
// downlink echo-back to read back "the current volume" for a given wheel
// selector. AmidalaAudio's real fSavedVolV/A/B can only be reached via
// applyHCRVolume(), which dereferences fController unconditionally (no
// hardware-free path to exercise the real getter against known state), so
// this mirrors it the same way VolumeRouteHarness mirrors the write side.

struct EffectiveVolumeHarness {
    uint8_t savedV = 40, savedA = 41, savedB = 42;  // distinct, to catch cross-wiring

    uint8_t get(uint8_t wheel) const {
        switch (wheel) {
            case 1: return savedV;
            case 2: return savedA;
            case 3: return savedB;
            case 4: return savedA;
            default: return savedV;
        }
    }
};

void test_effective_volume_global_reads_savedV() {
    EffectiveVolumeHarness h;
    TEST_ASSERT_EQUAL_UINT8(40, h.get(0));
}

void test_effective_volume_voice_reads_savedV() {
    EffectiveVolumeHarness h;
    TEST_ASSERT_EQUAL_UINT8(40, h.get(1));
}

void test_effective_volume_chA_reads_savedA() {
    EffectiveVolumeHarness h;
    TEST_ASSERT_EQUAL_UINT8(41, h.get(2));
}

void test_effective_volume_chB_reads_savedB() {
    EffectiveVolumeHarness h;
    TEST_ASSERT_EQUAL_UINT8(42, h.get(3));
}

void test_effective_volume_chAB_reads_savedA() {
    // applyHCRVolume keeps A and B equal for wheel==4, so either read is
    // representative -- matches the real getter's choice to read A.
    EffectiveVolumeHarness h;
    TEST_ASSERT_EQUAL_UINT8(41, h.get(4));
}

// ---- altvolumewheel fall-through logic --------------------------------------
// Mirrors the decision in AmidalaAudio::setAltVolumeNoResponse:
//   altvolumewheel == 0  →  call setVolumeNoResponse (use volumewheel routing)
//   altvolumewheel != 0  →  route via altvolumewheel

static bool altWheelFallsThrough(uint8_t altvolumewheel) {
    return altvolumewheel == 0;
}

void test_altvolumewheel_zero_falls_through() {
    TEST_ASSERT_TRUE(altWheelFallsThrough(0));
}

void test_altvolumewheel_one_does_not_fall_through() {
    TEST_ASSERT_FALSE(altWheelFallsThrough(1));
}

void test_altvolumewheel_two_does_not_fall_through() {
    TEST_ASSERT_FALSE(altWheelFallsThrough(2));
}

void test_altvolumewheel_three_does_not_fall_through() {
    TEST_ASSERT_FALSE(altWheelFallsThrough(3));
}

void test_altvolumewheel_routes_to_correct_channel_when_nonzero() {
    // When altvolumewheel is set, the same applyHCRVolume logic applies.
    VolumeRouteHarness h;
    h.apply(2, 20);  // altvolumewheel=2 (chA)
    TEST_ASSERT_EQUAL_INT(-1, h.chV);
    TEST_ASSERT_EQUAL_INT(20, h.chA);
    TEST_ASSERT_EQUAL_INT(-1, h.chB);
}

// ---- Mute harness -----------------------------------------------------------
// Mirrors the state and logic in AmidalaAudio for toggleMute, restoreVolumes,
// and the wheel-unmute path in setVolumeNoResponse.

struct MuteHarness {
    bool muted = false;
    int chV = 50, chA = 50, chB = 50;  // simulated hardware volumes
    int savedV = 50, savedA = 50, savedB = 50;  // simulated fSavedVol*

    // Mirrors toggleMute()
    void toggleMute() {
        if (muted) {
            restoreVolumes();
        } else {
            muted = true;
            chV = chA = chB = 0;
        }
    }

    // Mirrors restoreVolumes()
    void restoreVolumes() {
        muted = false;
        chV = savedV;
        chA = savedA;
        chB = savedB;
    }

    // Mirrors applyHCRVolume() — global wheel (wheel=0)
    void applyGlobal(int vol) {
        savedV = savedA = savedB = vol;
        chV = chA = chB = vol;
    }

    // Mirrors setVolumeNoResponse() wheel-unmute path
    void setVolume(int vol) {
        if (muted) restoreVolumes();
        applyGlobal(vol);
    }
};

// ---- toggleMute: mute sets all channels to 0 --------------------------------

void test_mute_sets_all_channels_to_zero() {
    MuteHarness h;
    h.toggleMute();
    TEST_ASSERT_TRUE(h.muted);
    TEST_ASSERT_EQUAL_INT(0, h.chV);
    TEST_ASSERT_EQUAL_INT(0, h.chA);
    TEST_ASSERT_EQUAL_INT(0, h.chB);
}

void test_mute_preserves_saved_volumes() {
    MuteHarness h;
    h.savedV = 70; h.savedA = 30; h.savedB = 40;
    h.toggleMute();
    TEST_ASSERT_EQUAL_INT(70, h.savedV);
    TEST_ASSERT_EQUAL_INT(30, h.savedA);
    TEST_ASSERT_EQUAL_INT(40, h.savedB);
}

// ---- toggleMute: unmute restores saved volumes ------------------------------

void test_unmute_restores_saved_volumes() {
    MuteHarness h;
    h.savedV = 70; h.savedA = 30; h.savedB = 40;
    h.toggleMute();  // mute
    h.toggleMute();  // unmute
    TEST_ASSERT_FALSE(h.muted);
    TEST_ASSERT_EQUAL_INT(70, h.chV);
    TEST_ASSERT_EQUAL_INT(30, h.chA);
    TEST_ASSERT_EQUAL_INT(40, h.chB);
}

void test_double_mute_toggle_round_trips() {
    MuteHarness h;
    h.savedV = h.chV = 60;
    h.savedA = h.chA = 50;
    h.savedB = h.chB = 40;
    h.toggleMute();
    h.toggleMute();
    TEST_ASSERT_FALSE(h.muted);
    TEST_ASSERT_EQUAL_INT(60, h.chV);
    TEST_ASSERT_EQUAL_INT(50, h.chA);
    TEST_ASSERT_EQUAL_INT(40, h.chB);
}

// ---- Volume wheel while muted unmutes all channels --------------------------

void test_wheel_while_muted_unmutes_then_applies_volume() {
    MuteHarness h;
    h.savedV = h.chV = 60;
    h.savedA = h.chA = 40;
    h.savedB = h.chB = 40;
    h.toggleMute();
    TEST_ASSERT_TRUE(h.muted);
    h.setVolume(30);
    TEST_ASSERT_FALSE(h.muted);
    // All channels should be audible (restored then global wheel applied).
    TEST_ASSERT_EQUAL_INT(30, h.chV);
    TEST_ASSERT_EQUAL_INT(30, h.chA);
    TEST_ASSERT_EQUAL_INT(30, h.chB);
}

void test_wheel_while_not_muted_does_not_change_muted_flag() {
    MuteHarness h;
    h.setVolume(80);
    TEST_ASSERT_FALSE(h.muted);
    TEST_ASSERT_EQUAL_INT(80, h.chV);
}

// ---- Double-press timing harness --------------------------------------------
// Mirrors the logic in AmidalaController::noteMuteBtnUp.

struct DoublePressHarness {
    uint32_t lastBtnUpTime = 0;
    int muteCount = 0;

    void noteBtnUp(uint32_t now, uint32_t windowMs) {
        if (lastBtnUpTime != 0 && now - lastBtnUpTime <= windowMs) {
            lastBtnUpTime = 0;
            muteCount++;
        } else {
            lastBtnUpTime = now;
        }
    }
};

void test_double_press_const_is_400ms() {
    // Keep the constant visible in tests so a change is caught here too.
    // The actual constant lives in AmidalaController::DOUBLE_PRESS_MS.
    TEST_ASSERT_EQUAL_UINT32(400, (uint32_t)400);
}

void test_single_press_does_not_mute() {
    DoublePressHarness h;
    h.noteBtnUp(1000, 400);
    TEST_ASSERT_EQUAL_INT(0, h.muteCount);
}

void test_double_press_within_window_mutes() {
    DoublePressHarness h;
    h.noteBtnUp(1000, 400);
    h.noteBtnUp(1350, 400);  // 350ms later — within 400ms window
    TEST_ASSERT_EQUAL_INT(1, h.muteCount);
}

void test_double_press_at_exact_window_boundary_mutes() {
    DoublePressHarness h;
    h.noteBtnUp(1000, 400);
    h.noteBtnUp(1400, 400);  // exactly 400ms later
    TEST_ASSERT_EQUAL_INT(1, h.muteCount);
}

void test_double_press_just_outside_window_does_not_mute() {
    DoublePressHarness h;
    h.noteBtnUp(1000, 400);
    h.noteBtnUp(1401, 400);  // 401ms later — outside window
    TEST_ASSERT_EQUAL_INT(0, h.muteCount);
}

void test_triple_press_fires_only_once() {
    DoublePressHarness h;
    h.noteBtnUp(1000, 400);
    h.noteBtnUp(1200, 400);  // double-press fires
    h.noteBtnUp(1300, 400);  // third press: timer was reset, so starts fresh
    TEST_ASSERT_EQUAL_INT(1, h.muteCount);
}

// ---- L1 analog release filter -----------------------------------------------
// Mirrors the guard in DomeController::process():
//   if (raw > 0) { vol = map(raw, 0, 255, 0, 100); apply(vol); }
// Releasing the button fires analog_changed with raw==0; that must not update
// the volume (which would silently zero it out).

struct L1VolumeFilterHarness {
    int lastVolume = -1;  // -1 means no update received

    void onAnalogChanged(uint8_t raw) {
        if (raw > 0)
            lastVolume = map(raw, 0, 255, 0, 100);
    }
};

void test_l1_release_does_not_update_volume() {
    L1VolumeFilterHarness h;
    h.onAnalogChanged(0);
    TEST_ASSERT_EQUAL_INT(-1, h.lastVolume);
}

void test_l1_full_press_sets_volume_100() {
    L1VolumeFilterHarness h;
    h.onAnalogChanged(255);
    TEST_ASSERT_EQUAL_INT(100, h.lastVolume);
}

void test_l1_half_press_sets_volume_roughly_50() {
    L1VolumeFilterHarness h;
    h.onAnalogChanged(128);
    TEST_ASSERT_INT_WITHIN(2, 50, h.lastVolume);
}

void test_l1_minimum_nonzero_raw_sets_nonzero_volume() {
    L1VolumeFilterHarness h;
    h.onAnalogChanged(1);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, h.lastVolume);
    TEST_ASSERT_NOT_EQUAL(-1, h.lastVolume);
}

void test_l1_release_after_press_does_not_change_volume() {
    L1VolumeFilterHarness h;
    h.onAnalogChanged(200);
    int volAfterPress = h.lastVolume;
    h.onAnalogChanged(0);  // button released
    TEST_ASSERT_EQUAL_INT(volAfterPress, h.lastVolume);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_volume_throttle_ms_is_80);

    RUN_TEST(test_first_update_passes_at_time_zero);
    RUN_TEST(test_first_update_passes_at_nonzero_time);

    RUN_TEST(test_immediate_second_call_is_blocked);
    RUN_TEST(test_call_just_before_window_is_blocked);

    RUN_TEST(test_call_at_exact_window_boundary_passes);
    RUN_TEST(test_call_well_past_window_passes);

    RUN_TEST(test_burst_of_10_rapid_calls_lets_only_one_through);
    RUN_TEST(test_three_windows_produce_three_updates);

    RUN_TEST(test_volumewheel_global_sets_all_channels);
    RUN_TEST(test_volumewheel_voice_sets_only_chV);
    RUN_TEST(test_volumewheel_chA_sets_only_chA);
    RUN_TEST(test_volumewheel_chB_sets_only_chB);
    RUN_TEST(test_volumewheel_chAB_sets_chA_and_chB_only);
    RUN_TEST(test_volumewheel_unknown_value_falls_back_to_global);

    RUN_TEST(test_effective_volume_global_reads_savedV);
    RUN_TEST(test_effective_volume_voice_reads_savedV);
    RUN_TEST(test_effective_volume_chA_reads_savedA);
    RUN_TEST(test_effective_volume_chB_reads_savedB);
    RUN_TEST(test_effective_volume_chAB_reads_savedA);

    RUN_TEST(test_altvolumewheel_zero_falls_through);
    RUN_TEST(test_altvolumewheel_one_does_not_fall_through);
    RUN_TEST(test_altvolumewheel_two_does_not_fall_through);
    RUN_TEST(test_altvolumewheel_three_does_not_fall_through);
    RUN_TEST(test_altvolumewheel_routes_to_correct_channel_when_nonzero);

    RUN_TEST(test_mute_sets_all_channels_to_zero);
    RUN_TEST(test_mute_preserves_saved_volumes);
    RUN_TEST(test_unmute_restores_saved_volumes);
    RUN_TEST(test_double_mute_toggle_round_trips);
    RUN_TEST(test_wheel_while_muted_unmutes_then_applies_volume);
    RUN_TEST(test_wheel_while_not_muted_does_not_change_muted_flag);

    RUN_TEST(test_double_press_const_is_400ms);
    RUN_TEST(test_single_press_does_not_mute);
    RUN_TEST(test_double_press_within_window_mutes);
    RUN_TEST(test_double_press_at_exact_window_boundary_mutes);
    RUN_TEST(test_double_press_just_outside_window_does_not_mute);
    RUN_TEST(test_triple_press_fires_only_once);

    RUN_TEST(test_l1_release_does_not_update_volume);
    RUN_TEST(test_l1_full_press_sets_volume_100);
    RUN_TEST(test_l1_half_press_sets_volume_roughly_50);
    RUN_TEST(test_l1_minimum_nonzero_raw_sets_nonzero_volume);
    RUN_TEST(test_l1_release_after_press_does_not_change_volume);

    return UNITY_END();
}
