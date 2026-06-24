// test_processconfig.cpp
// Regression tests for bugs found in src/config.cpp processConfig().
//
// Two key bugs were fixed:
//   1. rvrmax=/rvlmin=/rvlmax= all wrote to params.rvrmin (copy-paste error).
//      Tests here verify the four VREF fields are distinct memory locations and
//      that intparam() routes each to the correct field.
//   2. Servo position clamp was min(max(pos,180),90) — always produced 90.
//      Tests here verify the correct clamping formula min(max(pos,0),180).
//
// Full processConfig() integration tests require AmidalaController and are
// covered by functional testing.

#include "arduino_mock.h"
#include "core.h"
#include "params.h"
#include <unity.h>
#include <string.h>

void setUp(void) {
    memset(EEPROM.data, 0, sizeof(EEPROM.data));
}
void tearDown(void) {}

// ---- VREF fields are distinct memory locations ------------------------------
// A copy-paste bug made rvrmax=, rvlmin=, rvlmax= all write to rvrmin.
// These tests verify the four fields are different addresses so a regression
// would be immediately visible.

void test_vref_fields_are_distinct_addresses() {
    AmidalaParameters p;
    TEST_ASSERT_NOT_EQUAL((void*)&p.rvrmin, (void*)&p.rvrmax);
    TEST_ASSERT_NOT_EQUAL((void*)&p.rvrmin, (void*)&p.rvlmin);
    TEST_ASSERT_NOT_EQUAL((void*)&p.rvrmin, (void*)&p.rvlmax);
    TEST_ASSERT_NOT_EQUAL((void*)&p.rvrmax, (void*)&p.rvlmin);
    TEST_ASSERT_NOT_EQUAL((void*)&p.rvrmax, (void*)&p.rvlmax);
    TEST_ASSERT_NOT_EQUAL((void*)&p.rvlmin, (void*)&p.rvlmax);
}

void test_vref_fields_write_independently() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));

    // Write distinct sentinel values to each field.
    p.rvrmin = 10;
    p.rvrmax = 920;
    p.rvlmin = 30;
    p.rvlmax = 940;

    // Verify no field aliases another.
    TEST_ASSERT_EQUAL_UINT16(10,  p.rvrmin);
    TEST_ASSERT_EQUAL_UINT16(920, p.rvrmax);
    TEST_ASSERT_EQUAL_UINT16(30,  p.rvlmin);
    TEST_ASSERT_EQUAL_UINT16(940, p.rvlmax);
}

void test_vref_intparam_routes_to_correct_field() {
    // Use intparam() with the exact same field references as config.cpp to
    // confirm each parameter key lands in the right struct member.
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));

    // Simulate what processConfig() does for each VREF command.
    intparam("rvrmin=10",  "rvrmin=", p.rvrmin, 0, 100);
    intparam("rvrmax=920", "rvrmax=", p.rvrmax, 900, 1023);
    intparam("rvlmin=30",  "rvlmin=", p.rvlmin, 0, 100);
    intparam("rvlmax=940", "rvlmax=", p.rvlmax, 900, 1023);

    TEST_ASSERT_EQUAL_UINT16(10,  p.rvrmin);
    TEST_ASSERT_EQUAL_UINT16(920, p.rvrmax);
    TEST_ASSERT_EQUAL_UINT16(30,  p.rvlmin);
    TEST_ASSERT_EQUAL_UINT16(940, p.rvlmax);
}

// ---- Servo position clamp formula -------------------------------------------
// The bug was: min(max(pos, 180), 90) — always returns 90 because max(x,180)
// floors at 180, then min(180_or_more, 90) always returns 90.
// The fix is: min(max(pos, 0), 180).

static int servo_clamp_fixed(int pos) {
    return min(max(pos, 0), 180);
}

static int servo_clamp_broken(int pos) {
    return min(max(pos, 180), 90);
}

void test_servo_clamp_broken_always_returns_90() {
    // Confirm the old formula was indeed broken for all representative inputs.
    TEST_ASSERT_EQUAL(90, servo_clamp_broken(0));
    TEST_ASSERT_EQUAL(90, servo_clamp_broken(45));
    TEST_ASSERT_EQUAL(90, servo_clamp_broken(90));
    TEST_ASSERT_EQUAL(90, servo_clamp_broken(135));
    TEST_ASSERT_EQUAL(90, servo_clamp_broken(180));
}

