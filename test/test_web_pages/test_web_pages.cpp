// test_web_pages.cpp
// Tests for the web UI layer:
//   1. HTML content — verify pages reference /api/config, have back links,
//      and list the correct SCHEMA keys so renames or omissions are caught
//      at test time rather than at runtime in the browser.
//   2. JSON builder (web_api.h) — verify that buildFullConfigJson and
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
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/controllers"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/servos"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/connectivity"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/serial-strings"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/gadgets"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/config/rc-radio"));
}

void test_home_page_has_tools_nav_links() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/droid-control"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/monitor"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/update"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_HOME, "/safety"));
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
// Config pages — shared endpoint + back-link checks
// ---------------------------------------------------------------------------

void test_general_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "href=\"/\""));
}

void test_general_page_has_viewport_meta() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "viewport"));
}

void test_wifi_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_WIFI, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_WIFI, "href=\"/\""));
}

void test_xbee_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_XBEE, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_XBEE, "href=\"/\""));
}

void test_connectivity_page_has_all_sections() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "href=\"/\""));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "'xbr'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "'xbl'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "'wifion'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "'wifissid'"));
    // BT section uses standard section-label class (no custom bt-* styles needed)
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "Bluetooth Controller"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "/api/bt/status"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "/api/bt/scan"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONFIG_CONNECTIVITY, "/api/bt/pair"));
}

void test_audio_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "href=\"/\""));
}

void test_rc_radio_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_RC_RADIO, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_RC_RADIO, "href=\"/\""));
}

void test_dome_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DOME, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DOME, "href=\"/\""));
}

// ---------------------------------------------------------------------------
// Config pages — SCHEMA key checks
// ---------------------------------------------------------------------------

void test_general_page_schema_keys() {
    // Sound settings moved to audio page
    TEST_ASSERT_FALSE(contains(WEB_PAGE_GENERAL, "'volume'"));
    TEST_ASSERT_FALSE(contains(WEB_PAGE_GENERAL, "'startup'"));
    TEST_ASSERT_FALSE(contains(WEB_PAGE_GENERAL, "'rndon'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'goslow'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'mix12'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'auto'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'serialbaud'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'serialdelim'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'serialeol'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "'myi2c'"));
}

void test_estop_present_on_config_pages() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GENERAL, "/api/estop"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO,   "/api/estop"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERVOS,  "/api/estop"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_MONITOR, "/api/estop"));
}

void test_wifi_page_schema_keys() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_WIFI, "'wifion'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_WIFI, "'wifissid'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_WIFI, "'wifipassword'"));
}

void test_xbee_page_schema_keys() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_XBEE, "'xbr'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_XBEE, "'xbl'"));
}

void test_audio_page_schema_keys() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'volume'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'volumeChA'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'volumeChB'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'startupem'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'ackem'"));
    // Sound playback settings moved here from general
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'startup'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'rndon'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'mindelay'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'maxdelay'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_AUDIO, "'ackon'"));
}

void test_info_json_has_free_heap() {
    String json = buildInfoJson("pwm", "pwm", "hcr", "amidala", "192.168.4.1", 27, 290816);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"sstr_used\":27"));
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"free_heap\":290816"));
    TEST_ASSERT_FALSE(contains(json.c_str(), "sstr_max"));
}

void test_rc_radio_page_schema_keys() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_RC_RADIO, "'rcchn'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_RC_RADIO, "'rvrmin'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_RC_RADIO, "'fst'"));
}

void test_dome_page_schema_keys() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DOME, "'domespeed'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DOME, "'domeflip'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DOME, "'domercaddr'"));
}

void test_monitor_page_uses_monitor_api() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_MONITOR, "/api/monitor"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_MONITOR, "href=\"/\""));
}

void test_monitor_page_has_send_and_pause_ui() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_MONITOR, "sendCmd"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_MONITOR, "togglePause"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_MONITOR, "clearLog"));
}

void test_servos_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERVOS, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERVOS, "href=\"/\""));
}

void test_servos_page_has_edit_ui() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERVOS, "editRow"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERVOS, "saveRow"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERVOS, "key=s&value="));
}

void test_serial_strings_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERIAL_STRINGS, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERIAL_STRINGS, "href=\"/\""));
}

void test_serial_strings_page_has_add_and_delete_ui() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERIAL_STRINGS, "addStr"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERIAL_STRINGS, "delStr"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERIAL_STRINGS, "sstr_del_"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERIAL_STRINGS, "confirm"));
}

// ---------------------------------------------------------------------------
// buildFullConfigJson — JSON content
// ---------------------------------------------------------------------------

