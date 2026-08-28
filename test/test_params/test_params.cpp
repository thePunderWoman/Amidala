// test_params.cpp
// Tests for include/params.h:
//   AmidalaParameters — default values, EEPROM load, capacity constants

#include "arduino_mock.h"
#include "params.h"
#include <unity.h>
#include <string.h>

// Each test gets a fresh AmidalaParameters to avoid static init() caching.
// We reinitialise the mock EEPROM in setUp.
void setUp(void) {
    // Reset EEPROM mock to all zeros before each test.
    memset(EEPROM.data, 0, sizeof(EEPROM.data));
}
void tearDown(void) {}

// ---- Audio hardware constants -----------------------------------------------

void test_audio_hw_constants() {
    TEST_ASSERT_EQUAL(1, AUDIO_HW_HCR);
    TEST_ASSERT_EQUAL(2, AUDIO_HW_VMUSIC);
}

// ---- Array capacities --------------------------------------------------------

void test_sound_bank_count() {
    AmidalaParameters p;
    TEST_ASSERT_EQUAL(20, p.getSoundBankCount());
}

void test_servo_storage_capacity() {
    // S[]'s fixed storage capacity (kMaxServoChannels), NOT the live
    // pinRole[]-derived count -- see test_default_servo_count_matches_
    // default_servo_pins() for the live-count-on-default-init case.
    AmidalaParameters p;
    TEST_ASSERT_EQUAL(kMaxServoChannels, sizeof(p.S) / sizeof(p.S[0]));
}

void test_button_count() {
    AmidalaParameters p;
    TEST_ASSERT_EQUAL(9, p.getButtonCount());
}

void test_gesture_count() {
    AmidalaParameters p;
    TEST_ASSERT_EQUAL(MAX_GESTURES, p.getGestureCount());
}

void test_serial_string_count() {
    AmidalaParameters p;
    TEST_ASSERT_EQUAL(MAX_SERIAL_STRINGS, (int)p.getSerialStringCount());
}

// ---- Default values after init() (no EEPROM signature) ----------------------
// The static sInited/sRAMInited flags mean we can only test one instance per
// process run — use a single AmidalaParameters here for the default tests.

static AmidalaParameters gDefaultParams;

void test_default_volume() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(50, gDefaultParams.volume);
}

void test_default_startup_true() {
    gDefaultParams.init();
    TEST_ASSERT_TRUE(gDefaultParams.startup);
}

void test_default_rndon_true() {
    gDefaultParams.init();
    TEST_ASSERT_TRUE(gDefaultParams.rndon);
}

void test_default_ackon_false() {
    gDefaultParams.init();
    TEST_ASSERT_FALSE(gDefaultParams.ackon);
}

void test_default_btcontrolleron_false() {
    gDefaultParams.init();
    TEST_ASSERT_FALSE(gDefaultParams.btcontrolleron);
}

void test_default_wcbenable_false() {
    gDefaultParams.init();
    TEST_ASSERT_FALSE(gDefaultParams.wcbenable);
}

void test_default_wcb_identity_zeroed() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(0, gDefaultParams.wcboct2);
    TEST_ASSERT_EQUAL(0, gDefaultParams.wcboct3);
    TEST_ASSERT_EQUAL_STRING("", gDefaultParams.wcbpassword);
    TEST_ASSERT_EQUAL(0, gDefaultParams.wcbquantity);
    TEST_ASSERT_EQUAL(0, gDefaultParams.wcbid);
}

void test_default_outboundserial_uart0() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(0, gDefaultParams.outboundserial);
}

void test_default_wifichannel() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(1, gDefaultParams.wifichannel);
}

void test_default_mindelay() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(60, gDefaultParams.mindelay);
}

// Regression test for issue #185: fst previously had no default and fell
// through to 0 from init()'s memset when no legacy EEPROM "SC23" block was
// present, which broke the failsafe timeout check in
// AmidalaController::animate() (constant XBee connect/disconnect flapping).
void test_default_fst_in_valid_range() {
    gDefaultParams.init();
    TEST_ASSERT_GREATER_OR_EQUAL(1000, gDefaultParams.fst);
    TEST_ASSERT_LESS_OR_EQUAL(3000, gDefaultParams.fst);
}

void test_default_maxdelay() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(120, gDefaultParams.maxdelay);
}

void test_default_serialbaud() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(9600UL, gDefaultParams.serialbaud);
}

void test_default_serialdelim() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(':', gDefaultParams.serialdelim);
}