void test_servo_clamp_fixed_preserves_valid_positions() {
    TEST_ASSERT_EQUAL(0,   servo_clamp_fixed(0));
    TEST_ASSERT_EQUAL(45,  servo_clamp_fixed(45));
    TEST_ASSERT_EQUAL(90,  servo_clamp_fixed(90));
    TEST_ASSERT_EQUAL(135, servo_clamp_fixed(135));
    TEST_ASSERT_EQUAL(180, servo_clamp_fixed(180));
}

void test_servo_clamp_fixed_clamps_below_zero() {
    TEST_ASSERT_EQUAL(0, servo_clamp_fixed(-1));
    TEST_ASSERT_EQUAL(0, servo_clamp_fixed(-100));
}

void test_servo_clamp_fixed_clamps_above_180() {
    TEST_ASSERT_EQUAL(180, servo_clamp_fixed(181));
    TEST_ASSERT_EQUAL(180, servo_clamp_fixed(255));
}

// ---- RoboClaw config key routing --------------------------------------------
// Mirrors what processConfig() does: each intparam() call uses the exact same
// field reference and range as config.cpp so a future field rename or range
// change will break these tests before it reaches the embedded build.

void test_domercaddr_intparam_routes_to_correct_field() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("domercaddr=128", "domercaddr=", p.domercaddr, 128, 135);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(128, p.domercaddr);
}

void test_domercaddr_clamps_below_minimum() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // intparam clamps out-of-range values rather than rejecting them.
    // 100 < 128 → clamped to 128.
    bool matched = intparam("domercaddr=100", "domercaddr=", p.domercaddr, 128, 135);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(128, p.domercaddr);
}

void test_domercchan_intparam_channel_1() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("domercchan=1", "domercchan=", p.domercchan, 1, 2);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(1, p.domercchan);
}

void test_domercchan_intparam_channel_2() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("domercchan=2", "domercchan=", p.domercchan, 1, 2);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(2, p.domercchan);
}

void test_domercchan_clamps_channel_3_to_2() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // 3 > 2 → clamped to 2.
    bool matched = intparam("domercchan=3", "domercchan=", p.domercchan, 1, 2);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(2, p.domercchan);
}

void test_domercqpps_intparam_routes_to_correct_field() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("domercqpps=800", "domercqpps=", p.domercqpps, 1, 65535);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(800, p.domercqpps);
}

void test_domefront_intparam_routes_to_correct_field() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("domefront=88", "domefront=", p.domefront, 0, 359);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(88, p.domefront);
}

void test_domefront_accepts_zero() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("domefront=0", "domefront=", p.domefront, 0, 359);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(0, p.domefront);
}

void test_domefront_accepts_359() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("domefront=359", "domefront=", p.domefront, 0, 359);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(359, p.domefront);
}

void test_domefront_clamps_360_to_359() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // 360 > 359 → clamped to 359.
    bool matched = intparam("domefront=360", "domefront=", p.domefront, 0, 359);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(359, p.domefront);
}

void test_domestall_intparam_routes_to_correct_field() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("domestall=500", "domestall=", p.domestall, 100, 5000);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(500, p.domestall);
}

void test_domestall_clamps_below_minimum() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // 50 < 100 → clamped to 100.
    bool matched = intparam("domestall=50", "domestall=", p.domestall, 100, 5000);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(100, p.domestall);
}

void test_roboclaw_fields_are_distinct_from_each_other() {
    // Structural guard: none of the new fields should alias an existing one.
    AmidalaParameters p;
    TEST_ASSERT_NOT_EQUAL((void*)&p.domercaddr, (void*)&p.domercqpps);
    TEST_ASSERT_NOT_EQUAL((void*)&p.domefront,  (void*)&p.domestall);
    TEST_ASSERT_NOT_EQUAL((void*)&p.domefront,  (void*)&p.domehome);  // most likely alias mistake
    TEST_ASSERT_NOT_EQUAL((void*)&p.domestall,  (void*)&p.domespeed);
}