// AmidalaParameters::init() uses internal statics, so calling it on multiple
// instances in one process only fully initialises the first one.  For JSON
// builder tests we set fields explicitly after memset so every test is
// independent regardless of execution order.
static AmidalaParameters makeParams() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
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

void test_full_config_json_wraps_in_braces() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_EQUAL('{', s[0]);
    TEST_ASSERT_EQUAL('}', s[strlen(s) - 1]);
}

void test_full_config_json_no_trailing_comma() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_NOT_EQUAL(',', s[strlen(s) - 2]);
}

void test_full_config_json_general_keys() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
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

void test_full_config_json_wifi_keys() {
    AmidalaParameters p = makeParams();
    strncpy(p.wifiSSID, "amidala", sizeof(p.wifiSSID));
    String json = buildFullConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"wifion\""));
    TEST_ASSERT_TRUE(contains(s, "\"wifissid\""));
    TEST_ASSERT_TRUE(contains(s, "\"wifipassword\""));
    TEST_ASSERT_TRUE(contains(s, "\"wifissid\":\"amidala\""));
}

void test_full_config_json_xbee_hex_format() {
    AmidalaParameters p = makeParams();
    p.xbr = 0x0A1B2C3D;
    p.xbl = 0x00000000;
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"xbr\":\"0A1B2C3D\""));
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"xbl\":\"00000000\""));
}

void test_full_config_json_btaddr_empty_by_default() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"btaddr\":\"\""));
}

void test_full_config_json_btaddr_stored_correctly() {
    AmidalaParameters p = makeParams();
    strncpy(p.btaddr, "AA:BB:CC:DD:EE:FF", sizeof(p.btaddr));
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"btaddr\":\"AA:BB:CC:DD:EE:FF\""));
}

void test_full_config_json_audio_keys() {
    AmidalaParameters p = makeParams();
    p.audiohw = 1; // AUDIO_HW_HCR
    String json = buildFullConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"audiohw\":\"hcr\""));
    TEST_ASSERT_TRUE(contains(s, "\"volumeChA\""));
    TEST_ASSERT_TRUE(contains(s, "\"volumeChB\""));
    TEST_ASSERT_TRUE(contains(s, "\"startupem\""));
    TEST_ASSERT_TRUE(contains(s, "\"ackem\""));
}

void test_full_config_json_rc_radio_keys() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"rcchn\""));
    TEST_ASSERT_TRUE(contains(s, "\"rcd\""));
    TEST_ASSERT_TRUE(contains(s, "\"fst\""));
    TEST_ASSERT_TRUE(contains(s, "\"rvrmin\""));
    TEST_ASSERT_TRUE(contains(s, "\"j1adjv\""));
}

void test_full_config_json_dome_keys() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"domespeed\""));
    TEST_ASSERT_TRUE(contains(s, "\"domeflip\""));
    TEST_ASSERT_TRUE(contains(s, "\"domeimu\""));
    TEST_ASSERT_TRUE(contains(s, "\"domercaddr\""));
    TEST_ASSERT_TRUE(contains(s, "\"domestall\""));
}

void test_full_config_json_default_volume() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"volume\":50"));
}

void test_full_config_json_bool_true_encodes_as_y() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"startup\":\"y\""));
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"rndon\":\"y\""));
}

void test_full_config_json_bool_false_encodes_as_n() {
    AmidalaParameters p = makeParams();
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"ackon\":\"n\""));
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"goslow\":\"n\""));
}

void test_full_config_json_reflects_changed_volume() {
    AmidalaParameters p = makeParams();
    p.volume = 77;
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"volume\":77"));
}

void test_full_config_json_reflects_changed_serialbaud() {
    AmidalaParameters p = makeParams();
    p.serialbaud = 115200;
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"serialbaud\":115200"));
}

void test_full_config_json_servos_array() {
    AmidalaParameters p = makeParams(); // S[] is zeroed
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"servos\":["));
}

void test_full_config_json_servos_channel_fields() {
    AmidalaParameters p = makeParams();
    p.S[0].min = 10;
    p.S[0].max = 170;
    p.S[0].n   = 90;
    p.S[0].d   = 5;
    p.S[0].t   = -3;
    p.S[0].s   = 75;
    p.S[0].r   = true;
    String json = buildFullConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"min\":10"));
    TEST_ASSERT_TRUE(contains(s, "\"max\":170"));
    TEST_ASSERT_TRUE(contains(s, "\"sp\":75"));
    TEST_ASSERT_TRUE(contains(s, "\"r\":1"));
}

void test_full_config_json_sstr_empty_array() {
    AmidalaParameters p = makeParams(); // serialcount == 0
    String json = buildFullConfigJson(p);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"sstr\":[]"));
}