void test_default_serialeol() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(13, gDefaultParams.serialeol);
}

void test_default_audiohw_is_hcr() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(AUDIO_HW_HCR, gDefaultParams.audiohw);
}

void test_default_dome_home_position() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_HOME_POSITION, gDefaultParams.domehome);
}

void test_default_dome_speed_home() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_SPEED_HOME, gDefaultParams.domespeedhome);
}

// Regression test for issue #196: domespeed (a 0-100 percentage) was
// defaulted from DOME_MAXIMUM_SPEED (a 0.0-1.0 fraction, 1.0f), which
// truncates to 1 when assigned into the uint8_t field -- a fresh board ran
// the dome at 1% speed, silently below driveFromJoystick()'s dead-band for
// any stick input, so the dome never responded to the joystick at all.
void test_default_domespeed_is_100_not_truncated_fraction() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(100, gDefaultParams.domespeed);
}

void test_default_dome_speed_min() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_SPEED_MIN, gDefaultParams.domespeedmin);
}

void test_default_dome_fudge() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_FUDGE, gDefaultParams.domefudge);
}

void test_default_min_pulse() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_MIN_PULSE, gDefaultParams.minpulse);
}

void test_default_max_pulse() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_MAX_PULSE, gDefaultParams.maxpulse);
}

// ---- RoboClaw dome drive defaults -------------------------------------------
// These all hang off gDefaultParams so init() is only called once (sInited).

void test_default_domercaddr() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_ROBOCLAW_ADDRESS, gDefaultParams.domercaddr);
}

void test_default_domercchan() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_ROBOCLAW_CHANNEL, gDefaultParams.domercchan);
}

void test_default_domercqpps() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_ROBOCLAW_QPPS, gDefaultParams.domercqpps);
}

void test_default_domefront() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(0, gDefaultParams.domefront);
}

void test_default_domestall() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(DEFAULT_DOME_STALL_TIMEOUT_MS, gDefaultParams.domestall);
}

// ---- Reassignable GPIO pin-role defaults (issue #133) -----------------------
// Defaults must reproduce today's REAL wiring exactly, in kAssignablePins
// order {1,2,3,4,5,6,39,40,41,42,47}, since upgrading firmware must not
// silently change anyone's existing behavior. Pin 40 is a special case: it
// defaults to Hall (not Dout) whenever RoboClaw dome drive is compiled in,
// since the hall sensor's pinMode(INPUT_PULLUP) always wins the pin-mode
// race there regardless of what DOUT setup claims -- see params.h's init().

void test_default_pin_roles_match_todays_wiring() {
    gDefaultParams.init();
    TEST_ASSERT_TRUE(PinRoleType::kAnalog == gDefaultParams.pinRole[0]);  // 1
    TEST_ASSERT_TRUE(PinRoleType::kAnalog == gDefaultParams.pinRole[1]);  // 2
    TEST_ASSERT_TRUE(PinRoleType::kServo  == gDefaultParams.pinRole[2]);  // 3
    TEST_ASSERT_TRUE(PinRoleType::kServo  == gDefaultParams.pinRole[3]);  // 4
    TEST_ASSERT_TRUE(PinRoleType::kServo  == gDefaultParams.pinRole[4]);  // 5
    TEST_ASSERT_TRUE(PinRoleType::kServo  == gDefaultParams.pinRole[5]);  // 6
    TEST_ASSERT_TRUE(PinRoleType::kDout   == gDefaultParams.pinRole[6]);  // 39
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    TEST_ASSERT_TRUE(PinRoleType::kHall   == gDefaultParams.pinRole[7]);  // 40
#else
    TEST_ASSERT_TRUE(PinRoleType::kDout   == gDefaultParams.pinRole[7]);  // 40
#endif
    TEST_ASSERT_TRUE(PinRoleType::kDout   == gDefaultParams.pinRole[8]);  // 41
    TEST_ASSERT_TRUE(PinRoleType::kDout   == gDefaultParams.pinRole[9]);  // 42
    TEST_ASSERT_TRUE(PinRoleType::kPpm    == gDefaultParams.pinRole[10]); // 47
}

// ---- Reassignable dome/drive serial port defaults (issue #147) -------------
// Defaults must reproduce today's REAL wiring: RoboClaw dome only ever used
// Serial1; everything else that consumes a serial port (Sabertooth dome,
// Sabertooth/Roboteq-serial/Roboteq-PWM-serial drive) only ever used
// Serial2/AUX_SERIAL. driveSerialPort still gets a deterministic default on
// the reference build (RoboClaw dome + Roboteq-PWM drive) even though
// nothing consumes it there -- inert, not uninitialized, same as
// domercaddr etc. being harmless on non-RoboClaw builds.