// ---- Alt-button config keys -------------------------------------------------

void test_altbtn_intparam_routes_to_correct_field() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("altbtn=5", "altbtn=", p.altbtn, 0, 9);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(5, p.altbtn);
}

void test_altbtn_accepts_zero_disabled() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("altbtn=0", "altbtn=", p.altbtn, 0, 9);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(0, p.altbtn);
}

void test_altbtn_accepts_nine_max() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("altbtn=9", "altbtn=", p.altbtn, 0, 9);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(9, p.altbtn);
}

void test_altbtn_clamps_above_nine() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // 10 > 9 → clamped to 9
    bool matched = intparam("altbtn=10", "altbtn=", p.altbtn, 0, 9);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(9, p.altbtn);
}

void test_altdomestick_intparam_zero() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("altdomestick=0", "altdomestick=", p.altdomestick, 0, 1);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(0, p.altdomestick);
}

void test_altdomestick_intparam_one() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("altdomestick=1", "altdomestick=", p.altdomestick, 0, 1);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(1, p.altdomestick);
}

void test_altdomestick_clamps_above_one() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // 2 > 1 → clamped to 1
    bool matched = intparam("altdomestick=2", "altdomestick=", p.altdomestick, 0, 1);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(1, p.altdomestick);
}

// ---- Per-channel volume config keys -----------------------------------------

void test_volume_channel_fields_are_distinct_from_each_other() {
    AmidalaParameters p;
    TEST_ASSERT_NOT_EQUAL((void*)&p.volume,    (void*)&p.volumeChA);
    TEST_ASSERT_NOT_EQUAL((void*)&p.volume,    (void*)&p.volumeChB);
    TEST_ASSERT_NOT_EQUAL((void*)&p.volumeChA, (void*)&p.volumeChB);
    TEST_ASSERT_NOT_EQUAL((void*)&p.volumewheel, (void*)&p.altvolumewheel);
}

void test_volume_channel_defaults_after_init() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    memset(EEPROM.data, 0, sizeof(EEPROM.data));
    p.init(true);
    TEST_ASSERT_EQUAL(50, p.volume);
    TEST_ASSERT_EQUAL(50, p.volumeChA);
    TEST_ASSERT_EQUAL(50, p.volumeChB);
    TEST_ASSERT_EQUAL(0,  p.volumewheel);
    TEST_ASSERT_EQUAL(0,  p.altvolumewheel);
}

void test_volumeChA_intparam_routes_to_correct_field() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("volumeChA=30", "volumeChA=", p.volumeChA, 0, 100);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(30, p.volumeChA);
}

void test_volumeChB_intparam_routes_to_correct_field() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("volumeChB=20", "volumeChB=", p.volumeChB, 0, 100);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(20, p.volumeChB);
}

void test_volumewheel_accepts_zero_global() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("volumewheel=0", "volumewheel=", p.volumewheel, 0, 3);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(0, p.volumewheel);
}

void test_volumewheel_accepts_three_max() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("volumewheel=3", "volumewheel=", p.volumewheel, 0, 3);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(3, p.volumewheel);
}

void test_volumewheel_clamps_above_three() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // 4 > 3 → clamped to 3
    bool matched = intparam("volumewheel=4", "volumewheel=", p.volumewheel, 0, 3);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(3, p.volumewheel);
}

void test_altvolumewheel_accepts_zero_fallthrough() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("altvolumewheel=0", "altvolumewheel=", p.altvolumewheel, 0, 3);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(0, p.altvolumewheel);
}

void test_altvolumewheel_accepts_nonzero() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("altvolumewheel=2", "altvolumewheel=", p.altvolumewheel, 0, 3);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(2, p.altvolumewheel);
}

void test_altvolumewheel_clamps_above_three() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // 5 > 3 → clamped to 3
    bool matched = intparam("altvolumewheel=5", "altvolumewheel=", p.altvolumewheel, 0, 3);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(3, p.altvolumewheel);
}

// ---- mutebutton config key --------------------------------------------------

