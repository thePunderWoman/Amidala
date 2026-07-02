// test_sstr_name_refs.cpp
// Regression tests for stable ID-based serial string references in button actions.
//
// Each SerialString has a permanent uint16_t id assigned at creation.
// Button actions reference serial strings by that ID (ButtonAction::serialid).
// IDs never change even if strings are reordered or deleted.
//
// Tests verify:
//   1. parseUintArgs correctly splits all-numeric comma-separated fields.
//   2. ButtonAction::serialid (uint16_t) field exists and holds the ID.
//   3. IDs survive reorder and delete of other entries.
//   4. findSerialStringById-style lookup works correctly.
//   5. Deleting a serial string clears button refs with matching ID (sets to 0).
//   6. serialid==0 means "no serial string" (graceful no-op).

#include "arduino_mock.h"
#include "params.h"
#include "core.h"
#include <unity.h>
#include <string.h>

void setUp(void)    { memset(EEPROM.data, 0, sizeof(EEPROM.data)); }
void tearDown(void) {}

// ---- parseUintArgs ----------------------------------------------------------

void test_parseUintArgs_all_numeric() {
    uint16_t args[4] = {};
    uint8_t cnt = parseUintArgs("1,7,2,1", args, 4);
    TEST_ASSERT_EQUAL(4, cnt);
    TEST_ASSERT_EQUAL(1, args[0]);
    TEST_ASSERT_EQUAL(7, args[1]);
    TEST_ASSERT_EQUAL(2, args[2]);
    TEST_ASSERT_EQUAL(1, args[3]);
}

void test_parseUintArgs_two_fields() {
    uint16_t args[4] = {};
    uint8_t cnt = parseUintArgs("1,5", args, 4);
    TEST_ASSERT_EQUAL(2, cnt);
    TEST_ASSERT_EQUAL(1, args[0]);
    TEST_ASSERT_EQUAL(5, args[1]);
}

void test_parseUintArgs_single_field() {
    uint16_t args[4] = {};
    uint8_t cnt = parseUintArgs("42", args, 4);
    TEST_ASSERT_EQUAL(1, cnt);
    TEST_ASSERT_EQUAL(42, args[0]);
}

void test_parseUintArgs_stops_at_non_digit() {
    uint16_t args[4] = {};
    uint8_t cnt = parseUintArgs("1,5,Hello", args, 4);
    TEST_ASSERT_EQUAL(2, cnt);
    TEST_ASSERT_EQUAL(1, args[0]);
    TEST_ASSERT_EQUAL(5, args[1]);
}

void test_parseUintArgs_empty_string_returns_zero() {
    uint16_t args[4] = {};
    uint8_t cnt = parseUintArgs("", args, 4);
    TEST_ASSERT_EQUAL(0, cnt);
}

void test_parseUintArgs_respects_maxargs() {
    uint16_t args[2] = {};
    uint8_t cnt = parseUintArgs("1,2,3,4", args, 2);
    TEST_ASSERT_EQUAL(2, cnt);
    TEST_ASSERT_EQUAL(1, args[0]);
    TEST_ASSERT_EQUAL(2, args[1]);
}

void test_parseUintArgs_large_id() {
    uint16_t args[2] = {};
    uint8_t cnt = parseUintArgs("1,65535", args, 2);
    TEST_ASSERT_EQUAL(2, cnt);
    TEST_ASSERT_EQUAL(65535, args[1]);
}

// ---- ButtonAction::serialid field ------------------------------------------

void test_serialid_field_exists_and_is_uint16() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.serialid = 42;
    TEST_ASSERT_EQUAL(42, (int)b.serialid);
    TEST_ASSERT_EQUAL(2, (int)sizeof(b.serialid));
}

void test_serialid_zero_means_none() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    TEST_ASSERT_EQUAL(0, (int)b.serialid);
}

void test_serialid_cleared_by_memset() {
    ButtonAction b;
    memset(&b, 0xff, sizeof(b));
    memset(&b, 0, sizeof(b));
    TEST_ASSERT_EQUAL(0, (int)b.serialid);
}

void test_serialid_holds_large_value() {
    ButtonAction b;
    memset(&b, 0, sizeof(b));
    b.serialid = 1000;
    TEST_ASSERT_EQUAL(1000, (int)b.serialid);
}

// ---- SerialString::id field -------------------------------------------------

void test_serial_string_has_id_field() {
    SerialString s;
    memset(&s, 0, sizeof(s));
    s.id = 99;
    TEST_ASSERT_EQUAL(99, (int)s.id);
}

// ---- ID stability: survives reorder and delete ------------------------------
// These helpers mirror the in-memory operations in wifi_ap.cpp.

static void addStrWithId(AmidalaParameters &p, uint16_t id,
                          const char *name, const char *str) {
    if ((int)p.serialcount >= (int)p.getSerialStringCount()) return;
    p.Str[p.serialcount].id = id;
    strncpy(p.Str[p.serialcount].name, name, sizeof(p.Str[0].name) - 1);
    strncpy(p.Str[p.serialcount].str,  str,  sizeof(p.Str[0].str)  - 1);
    p.serialcount++;
    if (id >= p.nextSstrId) p.nextSstrId = id + 1;
}