void test_default_serial_ports_match_todays_wiring() {
    gDefaultParams.init();
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
    TEST_ASSERT_TRUE(SerialPortId::kSerial1 == gDefaultParams.domeSerialPort);
#else
    TEST_ASSERT_TRUE(SerialPortId::kSerial2 == gDefaultParams.domeSerialPort);
#endif
    TEST_ASSERT_TRUE(SerialPortId::kSerial2 == gDefaultParams.driveSerialPort);
}

void test_default_servo_count_matches_default_servo_pins() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(4, gDefaultParams.getServoCount());
}

// ---- sanitizePinRoles() boot-time recovery sweep (issue #133) ---------------
// AmidalaConfig::validatePinAssignments() (config.cpp) calls this after
// config.txt loads, since a line's own conflict check only sees whatever's
// been parsed so far -- these exercise the pure function directly rather
// than through AmidalaConfig, which can't be constructed natively (it needs
// a real AmidalaController).

void test_sanitize_pin_roles_leaves_valid_defaults_unchanged() {
    PinRoleType roles[11];
    defaultPinRoles(roles);
    PinRoleType before[11];
    memcpy(before, roles, sizeof(before));
    sanitizePinRoles(roles, true);
    TEST_ASSERT_EQUAL_MEMORY(before, roles, sizeof(roles));
}

void test_sanitize_pin_roles_reverts_electrically_invalid_role() {
    // GPIO39 (index 6) isn't ADC1-capable -- simulate a corrupted/pre-#133
    // config state where it's somehow Analog anyway.
    PinRoleType roles[11];
    defaultPinRoles(roles);
    roles[6] = PinRoleType::kAnalog;
    sanitizePinRoles(roles, true);
    PinRoleType defaults[11];
    defaultPinRoles(defaults);
    TEST_ASSERT_TRUE(defaults[6] == roles[6]);
}

void test_sanitize_pin_roles_forces_hall_when_required_and_missing() {
    PinRoleType roles[11];
    defaultPinRoles(roles);
    roles[7] = PinRoleType::kDout;  // GPIO40 traded away, nothing else is Hall
    sanitizePinRoles(roles, true);
    TEST_ASSERT_TRUE(PinRoleType::kHall == roles[7]);
}

void test_sanitize_pin_roles_leaves_hall_missing_when_not_required() {
    PinRoleType roles[11];
    defaultPinRoles(roles);
    roles[7] = PinRoleType::kDout;
    sanitizePinRoles(roles, false);
    TEST_ASSERT_TRUE(PinRoleType::kDout == roles[7]);  // not forced back
}

void test_sanitize_pin_roles_does_not_force_hall_when_already_present_elsewhere() {
    // Hall already lives on a different pin (physically rewired) --
    // sanitizePinRoles() must not also stomp GPIO40 back to Hall.
    PinRoleType roles[11];
    defaultPinRoles(roles);
    roles[7] = PinRoleType::kDout;   // GPIO40 no longer Hall
    roles[8] = PinRoleType::kHall;   // GPIO41 is Hall instead
    sanitizePinRoles(roles, true);
    TEST_ASSERT_TRUE(PinRoleType::kDout == roles[7]);
    TEST_ASSERT_TRUE(PinRoleType::kHall == roles[8]);
}

void test_roboclaw_params_are_distinct_addresses() {
    // Guard against any future copy-paste that aliases one field to another.
    AmidalaParameters p;
    TEST_ASSERT_NOT_EQUAL((void*)&p.domercaddr, (void*)&p.domercchan);
    TEST_ASSERT_NOT_EQUAL((void*)&p.domercaddr, (void*)&p.domercqpps);
    TEST_ASSERT_NOT_EQUAL((void*)&p.domercaddr, (void*)&p.domefront);
    TEST_ASSERT_NOT_EQUAL((void*)&p.domercaddr, (void*)&p.domestall);
    TEST_ASSERT_NOT_EQUAL((void*)&p.domercqpps, (void*)&p.domefront);
    TEST_ASSERT_NOT_EQUAL((void*)&p.domefront,  (void*)&p.domestall);
}

// ---- Alt-button defaults -------------------------------------------------------

void test_default_altbtn_is_zero() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(0, gDefaultParams.altbtn);
}