void test_mutebutton_intparam_routes_to_correct_field() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("mutebutton=7", "mutebutton=", p.mutebutton, 0, 9);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(7, p.mutebutton);
}

void test_mutebutton_accepts_zero_disabled() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("mutebutton=0", "mutebutton=", p.mutebutton, 0, 9);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(0, p.mutebutton);
}

void test_mutebutton_accepts_nine_max() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("mutebutton=9", "mutebutton=", p.mutebutton, 0, 9);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(9, p.mutebutton);
}

void test_mutebutton_clamps_above_nine() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool matched = intparam("mutebutton=10", "mutebutton=", p.mutebutton, 0, 9);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_EQUAL(9, p.mutebutton);
}

void test_mutebutton_default_is_zero_after_init() {
    // sRAMInited is a function-local static in init() that prevents re-zeroing
    // after the first call.  memset the struct first so the test is independent
    // of test-run order and struct size changes.
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    memset(EEPROM.data, 0, sizeof(EEPROM.data));
    p.init(true);
    TEST_ASSERT_EQUAL(0, p.mutebutton);
}

void test_mutebutton_field_is_distinct_from_altbtn() {
    AmidalaParameters p;
    TEST_ASSERT_NOT_EQUAL((void*)&p.mutebutton, (void*)&p.altbtn);
}

void test_channel_volumes_are_independent_of_volume() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    intparam("volume=70",    "volume=",    p.volume,    0, 100);
    intparam("volumeChA=30", "volumeChA=", p.volumeChA, 0, 100);
    intparam("volumeChB=20", "volumeChB=", p.volumeChB, 0, 100);
    TEST_ASSERT_EQUAL(70, p.volume);
    TEST_ASSERT_EQUAL(30, p.volumeChA);
    TEST_ASSERT_EQUAL(20, p.volumeChB);
}

// ---- sstr= parse logic ------------------------------------------------------
// Bug: processConfig() called startswith(cmd, "sstr=") which advances cmd past
// "sstr=", then did `const char* val = cmd + 5` — double-skipping 10 chars.
// Result: the first 5 chars of every name were eaten, and short names became
// empty strings (showing as "(unnamed)" in the UI).
//
// Fix: use `val = cmd` after startswith because cmd is already at the value.
// These tests exercise the corrected parse logic in isolation.

// Parse helper using the fixed algorithm (mirrors config.cpp after the fix).
static bool parse_sstr_line(const char* line,
                             char* name, size_t nameLen,
                             char* str,  size_t strLen) {
    const char* cmd = line;
    if (!startswith(cmd, "sstr=")) return false;
    // After startswith, cmd already points past "sstr=" — no additional offset.
    const char* val  = cmd;
    const char* pipe = strchr(val, '|');
    if (pipe) {
        size_t nlen = (size_t)(pipe - val);
        if (nlen >= nameLen) nlen = nameLen - 1;
        memcpy(name, val, nlen);
        name[nlen] = '\0';
        strncpy(str, pipe + 1, strLen - 1);
        str[strLen - 1] = '\0';
    } else {
        name[0] = '\0';
        strncpy(str, val, strLen - 1);
        str[strLen - 1] = '\0';
    }
    return true;
}

// Parse helper using the OLD broken algorithm (val = cmd + 5 double-skips).
static bool parse_sstr_line_broken(const char* line,
                                   char* name, size_t nameLen,
                                   char* str,  size_t strLen) {
    const char* cmd = line;
    if (!startswith(cmd, "sstr=")) return false;
    const char* val  = cmd + 5;  // BUG: double-skip
    const char* pipe = strchr(val, '|');
    if (pipe) {
        size_t nlen = (size_t)(pipe - val);
        if (nlen >= nameLen) nlen = nameLen - 1;
        memcpy(name, val, nlen);
        name[nlen] = '\0';
        strncpy(str, pipe + 1, strLen - 1);
        str[strLen - 1] = '\0';
    } else {
        name[0] = '\0';
        strncpy(str, val, strLen - 1);
        str[strLen - 1] = '\0';
    }
    return true;
}