// Mirror of findSerialStringById in buttons.cpp (static there, so tested here).
static int findById(AmidalaParameters &p, uint16_t id) {
    if (id == 0) return -1;
    for (int i = 0; i < (int)p.serialcount; i++)
        if (p.Str[i].id == id) return i;
    return -1;
}

// Mirror of sstr_del shift + ref-clearing (from wifi_ap.cpp).
static void delStr(AmidalaParameters &p, int idx) {
    if (idx < 0 || idx >= (int)p.serialcount) return;
    uint16_t deletedId = p.Str[idx].id;
    for (int j = idx; j < (int)p.serialcount - 1; j++)
        p.Str[j] = p.Str[j + 1];
    memset(&p.Str[p.serialcount - 1], 0, sizeof(SerialString));
    p.serialcount--;
    ButtonAction *layers[4] = {p.B, p.LB, p.AB, p.DB};
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < (int)p.getButtonCount(); i++)
            if (layers[l][i].serialid == deletedId)
                layers[l][i].serialid = 0;
    for (int i = 0; i < (int)p.gcount; i++)
        if (p.G[i].action.serialid == deletedId)
            p.G[i].action.serialid = 0;
}

void test_id_lookup_finds_correct_entry() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 1,  "Vader",       "BD:VADER");
    addStrWithId(p, 9,  "Hello There", "DM:HELLO");
    addStrWithId(p, 5,  "Low Panels",  "DM:LOW");

    int idx = findById(p, 9);
    TEST_ASSERT_EQUAL(1, idx);
    TEST_ASSERT_EQUAL_STRING("Hello There", p.Str[idx].name);
    TEST_ASSERT_EQUAL_STRING("DM:HELLO",    p.Str[idx].str);
}

void test_id_lookup_zero_returns_neg1() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 1, "Vader", "BD:VADER");
    TEST_ASSERT_EQUAL(-1, findById(p, 0));
}

void test_id_lookup_missing_id_returns_neg1() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 1, "Vader", "BD:VADER");
    TEST_ASSERT_EQUAL(-1, findById(p, 99));
}

void test_button_ref_survives_delete_of_earlier_string() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 1, "Vader",       "BD:VADER");
    addStrWithId(p, 9, "Hello There", "DM:HELLO");
    addStrWithId(p, 5, "Heart",       "DM:HEART");

    p.B[0].action   = ButtonAction::kSerialStr;
    p.B[0].serialid = 9;

    // Delete "Vader" (index 0) — shifts Hello There from index 1 → 0.
    delStr(p, 0);

    int idx = findById(p, 9);
    TEST_ASSERT_NOT_EQUAL(-1, idx);
    TEST_ASSERT_EQUAL_STRING("DM:HELLO", p.Str[idx].str);
    TEST_ASSERT_EQUAL(9, (int)p.B[0].serialid);  // ref still intact
}

void test_button_ref_survives_delete_of_later_string() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 9, "Hello There", "DM:HELLO");
    addStrWithId(p, 5, "Heart",       "DM:HEART");

    p.B[0].action   = ButtonAction::kSerialStr;
    p.B[0].serialid = 9;

    delStr(p, 1);  // delete Heart — Hello There stays at index 0

    int idx = findById(p, 9);
    TEST_ASSERT_NOT_EQUAL(-1, idx);
    TEST_ASSERT_EQUAL_STRING("DM:HELLO", p.Str[idx].str);
    TEST_ASSERT_EQUAL(9, (int)p.B[0].serialid);
}

void test_button_ref_survives_add() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 9, "Hello There", "DM:HELLO");

    p.B[0].action   = ButtonAction::kSerialStr;
    p.B[0].serialid = 9;

    addStrWithId(p, 14, "Alarm", "DM:ALARM");  // new ID — doesn't disturb 9

    int idx = findById(p, 9);
    TEST_ASSERT_NOT_EQUAL(-1, idx);
    TEST_ASSERT_EQUAL_STRING("DM:HELLO", p.Str[idx].str);
    TEST_ASSERT_EQUAL(9, (int)p.B[0].serialid);
}

void test_delete_clears_button_refs_with_matching_id() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 9, "Hello There", "DM:HELLO");
    addStrWithId(p, 5, "Heart",       "DM:HEART");

    p.B[0].action    = ButtonAction::kSerialStr;
    p.B[0].serialid  = 9;
    p.LB[1].action   = ButtonAction::kSerialStr;
    p.LB[1].serialid = 9;
    p.B[1].action    = ButtonAction::kSerialStr;
    p.B[1].serialid  = 5;  // different ID — must NOT be cleared

    delStr(p, 0);  // delete Hello There (ID 9)

    TEST_ASSERT_EQUAL(0, (int)p.B[0].serialid);
    TEST_ASSERT_EQUAL(0, (int)p.LB[1].serialid);
    TEST_ASSERT_EQUAL(5, (int)p.B[1].serialid);  // untouched
}