void test_full_config_json_sstr_with_entries() {
    AmidalaParameters p = makeParams();
    p.serialcount = 2;
    strncpy(p.Str[0].name, "Leia Sequence", sizeof(p.Str[0].name));
    strncpy(p.Str[0].str,  ":LD00",         sizeof(p.Str[0].str));
    strncpy(p.Str[1].name, "Happy R2",      sizeof(p.Str[1].name));
    strncpy(p.Str[1].str,  ":001",          sizeof(p.Str[1].str));
    String json = buildFullConfigJson(p);
    const char* s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"sstr\":["));
    TEST_ASSERT_TRUE(contains(s, "\"n\":\"Leia Sequence\""));
    TEST_ASSERT_TRUE(contains(s, "\"s\":\":LD00\""));
    TEST_ASSERT_TRUE(contains(s, "\"n\":\"Happy R2\""));
}

// ---------------------------------------------------------------------------
// Gadgets config page
// ---------------------------------------------------------------------------

void test_gadgets_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, "href=\"/\""));
}

void test_gadgets_page_has_gadget_cmd_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, "/api/gadget-cmd"));
}

void test_gadgets_page_has_periscope_and_uppity_spinner() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, "Periscope"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, "Uppity Spinner"));
}

void test_gadgets_page_has_uppity_ops_and_cfg_commands() {
    // Operational commands
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, ":PH"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, ":PMG"));
    // Calibration commands
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, "#PSC"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, "#PRESTART"));
}

void test_gadgets_page_has_droid_control_link() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_GADGETS, "/droid-control"));
}

// ---------------------------------------------------------------------------
// Safety page
// ---------------------------------------------------------------------------

void test_safety_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SAFETY, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SAFETY, "href=\"/\""));
}

void test_safety_page_has_schema_keys() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SAFETY, "'fst'"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SAFETY, "'domestall'"));
}

// ---------------------------------------------------------------------------
// Controllers page
// ---------------------------------------------------------------------------

void test_controllers_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONTROLLERS, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONTROLLERS, "href=\"/\""));
}

void test_controllers_page_has_periscope_optgroup() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONTROLLERS, "Periscope"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_CONTROLLERS, "gadgets_cfg"));
}

// ---------------------------------------------------------------------------
// Droid control page
// ---------------------------------------------------------------------------

void test_droid_control_page_uses_config_endpoint() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DROID_CONTROL, "/api/config"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DROID_CONTROL, "href=\"/\""));
}

void test_droid_control_page_has_three_tabs() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DROID_CONTROL, "Dome"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DROID_CONTROL, "Sequences"));
    TEST_ASSERT_TRUE(contains(WEB_PAGE_DROID_CONTROL, "Gadgets"));
}

// ---------------------------------------------------------------------------
// Serial strings page — respects sstr_user_cnt
// ---------------------------------------------------------------------------

void test_serial_strings_page_respects_user_cnt() {
    TEST_ASSERT_TRUE(contains(WEB_PAGE_SERIAL_STRINGS, "sstr_user_cnt"));
}

// ---------------------------------------------------------------------------
// buildFullConfigJson — gadgets overloads
// ---------------------------------------------------------------------------

void test_full_config_json_gadgets_overload_valid_json() {
    AmidalaParameters p = makeParams();
    String gadgets = "[{\"type\":0,\"sstr\":[]}]";
    String json = buildFullConfigJson(p, gadgets);
    const char* s = json.c_str();
    TEST_ASSERT_EQUAL('{', s[0]);
    TEST_ASSERT_EQUAL('}', s[strlen(s) - 1]);
    TEST_ASSERT_TRUE(contains(s, "\"gadgets_cfg\":[{\"type\":0"));
}

void test_full_config_json_with_sstr_user_cnt() {
    AmidalaParameters p = makeParams();
    p.serialcount = 5;
    String gadgets = "[]";
    String json = buildFullConfigJson(p, gadgets, 3);
    const char* s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"sstr_user_cnt\":3"));
    TEST_ASSERT_EQUAL('}', s[strlen(s) - 1]);
}

void test_full_config_json_sstr_user_cnt_zero() {
    AmidalaParameters p = makeParams(); // serialcount = 0
    String json = buildFullConfigJson(p, "[]", 0);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"sstr_user_cnt\":0"));
}

// ---------------------------------------------------------------------------
// buildInfoJson — JSON content
// ---------------------------------------------------------------------------

void test_info_json_contains_version_key() {
    String json = buildInfoJson("roboteq-pwm", "roboclaw", "hcr", "amidala", "192.168.4.1");
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"version\""));
}