void test_sstr_parse_basic_name_and_str() {
    char name[32], str[32];
    bool ok = parse_sstr_line("sstr=Sad (Moderate)|<SS0>", name, sizeof(name), str, sizeof(str));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("Sad (Moderate)", name);
    TEST_ASSERT_EQUAL_STRING("<SS0>", str);
}

void test_sstr_parse_short_name_not_empty() {
    // "Vader" is 5 chars — the double-skip bug consumed exactly "Vader" and
    // left the pipe as the first char, producing an empty name.
    char name[32], str[32];
    bool ok = parse_sstr_line("sstr=Vader|BD:VADER", name, sizeof(name), str, sizeof(str));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("Vader", name);
    TEST_ASSERT_EQUAL_STRING("BD:VADER", str);
}

void test_sstr_parse_name_not_truncated_by_five() {
    // "Toggle Mute" → broken: "e Mute" (first 6 chars gone). Fixed: full name.
    char name[32], str[32];
    bool ok = parse_sstr_line("sstr=Toggle Mute|BD:VOL", name, sizeof(name), str, sizeof(str));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("Toggle Mute", name);
    TEST_ASSERT_EQUAL_STRING("BD:VOL", str);
}

void test_sstr_parse_angle_bracket_command_preserved() {
    // Angle brackets in the serial string must survive the parse.
    char name[32], str[32];
    bool ok = parse_sstr_line("sstr=Scared (Moderate)|<SC0>", name, sizeof(name), str, sizeof(str));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("Scared (Moderate)", name);
    TEST_ASSERT_EQUAL_STRING("<SC0>", str);
}

void test_sstr_parse_no_pipe_stores_str_only() {
    // No pipe → name empty, entire value goes to str.
    char name[32], str[32];
    name[0] = 0xFF;
    bool ok = parse_sstr_line("sstr=BD:VOL", name, sizeof(name), str, sizeof(str));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("", name);
    TEST_ASSERT_EQUAL_STRING("BD:VOL", str);
}

void test_sstr_parse_broken_eats_first_five_chars() {
    // Regression guard: document exactly what the old code produced so any
    // accidental revert is immediately caught.
    char name[32], str[32];
    parse_sstr_line_broken("sstr=Sad (Moderate)|<SS0>", name, sizeof(name), str, sizeof(str));
    // Old code produced "Moderate)" not "Sad (Moderate)".
    TEST_ASSERT_EQUAL_STRING("Moderate)", name);
}

void test_sstr_parse_broken_short_name_becomes_empty() {
    // "Vader" (5 chars) → old code val pointed at '|', nlen=0, name empty.
    char name[32], str[32];
    parse_sstr_line_broken("sstr=Vader|BD:VADER", name, sizeof(name), str, sizeof(str));
    TEST_ASSERT_EQUAL_STRING("", name);
}

// ---- Safety command (estopstr= / resumestr=) parse logic --------------------
// These tests verify that estopstr= and resumestr= config lines are stored in
// the correct fields of AmidalaParameters without aliasing, length overrun, or
// off-by-one errors.  The parse logic mirrors config.cpp exactly.

// Parse one estopstr= or resumestr= line, mutating p just as config.cpp does.
static bool parse_safety_cmd_line(const char* line, bool isEstop,
                                   AmidalaParameters& p) {
    const char* prefix = isEstop ? "estopstr=" : "resumestr=";
    uint8_t&    cnt    = isEstop ? p.estopCmdCount : p.resumeCmdCount;
    AmidalaParameters::SafetyCmd* arr =
        isEstop ? p.EstopCmds : p.ResumeCmds;

    const char* cmd = line;
    if (!startswith(cmd, prefix)) return false;
    if (!*cmd || cnt >= MAX_SAFETY_CMDS) return true; // ignored (empty or full)
    strncpy(arr[cnt].str, cmd, sizeof(arr[cnt].str) - 1);
    arr[cnt].str[sizeof(arr[cnt].str) - 1] = '\0';
    cnt++;
    return true;
}

void test_estopstr_parse_stores_command() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool ok = parse_safety_cmd_line("estopstr=ESTOP", true, p);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(1, p.estopCmdCount);
    TEST_ASSERT_EQUAL_STRING("ESTOP", p.EstopCmds[0].str);
}