void test_default_altdomestick_is_zero() {
    gDefaultParams.init();
    TEST_ASSERT_EQUAL(0, gDefaultParams.altdomestick);
}

void test_altbtn_and_altdomestick_are_distinct_addresses() {
    AmidalaParameters p;
    TEST_ASSERT_NOT_EQUAL((void*)&p.altbtn,       (void*)&p.altdomestick);
    // Must not alias any existing dome field.
    TEST_ASSERT_NOT_EQUAL((void*)&p.altbtn,       (void*)&p.domefront);
    TEST_ASSERT_NOT_EQUAL((void*)&p.altbtn,       (void*)&p.domestall);
    TEST_ASSERT_NOT_EQUAL((void*)&p.altdomestick, (void*)&p.domefront);
    TEST_ASSERT_NOT_EQUAL((void*)&p.altdomestick, (void*)&p.domestall);
}

// ---- D[] / Str[] layout: no overlap -----------------------------------------
// Regression: D[] was declared D[4] but setDigitalPin/getDigitalPin access
// indices 0..7 (pins 1..8).  D[4..7] overflowed into Str[0], zeroing
// Str[0].name[0] and making the first serial string name appear empty in the UI.

void test_dout_array_does_not_overlap_str() {
    AmidalaParameters p;
    char* d_end   = (char*)&p.D[0] + sizeof(p.D);     // first byte past D[]
    char* str_beg = (char*)&p.Str[0];                  // first byte of Str[]
    // D[] must end at or before Str[] begins — no overlap allowed.
    TEST_ASSERT_TRUE_MESSAGE(d_end <= str_beg,
        "D[] overlaps Str[0]: D[4..7] would corrupt Str[0].name");
}

void test_dout_all_eleven_pins_fit() {
    // Issue #133: DOUT count is no longer fixed at 4 (or 8) -- up to all 11
    // pool pins could be DOUT-typed. Verify the array is large enough so no
    // out-of-bounds write occurs at the new ceiling.
    AmidalaParameters p;
    TEST_ASSERT_EQUAL_MESSAGE(11, (int)(sizeof(p.D) / sizeof(p.D[0])),
        "D[] must hold 11 entries -- the full assignable-pool size");
}

void test_dout_write_does_not_corrupt_sstr_name() {
    // Simulate what setup() does: write false to D[4..7] (pins 5..8).
    // Before the fix this zeroed Str[0].name[0], making the first serial
    // string appear unnamed in the web UI on every boot.
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    strncpy(p.Str[0].name, "Sad (Moderate)", sizeof(p.Str[0].name) - 1);
    p.Str[0].name[sizeof(p.Str[0].name) - 1] = '\0';

    // Write false to D[4..7] as setDigitalPin(5..8, false) does
    p.D[4].state = false;
    p.D[5].state = false;
    p.D[6].state = false;
    p.D[7].state = false;

    TEST_ASSERT_EQUAL_STRING("Sad (Moderate)", p.Str[0].name);
}

// ---- EEPROM serial number load (DB01 signature) -----------------------------

void test_eeprom_serial_loaded_when_db01_signature_present() {
    // Write 'D','B','0','1',0 signature then serial "R2D2"
    EEPROM.data[0] = 'D'; EEPROM.data[1] = 'B';
    EEPROM.data[2] = '0'; EEPROM.data[3] = '1'; EEPROM.data[4] = 0;
    EEPROM.data[5] = 'R'; EEPROM.data[6] = '2';
    EEPROM.data[7] = 'D'; EEPROM.data[8] = '2'; EEPROM.data[9] = 0;

    // sInited is already set by the default-value tests above; use forceReload
    // and zero the instance first so unloaded fields start clean.
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    p.init(true);
    TEST_ASSERT_EQUAL('R', p.serial[0]);
    TEST_ASSERT_EQUAL('2', p.serial[1]);
    TEST_ASSERT_EQUAL('D', p.serial[2]);
    TEST_ASSERT_EQUAL('2', p.serial[3]);
}

void test_eeprom_serial_not_loaded_when_no_signature() {
    // All zeros — no DB01 signature.
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    p.init(true);
    TEST_ASSERT_EQUAL(0, p.serial[0]);
}

// ---- EEPROM XBee address load (SC23 signature) ------------------------------

