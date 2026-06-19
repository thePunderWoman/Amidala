// test_web_pages.cpp
// Tests for the web UI layer:
//   1. HTML content — verify pages reference the correct API endpoints,
//      navigation links, and config keys so renames or copy-paste errors are
//      caught at compile/test time rather than at runtime in the browser.
//   2. JSON builders (web_api.h) — verify that buildGeneralConfigJson and
//      buildInfoJson produce well-formed output with the correct keys and
//      values for known AmidalaParameters states.

#include "arduino_mock.h"
#include "params.h"
#include "web_pages.h"
#include "web_api.h"
#include <unity.h>
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != nullptr;
}

// ---------------------------------------------------------------------------
// Landing page (WEB_PAGE_HOME) — content checks
// ---------------------------------------------------------------------------

void test_home_page_has_info_api_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/api/info"));
}

void test_home_page_has_all_config_nav_links() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/general"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/audio"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/dome"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/buttons"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/servos"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/xbee"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/serial-strings"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/rc-radio"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/wifi"));
}

void test_home_page_has_tools_nav_links() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/sequences"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/monitor"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/update"));
}

void test_home_page_has_viewport_meta() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "viewport"));
}

void test_home_page_populates_build_strip_ids() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "bi-dr"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "bi-do"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "bi-au"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "bi-wi"));
}

// ---------------------------------------------------------------------------
// General settings page (WEB_PAGE_GENERAL) — content checks
// ---------------------------------------------------------------------------

void test_general_page_fetches_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "/api/config/general"));
}

void test_general_page_posts_to_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "/api/config"));
}

void test_general_page_has_back_link() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "href=\"/\""));
}

void test_general_page_schema_has_expected_keys() {
    // These keys must match what buildGeneralConfigJson returns AND what
    // processConfig() accepts.  Any rename in either place will break the UI.
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'volume'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'startup'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'rndon'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'mindelay'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'maxdelay'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'ackon'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'goslow'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'mix12'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'auto'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'serialbaud'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'serialdelim'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'serialeol'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'myi2c'"));
}

void test_general_page_has_viewport_meta() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "viewport"));
}

// ---------------------------------------------------------------------------
// buildGeneralConfigJson — JSON content
// ---------------------------------------------------------------------------

// AmidalaParameters::init() uses internal statics, so calling it on multiple
// instances in one process only fully initialises the first one.  For JSON
// builder tests we set fields explicitly after memset so every test is
// independent regardless of execution order.
static AmidalaParameters makeParams() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    // Mirror the defaults from params.h init() that the JSON tests care about
    p.volume      = 50;
    p.startup     = true;
    p.rndon       = true;
    p.ackon       = false;
    p.goslow      = false;
    p.mix12       = false;
    p.autocorrect = false;
    p.mindelay    = 60;
    p.maxdelay    = 120;
    p.serialbaud  = 9600;
    p.serialdelim = ':';
    p.serialeol   = 13;
    p.myi2c       = 0;
    return p;
}

void test_general_json_contains_all_keys() {
    AmidalaParameters p = makeParams();
    String json = buildGeneralConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"volume\""));
    TEST_ASSERT_TRUE(contains(s, "\"startup\""));
    TEST_ASSERT_TRUE(contains(s, "\"rndon\""));
    TEST_ASSERT_TRUE(contains(s, "\"mindelay\""));
    TEST_ASSERT_TRUE(contains(s, "\"maxdelay\""));
    TEST_ASSERT_TRUE(contains(s, "\"ackon\""));
    TEST_ASSERT_TRUE(contains(s, "\"goslow\""));
    TEST_ASSERT_TRUE(contains(s, "\"mix12\""));
    TEST_ASSERT_TRUE(contains(s, "\"auto\""));
    TEST_ASSERT_TRUE(contains(s, "\"serialbaud\""));
    TEST_ASSERT_TRUE(contains(s, "\"serialdelim\""));
    TEST_ASSERT_TRUE(contains(s, "\"serialeol\""));
    TEST_ASSERT_TRUE(contains(s, "\"myi2c\""));
}

void test_general_json_default_volume() {
    AmidalaParameters p = makeParams();
    String json = buildGeneralConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"volume\":50"));
}

void test_general_json_bool_true_encodes_as_y() {
    AmidalaParameters p = makeParams();
    // startup=true, rndon=true by default in makeParams()
    String json = buildGeneralConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"startup\":\"y\""));
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"rndon\":\"y\""));
}