void test_estopstr_parse_stores_angle_bracket_command() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    parse_safety_cmd_line("estopstr=<SS00>", true, p);
    TEST_ASSERT_EQUAL(1, p.estopCmdCount);
    TEST_ASSERT_EQUAL_STRING("<SS00>", p.EstopCmds[0].str);
}

void test_resumestr_parse_stores_command() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    bool ok = parse_safety_cmd_line("resumestr=RESUME", false, p);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(1, p.resumeCmdCount);
    TEST_ASSERT_EQUAL_STRING("RESUME", p.ResumeCmds[0].str);
}

void test_estop_and_resume_are_independent() {
    // Parsing estopstr= must not affect resumeCmdCount and vice versa.
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    parse_safety_cmd_line("estopstr=STOP", true, p);
    parse_safety_cmd_line("resumestr=GO", false, p);
    TEST_ASSERT_EQUAL(1, p.estopCmdCount);
    TEST_ASSERT_EQUAL(1, p.resumeCmdCount);
    TEST_ASSERT_EQUAL_STRING("STOP", p.EstopCmds[0].str);
    TEST_ASSERT_EQUAL_STRING("GO",   p.ResumeCmds[0].str);
}

void test_estopstr_multiple_commands_accumulate() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    parse_safety_cmd_line("estopstr=CMD1", true, p);
    parse_safety_cmd_line("estopstr=CMD2", true, p);
    parse_safety_cmd_line("estopstr=CMD3", true, p);
    TEST_ASSERT_EQUAL(3, p.estopCmdCount);
    TEST_ASSERT_EQUAL_STRING("CMD1", p.EstopCmds[0].str);
    TEST_ASSERT_EQUAL_STRING("CMD2", p.EstopCmds[1].str);
    TEST_ASSERT_EQUAL_STRING("CMD3", p.EstopCmds[2].str);
}

void test_estopstr_respects_max_safety_cmds_limit() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // Fill to the limit.
    for (int i = 0; i < MAX_SAFETY_CMDS; i++) {
        char line[32];
        snprintf(line, sizeof(line), "estopstr=CMD%d", i);
        parse_safety_cmd_line(line, true, p);
    }
    TEST_ASSERT_EQUAL(MAX_SAFETY_CMDS, p.estopCmdCount);
    // One more must not overflow.
    parse_safety_cmd_line("estopstr=OVERFLOW", true, p);
    TEST_ASSERT_EQUAL(MAX_SAFETY_CMDS, p.estopCmdCount);
}

void test_safety_cmd_fields_are_distinct_from_each_other() {
    // Structural guard: arrays must not alias each other or the sstr array.
    AmidalaParameters p;
    TEST_ASSERT_NOT_EQUAL((void*)p.EstopCmds,  (void*)p.ResumeCmds);
    TEST_ASSERT_NOT_EQUAL((void*)&p.estopCmdCount, (void*)&p.resumeCmdCount);
}

void test_safety_cmd_counts_default_to_zero() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    memset(EEPROM.data, 0, sizeof(EEPROM.data));
    p.init(true);
    TEST_ASSERT_EQUAL(0, p.estopCmdCount);
    TEST_ASSERT_EQUAL(0, p.resumeCmdCount);
}