void test_info_json_contains_board_rev_and_mcu() {
    String json = buildInfoJson("roboteq-pwm", "roboclaw", "hcr", "amidala", "192.168.4.1");
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"board_rev\":\"1.1\""));
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"mcu\":\"ESP32-S3 N16R8\""));
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

    // Config pages — endpoint + back-link
    RUN_TEST(test_general_page_uses_config_endpoint);
    RUN_TEST(test_general_page_has_viewport_meta);
    RUN_TEST(test_wifi_page_uses_config_endpoint);
    RUN_TEST(test_xbee_page_uses_config_endpoint);
    RUN_TEST(test_connectivity_page_has_all_sections);
    RUN_TEST(test_audio_page_uses_config_endpoint);
    RUN_TEST(test_rc_radio_page_uses_config_endpoint);
    RUN_TEST(test_dome_page_uses_config_endpoint);

    // Config pages — SCHEMA keys
    RUN_TEST(test_general_page_schema_keys);
    RUN_TEST(test_estop_present_on_config_pages);
    RUN_TEST(test_wifi_page_schema_keys);
    RUN_TEST(test_xbee_page_schema_keys);
    RUN_TEST(test_audio_page_schema_keys);
    RUN_TEST(test_rc_radio_page_schema_keys);
    RUN_TEST(test_dome_page_schema_keys);
    RUN_TEST(test_monitor_page_uses_monitor_api);
    RUN_TEST(test_monitor_page_has_send_and_pause_ui);
    RUN_TEST(test_servos_page_uses_config_endpoint);
    RUN_TEST(test_servos_page_has_edit_ui);
    RUN_TEST(test_serial_strings_page_uses_config_endpoint);
    RUN_TEST(test_serial_strings_page_has_add_and_delete_ui);
    RUN_TEST(test_serial_strings_page_respects_user_cnt);

    // Gadgets config page
    RUN_TEST(test_gadgets_page_uses_config_endpoint);
    RUN_TEST(test_gadgets_page_has_gadget_cmd_endpoint);
    RUN_TEST(test_gadgets_page_has_periscope_and_uppity_spinner);
    RUN_TEST(test_gadgets_page_has_uppity_ops_and_cfg_commands);
    RUN_TEST(test_gadgets_page_has_droid_control_link);

    // Safety page
    RUN_TEST(test_safety_page_uses_config_endpoint);
    RUN_TEST(test_safety_page_has_schema_keys);

    // Controllers page
    RUN_TEST(test_controllers_page_uses_config_endpoint);
    RUN_TEST(test_controllers_page_has_periscope_optgroup);

    // Droid control page
    RUN_TEST(test_droid_control_page_uses_config_endpoint);
    RUN_TEST(test_droid_control_page_has_three_tabs);

    // buildFullConfigJson
    RUN_TEST(test_full_config_json_wraps_in_braces);
    RUN_TEST(test_full_config_json_no_trailing_comma);
    RUN_TEST(test_full_config_json_general_keys);
    RUN_TEST(test_full_config_json_wifi_keys);
    RUN_TEST(test_full_config_json_xbee_hex_format);
    RUN_TEST(test_full_config_json_btaddr_empty_by_default);
    RUN_TEST(test_full_config_json_btaddr_stored_correctly);
    RUN_TEST(test_full_config_json_audio_keys);
    RUN_TEST(test_full_config_json_rc_radio_keys);
    RUN_TEST(test_full_config_json_dome_keys);
    RUN_TEST(test_full_config_json_default_volume);
    RUN_TEST(test_full_config_json_bool_true_encodes_as_y);
    RUN_TEST(test_full_config_json_bool_false_encodes_as_n);
    RUN_TEST(test_full_config_json_reflects_changed_volume);
    RUN_TEST(test_full_config_json_reflects_changed_serialbaud);
    RUN_TEST(test_full_config_json_servos_array);
    RUN_TEST(test_full_config_json_servos_channel_fields);
    RUN_TEST(test_full_config_json_sstr_empty_array);
    RUN_TEST(test_full_config_json_sstr_with_entries);
    RUN_TEST(test_full_config_json_gadgets_overload_valid_json);
    RUN_TEST(test_full_config_json_with_sstr_user_cnt);
    RUN_TEST(test_full_config_json_sstr_user_cnt_zero);

    // buildInfoJson
    RUN_TEST(test_info_json_contains_version_key);
    RUN_TEST(test_info_json_contains_board_rev_and_mcu);
    RUN_TEST(test_info_json_drive_string_appears);
    RUN_TEST(test_info_json_null_drive_emits_null);
    RUN_TEST(test_info_json_ssid_appears);
    RUN_TEST(test_info_json_wraps_in_braces);
    RUN_TEST(test_info_json_has_free_heap);

    return UNITY_END();
}