void test_general_json_bool_false_encodes_as_n() {
    AmidalaParameters p = makeParams();
    // ackon=false, goslow=false by default in makeParams()
    String json = buildGeneralConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"ackon\":\"n\""));
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"goslow\":\"n\""));
}

void test_general_json_wraps_in_braces() {
    AmidalaParameters p = makeParams();
    String json = buildGeneralConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_EQUAL('{', s[0]);
    TEST_ASSERT_EQUAL('}', s[strlen(s) - 1]);
}

void test_general_json_no_trailing_comma_before_close() {
    AmidalaParameters p = makeParams();
    String json = buildGeneralConfigJson(p);
    // The character just before the closing brace must not be a comma
    const char* s = json.c_str();
    size_t len = strlen(s);
    TEST_ASSERT_TRUE(len >= 2);
    TEST_ASSERT_NOT_EQUAL(',', s[len - 2]);
}

void test_general_json_reflects_changed_volume() {
    AmidalaParameters p = makeParams();
    p.volume = 77;
    String json = buildGeneralConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"volume\":77"));
}

void test_general_json_reflects_changed_serialbaud() {
    AmidalaParameters p = makeParams();
    p.serialbaud = 115200;
    String json = buildGeneralConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"serialbaud\":115200"));
}

// ---------------------------------------------------------------------------
// buildInfoJson — JSON content
// ---------------------------------------------------------------------------

void test_info_json_contains_firmware_key() {
    String json = buildInfoJson("roboteq-pwm", "roboclaw", "hcr", "amidala", "192.168.4.1");
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"firmware\""));
}

void test_info_json_contains_version_key() {
    String json = buildInfoJson("roboteq-pwm", "roboclaw", "hcr", "amidala", "192.168.4.1");
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"version\""));
}

void test_info_json_drive_string_appears() {
    String json = buildInfoJson("roboteq-pwm", "roboclaw", "hcr", "amidala", "192.168.4.1");
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"roboteq-pwm\""));
}

void test_info_json_null_drive_emits_null() {
    String json = buildInfoJson(nullptr, nullptr, "hcr", "amidala", "192.168.4.1");
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"drive\":null"));
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"dome\":null"));
}

void test_info_json_ssid_appears() {
    String json = buildInfoJson("pwm", "pwm", "hcr", "myrobot", "192.168.4.1");
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"wifi_ssid\":\"myrobot\""));
}

void test_info_json_wraps_in_braces() {
    String json = buildInfoJson("pwm", "pwm", "hcr", "amidala", "192.168.4.1");
    const char* s = json.c_str();
    TEST_ASSERT_EQUAL('{', s[0]);
    TEST_ASSERT_EQUAL('}', s[strlen(s) - 1]);
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    // Landing page
    RUN_TEST(test_home_page_has_info_api_endpoint);
    RUN_TEST(test_home_page_has_all_config_nav_links);
    RUN_TEST(test_home_page_has_tools_nav_links);
    RUN_TEST(test_home_page_has_viewport_meta);
    RUN_TEST(test_home_page_populates_build_strip_ids);

    // General settings page
    RUN_TEST(test_general_page_fetches_config_endpoint);
    RUN_TEST(test_general_page_posts_to_config_endpoint);
    RUN_TEST(test_general_page_has_back_link);
    RUN_TEST(test_general_page_schema_has_expected_keys);
    RUN_TEST(test_general_page_has_viewport_meta);

    // buildGeneralConfigJson
    RUN_TEST(test_general_json_contains_all_keys);
    RUN_TEST(test_general_json_default_volume);
    RUN_TEST(test_general_json_bool_true_encodes_as_y);
    RUN_TEST(test_general_json_bool_false_encodes_as_n);
    RUN_TEST(test_general_json_wraps_in_braces);
    RUN_TEST(test_general_json_no_trailing_comma_before_close);
    RUN_TEST(test_general_json_reflects_changed_volume);
    RUN_TEST(test_general_json_reflects_changed_serialbaud);

    // buildInfoJson
    RUN_TEST(test_info_json_contains_firmware_key);
    RUN_TEST(test_info_json_contains_version_key);
    RUN_TEST(test_info_json_drive_string_appears);
    RUN_TEST(test_info_json_null_drive_emits_null);
    RUN_TEST(test_info_json_ssid_appears);
    RUN_TEST(test_info_json_wraps_in_braces);

    return UNITY_END();
}