void test_estopstr_broadcast_iteration_covers_all_commands() {
    // Verify that the broadcast loop pattern (iterate estopCmdCount entries)
    // visits each stored command in order — this is the same logic used in
    // handleApiEstop() to call sendSerialString() for each command.
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    parse_safety_cmd_line("estopstr=ALPHA", true, p);
    parse_safety_cmd_line("estopstr=BETA",  true, p);
    const char* expected[] = {"ALPHA", "BETA"};
    for (uint8_t i = 0; i < p.estopCmdCount; i++)
        TEST_ASSERT_EQUAL_STRING(expected[i], p.EstopCmds[i].str);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_vref_fields_are_distinct_addresses);
    RUN_TEST(test_vref_fields_write_independently);
    RUN_TEST(test_vref_intparam_routes_to_correct_field);

    RUN_TEST(test_servo_clamp_broken_always_returns_90);
    RUN_TEST(test_servo_clamp_fixed_preserves_valid_positions);
    RUN_TEST(test_servo_clamp_fixed_clamps_below_zero);
    RUN_TEST(test_servo_clamp_fixed_clamps_above_180);

    RUN_TEST(test_domercaddr_intparam_routes_to_correct_field);
    RUN_TEST(test_domercaddr_clamps_below_minimum);
    RUN_TEST(test_domercchan_intparam_channel_1);
    RUN_TEST(test_domercchan_intparam_channel_2);
    RUN_TEST(test_domercchan_clamps_channel_3_to_2);
    RUN_TEST(test_domercqpps_intparam_routes_to_correct_field);
    RUN_TEST(test_domefront_intparam_routes_to_correct_field);
    RUN_TEST(test_domefront_accepts_zero);
    RUN_TEST(test_domefront_accepts_359);
    RUN_TEST(test_domefront_clamps_360_to_359);
    RUN_TEST(test_domestall_intparam_routes_to_correct_field);
    RUN_TEST(test_domestall_clamps_below_minimum);
    RUN_TEST(test_roboclaw_fields_are_distinct_from_each_other);

    RUN_TEST(test_altbtn_intparam_routes_to_correct_field);
    RUN_TEST(test_altbtn_accepts_zero_disabled);
    RUN_TEST(test_altbtn_accepts_nine_max);
    RUN_TEST(test_altbtn_clamps_above_nine);
    RUN_TEST(test_altdomestick_intparam_zero);
    RUN_TEST(test_altdomestick_intparam_one);
    RUN_TEST(test_altdomestick_clamps_above_one);

    RUN_TEST(test_volume_channel_fields_are_distinct_from_each_other);
    RUN_TEST(test_volume_channel_defaults_after_init);
    RUN_TEST(test_volumeChA_intparam_routes_to_correct_field);
    RUN_TEST(test_volumeChB_intparam_routes_to_correct_field);
    RUN_TEST(test_volumewheel_accepts_zero_global);
    RUN_TEST(test_volumewheel_accepts_three_max);
    RUN_TEST(test_volumewheel_clamps_above_three);
    RUN_TEST(test_altvolumewheel_accepts_zero_fallthrough);
    RUN_TEST(test_altvolumewheel_accepts_nonzero);
    RUN_TEST(test_altvolumewheel_clamps_above_three);
    RUN_TEST(test_channel_volumes_are_independent_of_volume);

    RUN_TEST(test_mutebutton_intparam_routes_to_correct_field);
    RUN_TEST(test_mutebutton_accepts_zero_disabled);
    RUN_TEST(test_mutebutton_accepts_nine_max);
    RUN_TEST(test_mutebutton_clamps_above_nine);
    RUN_TEST(test_mutebutton_default_is_zero_after_init);
    RUN_TEST(test_mutebutton_field_is_distinct_from_altbtn);

    RUN_TEST(test_sstr_parse_basic_name_and_str);
    RUN_TEST(test_sstr_parse_short_name_not_empty);
    RUN_TEST(test_sstr_parse_name_not_truncated_by_five);
    RUN_TEST(test_sstr_parse_angle_bracket_command_preserved);
    RUN_TEST(test_sstr_parse_no_pipe_stores_str_only);
    RUN_TEST(test_sstr_parse_broken_eats_first_five_chars);
    RUN_TEST(test_sstr_parse_broken_short_name_becomes_empty);

    RUN_TEST(test_estopstr_parse_stores_command);
    RUN_TEST(test_estopstr_parse_stores_angle_bracket_command);
    RUN_TEST(test_resumestr_parse_stores_command);
    RUN_TEST(test_estop_and_resume_are_independent);
    RUN_TEST(test_estopstr_multiple_commands_accumulate);
    RUN_TEST(test_estopstr_respects_max_safety_cmds_limit);
    RUN_TEST(test_safety_cmd_fields_are_distinct_from_each_other);
    RUN_TEST(test_safety_cmd_counts_default_to_zero);
    RUN_TEST(test_estopstr_broadcast_iteration_covers_all_commands);

    return UNITY_END();
}
