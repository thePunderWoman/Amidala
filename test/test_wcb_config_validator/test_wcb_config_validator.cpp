// test_wcb_config_validator.cpp
// Unit tests for WCBConfigValidator (include/wcb_config_validator.h).
//
// Covers the "enabled but not fully configured" boot-warning path: WCB
// Client must never attempt to join the mesh with an incomplete identity.

#include "arduino_mock.h"
#include "wcb_config_validator.h"
#include <unity.h>
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

// A fully valid config, reused as a baseline and mutated per test.
static AmidalaParameters validParams() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    strncpy(p.wcbpassword, "correct horse battery staple", sizeof(p.wcbpassword) - 1);
    p.wcbquantity = 5;
    p.wcbid       = 3;
    p.wcboct2     = 10;
    p.wcboct3     = 20;
    return p;
}

// ---------------------------------------------------------------------------
// Valid configuration

void test_valid_config_passes() {
    AmidalaParameters p = validParams();
    WCBConfigValidation v = WCBConfigValidator::validate(p);
    TEST_ASSERT_TRUE(v.ok);
    TEST_ASSERT_NULL(v.reason);
}

void test_special_id_20_is_valid() {
    AmidalaParameters p = validParams();
    p.wcbid = 20;
    WCBConfigValidation v = WCBConfigValidator::validate(p);
    TEST_ASSERT_TRUE(v.ok);
}

void test_any_octet_value_is_valid() {
    AmidalaParameters p = validParams();
    p.wcboct2 = 0;
    p.wcboct3 = 255;
    WCBConfigValidation v = WCBConfigValidator::validate(p);
    TEST_ASSERT_TRUE(v.ok);
}

// ---------------------------------------------------------------------------
// Missing/invalid fields

void test_empty_password_fails() {
    AmidalaParameters p = validParams();
    p.wcbpassword[0] = '\0';
    WCBConfigValidation v = WCBConfigValidator::validate(p);
    TEST_ASSERT_FALSE(v.ok);
    TEST_ASSERT_NOT_NULL(v.reason);
}

void test_zero_quantity_fails() {
    AmidalaParameters p = validParams();
    p.wcbquantity = 0;
    WCBConfigValidation v = WCBConfigValidator::validate(p);
    TEST_ASSERT_FALSE(v.ok);
}

void test_zero_id_fails() {
    AmidalaParameters p = validParams();
    p.wcbid = 0;
    WCBConfigValidation v = WCBConfigValidator::validate(p);
    TEST_ASSERT_FALSE(v.ok);
}

void test_id_above_20_fails() {
    AmidalaParameters p = validParams();
    p.wcbid = 21;
    WCBConfigValidation v = WCBConfigValidator::validate(p);
    TEST_ASSERT_FALSE(v.ok);
}

// A completely zeroed/never-configured struct (the real "enabled but never
// touched the other settings" scenario from item 4) must fail cleanly.
void test_unconfigured_struct_fails() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    WCBConfigValidation v = WCBConfigValidator::validate(p);
    TEST_ASSERT_FALSE(v.ok);
    TEST_ASSERT_NOT_NULL(v.reason);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_valid_config_passes);
    RUN_TEST(test_special_id_20_is_valid);
    RUN_TEST(test_any_octet_value_is_valid);

    RUN_TEST(test_empty_password_fails);
    RUN_TEST(test_zero_quantity_fails);
    RUN_TEST(test_zero_id_fails);
    RUN_TEST(test_id_above_20_fails);
    RUN_TEST(test_unconfigured_struct_fails);

    return UNITY_END();
}