void test_eeprom_xbr_loaded_when_sc23_signature_present() {
    // SC23 signature at offset 0x64
    int base = 0x64;
    EEPROM.data[base + 0] = 'S'; EEPROM.data[base + 1] = 'C';
    EEPROM.data[base + 2] = '2'; EEPROM.data[base + 3] = '3';
    EEPROM.data[base + 4] = 0;
    // xbr little-endian at base+5: 0x01020304
    EEPROM.data[base + 5] = 0x04;  // byte 0 (LSB)
    EEPROM.data[base + 6] = 0x03;
    EEPROM.data[base + 7] = 0x02;
    EEPROM.data[base + 8] = 0x01;  // byte 3 (MSB)
    // xbl little-endian at base+9: 0x05060708
    EEPROM.data[base + 9]  = 0x08;
    EEPROM.data[base + 10] = 0x07;
    EEPROM.data[base + 11] = 0x06;
    EEPROM.data[base + 12] = 0x05;

    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    p.init(true);
    TEST_ASSERT_EQUAL_HEX32(0x01020304, p.xbr);
    TEST_ASSERT_EQUAL_HEX32(0x05060708, p.xbl);
}

// Regression coverage for issue #185's field report: a device that hit a
// restart mid-EEPROM-commit ended up with corrupted bytes after an intact
// "SC23" signature -- the signature only proves the block was intentionally
// written at some point, not that every byte in it is still what was
// written. Before this, those bytes were trusted straight into live params
// with no validation at all. Uses 0xFF fill (the erased-flash pattern) as
// the corruption, since that's the realistic failure mode -- clamping
// should bring every field back into the same range its own cfg_*() setter
// in config.cpp already enforces for a live edit.
void test_eeprom_sc23_corrupted_fields_clamped_to_valid_ranges() {
    int base = 0x64;
    EEPROM.data[base + 0] = 'S'; EEPROM.data[base + 1] = 'C';
    EEPROM.data[base + 2] = '2'; EEPROM.data[base + 3] = '3';
    EEPROM.data[base + 4] = 0;
    // xbr/xbl (base+5..base+12): opaque hardware addresses, no valid range
    // to clamp -- left as 0xFF fill deliberately, not under test here.
    for (int i = 5; i <= 12; i++) EEPROM.data[base + i] = 0xFF;
    EEPROM.data[base + 13] = 0xFF;               // rcchn      (valid: 6-8)
    EEPROM.data[base + 14] = 0xFF;               // unknown
    EEPROM.data[base + 15] = 0xFF; EEPROM.data[base + 16] = 0xFF;  // minpulse (valid: 0-2500)
    EEPROM.data[base + 17] = 0xFF; EEPROM.data[base + 18] = 0xFF;  // maxpulse (valid: 0-2500)
    for (int i = 19; i <= 23; i++) EEPROM.data[base + i] = 0xFF;   // unknown x5
    EEPROM.data[base + 24] = 0xFF; EEPROM.data[base + 25] = 0xFF;  // rvrmin (valid: 0-100)
    EEPROM.data[base + 26] = 0xFF; EEPROM.data[base + 27] = 0xFF;  // rvlmin (valid: 0-100)
    EEPROM.data[base + 28] = 0xFF; EEPROM.data[base + 29] = 0xFF;  // rvrmax (valid: 900-1023)
    EEPROM.data[base + 30] = 0xFF; EEPROM.data[base + 31] = 0xFF;  // rvlmax (valid: 900-1023)
    EEPROM.data[base + 32] = 0xFF; EEPROM.data[base + 33] = 0xFF;  // fst    (valid: 1000-3000)

    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    p.init(true);

    TEST_ASSERT_EQUAL(8, p.rcchn);
    TEST_ASSERT_EQUAL(2500, p.minpulse);
    TEST_ASSERT_EQUAL(2500, p.maxpulse);
    TEST_ASSERT_EQUAL(100, p.rvrmin);
    TEST_ASSERT_EQUAL(100, p.rvlmin);
    TEST_ASSERT_EQUAL(1023, p.rvrmax);
    TEST_ASSERT_EQUAL(1023, p.rvlmax);
    TEST_ASSERT_EQUAL(3000, p.fst);
}