void test_delete_clears_ab_db_refs() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 3, "Reset All", "DM:RESET");

    p.AB[0].action   = ButtonAction::kSerialStr;
    p.AB[0].serialid = 3;
    p.DB[0].action   = ButtonAction::kSerialStr;
    p.DB[0].serialid = 3;

    delStr(p, 0);  // delete Reset All (ID 3)

    TEST_ASSERT_EQUAL(0, (int)p.AB[0].serialid);
    TEST_ASSERT_EQUAL(0, (int)p.DB[0].serialid);
}

void test_delete_does_not_affect_unrelated_refs() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 1, "Vader",       "BD:VADER");
    addStrWithId(p, 9, "Hello There", "DM:HELLO");

    p.B[0].action   = ButtonAction::kSerialStr;
    p.B[0].serialid = 9;  // will NOT be deleted
    p.B[1].action   = ButtonAction::kSerialStr;
    p.B[1].serialid = 1;  // Vader — will be deleted

    delStr(p, 0);  // delete Vader (ID 1, index 0)

    TEST_ASSERT_EQUAL(0, (int)p.B[1].serialid);   // cleared
    TEST_ASSERT_EQUAL(9, (int)p.B[0].serialid);   // untouched
}

void test_rename_does_not_affect_button_id_ref() {
    // Renaming a string's display name has zero effect on ID-based button refs.
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 9, "Hello There", "DM:HELLO");

    p.B[0].action   = ButtonAction::kSerialStr;
    p.B[0].serialid = 9;

    strncpy(p.Str[0].name, "Greetings", sizeof(p.Str[0].name) - 1);

    TEST_ASSERT_EQUAL(9, (int)p.B[0].serialid);  // ref unchanged after rename
    int idx = findById(p, 9);
    TEST_ASSERT_NOT_EQUAL(-1, idx);
    TEST_ASSERT_EQUAL_STRING("Greetings", p.Str[idx].name);
    TEST_ASSERT_EQUAL_STRING("DM:HELLO",  p.Str[idx].str);
}

void test_zero_serialid_resolves_to_neg1() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    addStrWithId(p, 1, "Vader", "BD:VADER");
    TEST_ASSERT_EQUAL(-1, findById(p, 0));
}

// ---- nextSstrId counter ----------------------------------------------------

void test_nextsstr_id_advances_on_add() {
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    p.nextSstrId = 1;
    p.Str[0].id = p.nextSstrId++;
    p.Str[1].id = p.nextSstrId++;
    p.serialcount = 2;
    TEST_ASSERT_EQUAL(1, (int)p.Str[0].id);
    TEST_ASSERT_EQUAL(2, (int)p.Str[1].id);
    TEST_ASSERT_EQUAL(3, (int)p.nextSstrId);
}

void test_nextsstr_id_computed_from_max_on_boot() {
    // On boot, nextSstrId = max(all IDs) + 1
    AmidalaParameters p; memset(&p, 0, sizeof(p));
    p.nextSstrId = 0;
    addStrWithId(p, 5, "A", "a");
    addStrWithId(p, 2, "B", "b");
    addStrWithId(p, 9, "C", "c");
    TEST_ASSERT_EQUAL(10, (int)p.nextSstrId);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_parseUintArgs_all_numeric);
    RUN_TEST(test_parseUintArgs_two_fields);
    RUN_TEST(test_parseUintArgs_single_field);
    RUN_TEST(test_parseUintArgs_stops_at_non_digit);
    RUN_TEST(test_parseUintArgs_empty_string_returns_zero);
    RUN_TEST(test_parseUintArgs_respects_maxargs);
    RUN_TEST(test_parseUintArgs_large_id);

    RUN_TEST(test_serialid_field_exists_and_is_uint16);
    RUN_TEST(test_serialid_zero_means_none);
    RUN_TEST(test_serialid_cleared_by_memset);
    RUN_TEST(test_serialid_holds_large_value);

    RUN_TEST(test_serial_string_has_id_field);

    RUN_TEST(test_id_lookup_finds_correct_entry);
    RUN_TEST(test_id_lookup_zero_returns_neg1);
    RUN_TEST(test_id_lookup_missing_id_returns_neg1);

    RUN_TEST(test_button_ref_survives_delete_of_earlier_string);
    RUN_TEST(test_button_ref_survives_delete_of_later_string);
    RUN_TEST(test_button_ref_survives_add);
    RUN_TEST(test_delete_clears_button_refs_with_matching_id);
    RUN_TEST(test_delete_clears_ab_db_refs);
    RUN_TEST(test_delete_does_not_affect_unrelated_refs);
    RUN_TEST(test_rename_does_not_affect_button_id_ref);
    RUN_TEST(test_zero_serialid_resolves_to_neg1);

    RUN_TEST(test_nextsstr_id_advances_on_add);
    RUN_TEST(test_nextsstr_id_computed_from_max_on_boot);

    return UNITY_END();
}