// Same corruption class, but clamped from below instead of above -- a
// stuck-at-zero sector (rather than erased/0xFF) is just as plausible a
// failure mode, and the fix needs to hold in both directions.
void test_eeprom_sc23_zeroed_fields_clamped_up_to_valid_ranges() {
    int base = 0x64;
    EEPROM.data[base + 0] = 'S'; EEPROM.data[base + 1] = 'C';
    EEPROM.data[base + 2] = '2'; EEPROM.data[base + 3] = '3';
    EEPROM.data[base + 4] = 0;
    // Everything from base+5 onward is already 0 from setUp()'s memset --
    // rcchn/minpulse/maxpulse/rvrmin/rvlmin all have a valid 0 or near-0
    // floor, so only rvrmax/rvlmax/fst (whose valid ranges don't include 0)
    // actually exercise the low-side clamp.

    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    p.init(true);

    TEST_ASSERT_EQUAL(900, p.rvrmax);
    TEST_ASSERT_EQUAL(900, p.rvlmax);
    TEST_ASSERT_EQUAL(1000, p.fst);
}

// ---- getRadioChannelCount lazy-init -----------------------------------------

void test_get_radio_channel_count_returns_default_zero_when_eeprom_absent() {
    // Zero the instance explicitly — sInited is already set so init() will
    // return early; rcchn must come from our memset, not from defaults.
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    TEST_ASSERT_EQUAL(0, p.getRadioChannelCount());
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_audio_hw_constants);

    RUN_TEST(test_sound_bank_count);
    RUN_TEST(test_servo_storage_capacity);
    RUN_TEST(test_button_count);
    RUN_TEST(test_gesture_count);
    RUN_TEST(test_serial_string_count);

    RUN_TEST(test_default_volume);
    RUN_TEST(test_default_startup_true);
    RUN_TEST(test_default_rndon_true);
    RUN_TEST(test_default_ackon_false);
    RUN_TEST(test_default_btcontrolleron_false);
    RUN_TEST(test_default_wcbenable_false);
    RUN_TEST(test_default_wcb_identity_zeroed);
    RUN_TEST(test_default_outboundserial_uart0);
    RUN_TEST(test_default_wifichannel);
    RUN_TEST(test_default_mindelay);
    RUN_TEST(test_default_fst_in_valid_range);
    RUN_TEST(test_eeprom_sc23_corrupted_fields_clamped_to_valid_ranges);
    RUN_TEST(test_eeprom_sc23_zeroed_fields_clamped_up_to_valid_ranges);
    RUN_TEST(test_default_maxdelay);
    RUN_TEST(test_default_serialbaud);
    RUN_TEST(test_default_serialdelim);
    RUN_TEST(test_default_serialeol);
    RUN_TEST(test_default_audiohw_is_hcr);
    RUN_TEST(test_default_dome_home_position);
    RUN_TEST(test_default_dome_speed_home);
    RUN_TEST(test_default_domespeed_is_100_not_truncated_fraction);
    RUN_TEST(test_default_dome_speed_min);
    RUN_TEST(test_default_dome_fudge);
    RUN_TEST(test_default_min_pulse);
    RUN_TEST(test_default_max_pulse);

    RUN_TEST(test_default_domercaddr);
    RUN_TEST(test_default_domercchan);
    RUN_TEST(test_default_domercqpps);
    RUN_TEST(test_default_domefront);
    RUN_TEST(test_default_domestall);
    RUN_TEST(test_default_pin_roles_match_todays_wiring);
    RUN_TEST(test_default_serial_ports_match_todays_wiring);
    RUN_TEST(test_default_servo_count_matches_default_servo_pins);
    RUN_TEST(test_sanitize_pin_roles_leaves_valid_defaults_unchanged);
    RUN_TEST(test_sanitize_pin_roles_reverts_electrically_invalid_role);
    RUN_TEST(test_sanitize_pin_roles_forces_hall_when_required_and_missing);
    RUN_TEST(test_sanitize_pin_roles_leaves_hall_missing_when_not_required);
    RUN_TEST(test_sanitize_pin_roles_does_not_force_hall_when_already_present_elsewhere);
    RUN_TEST(test_roboclaw_params_are_distinct_addresses);

    RUN_TEST(test_default_altbtn_is_zero);
    RUN_TEST(test_default_altdomestick_is_zero);
    RUN_TEST(test_altbtn_and_altdomestick_are_distinct_addresses);

    RUN_TEST(test_dout_array_does_not_overlap_str);
    RUN_TEST(test_dout_all_eleven_pins_fit);
    RUN_TEST(test_dout_write_does_not_corrupt_sstr_name);

    RUN_TEST(test_eeprom_serial_loaded_when_db01_signature_present);
    RUN_TEST(test_eeprom_serial_not_loaded_when_no_signature);
    RUN_TEST(test_eeprom_xbr_loaded_when_sc23_signature_present);

    RUN_TEST(test_get_radio_channel_count_returns_default_zero_when_eeprom_absent);

    return UNITY_END();
}
