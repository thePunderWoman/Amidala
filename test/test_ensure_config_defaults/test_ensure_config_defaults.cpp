// test_ensure_config_defaults.cpp
// Unit tests for ensureConfigDefaults() in include/config_file.h.
//
// Regression coverage for the bug that motivated this feature: a scalar
// setting (btcontrolleron) added to AmidalaParameters had no way to ever
// appear in an existing user's config.txt, so it silently lived only as an
// in-memory default forever with no visible trace in the file a user would
// actually look at.

#include "arduino_mock.h"
#include "config_file.h"
#include <unity.h>
#include <string>

void setUp(void) { SD.reset(); }
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Helpers

static std::string configTxt() { return SD.getFile("/config.txt"); }
static void seedFile(const char* content) { SD._fs["/config.txt"] = content; }

// A params instance with recognisable, distinct non-default values so tests
// can confirm the *current* in-memory value is what gets written, not some
// hardcoded literal.
static AmidalaParameters makeParams() {
    AmidalaParameters p;
    memset(&p, 0, sizeof(p));
    p.volumeChA = 61;
    p.volumeChB = 62;
    p.auxserial3 = true;
    p.domedecelzone = 33;
    p.domeimu = false;
    p.wifion = true;
    strncpy(p.wifiSSID, "mydroid", sizeof(p.wifiSSID));
    strncpy(p.wifiPassword, "hunter2pass", sizeof(p.wifiPassword));
    p.btcontrolleron = true;
    strncpy(p.btaddr, "AA:BB:CC:DD:EE:FF", sizeof(p.btaddr));
    p.wcbenable = true;
    p.wcboct2 = 10;
    p.wcboct3 = 20;
    strncpy(p.wcbpassword, "hunter2mesh", sizeof(p.wcbpassword));
    p.wcbquantity = 8;
    p.wcbid = 3;
    p.outboundserial = 1;
    p.mutebutton = 4;
    p.b9 = 's';
    p.dbtimeout = 275;
    return p;
}

// ---------------------------------------------------------------------------
// No file / read failure

void test_returns_false_when_config_missing() {
    AmidalaParameters p = makeParams();
    TEST_ASSERT_FALSE(ensureConfigDefaults(p));
}

// ---------------------------------------------------------------------------
// All keys already present — nothing added, values untouched

void test_no_change_when_all_keys_present() {
    const char* full =
        "volumeChA=1\nvolumeChB=2\nauxserial3=n\ndomedecelzone=5\ndomeimu=y\n"
        "wifion=n\nwifissid=other\nwifipassword=otherpass\n"
        "btcontrolleron=n\nbtaddr=\n"
        "wcbenable=n\nwcboct2=0\nwcboct3=0\nwcbpassword=\nwcbquantity=0\n"
        "wcbid=0\noutboundserial=0\n"
        "mutebutton=0\nb9=n\ndbtimeout=300\n";
    seedFile(full);
    AmidalaParameters p = makeParams();
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    TEST_ASSERT_EQUAL_STRING(full, configTxt().c_str());
}

// ---------------------------------------------------------------------------
// Missing keys get appended with the CURRENT in-memory value

void test_appends_missing_bool_key_as_y_or_n() {
    seedFile("volumeChA=1\nvolumeChB=2\nauxserial3=n\ndomedecelzone=5\ndomeimu=y\n"
             "wifion=n\nwifissid=other\nwifipassword=otherpass\n"
             "btaddr=\nmutebutton=0\nb9=n\ndbtimeout=300\n");  // btcontrolleron= missing
    AmidalaParameters p = makeParams();  // btcontrolleron = true
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string got = configTxt();
    TEST_ASSERT_TRUE(got.find("btcontrolleron=y") != std::string::npos);
}

void test_appends_missing_int_key_with_current_value() {
    seedFile("auxserial3=n\ndomeimu=y\nwifion=n\nwifissid=other\n"
             "wifipassword=otherpass\nbtcontrolleron=n\nbtaddr=\n"
             "mutebutton=0\nb9=n\ndbtimeout=300\n");  // volumeChA/B, domedecelzone missing
    AmidalaParameters p = makeParams();
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string got = configTxt();
    TEST_ASSERT_TRUE(got.find("volumeChA=61") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("volumeChB=62") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("domedecelzone=33") != std::string::npos);
}

void test_appends_missing_string_key_with_current_value() {
    seedFile("volumeChA=1\nvolumeChB=2\nauxserial3=n\ndomedecelzone=5\ndomeimu=y\n"
             "wifion=n\nbtcontrolleron=n\nbtaddr=\nmutebutton=0\nb9=n\ndbtimeout=300\n");
    AmidalaParameters p = makeParams();  // wifissid=mydroid, wifipassword=hunter2pass
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string got = configTxt();
    TEST_ASSERT_TRUE(got.find("wifissid=mydroid") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wifipassword=hunter2pass") != std::string::npos);
}

void test_appends_missing_char_key() {
    seedFile("volumeChA=1\nvolumeChB=2\nauxserial3=n\ndomedecelzone=5\ndomeimu=y\n"
             "wifion=n\nwifissid=other\nwifipassword=otherpass\n"
             "btcontrolleron=n\nbtaddr=\nmutebutton=0\ndbtimeout=300\n");  // b9= missing
    AmidalaParameters p = makeParams();  // b9 = 's'
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string got = configTxt();
    TEST_ASSERT_TRUE(got.find("b9=s") != std::string::npos);
}

void test_appends_all_when_file_has_none_of_the_keys() {
    seedFile("xbr=DEADBEEF\nvolume=50\n");  // unrelated existing content only
    AmidalaParameters p = makeParams();
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string got = configTxt();
    TEST_ASSERT_TRUE(got.find("xbr=DEADBEEF") != std::string::npos);  // preserved
    TEST_ASSERT_TRUE(got.find("volume=50") != std::string::npos);     // preserved
    TEST_ASSERT_TRUE(got.find("volumeChA=61") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("volumeChB=62") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("auxserial3=y") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("domedecelzone=33") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("domeimu=n") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wifion=y") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wifissid=mydroid") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wifipassword=hunter2pass") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("btcontrolleron=y") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("btaddr=AA:BB:CC:DD:EE:FF") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wcbenable=y") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wcboct2=10") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wcboct3=20") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wcbpassword=hunter2mesh") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wcbquantity=8") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wcbid=3") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("outboundserial=1") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("mutebutton=4") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("b9=s") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("dbtimeout=275") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Never overwrites an existing (even non-default) value

void test_does_not_overwrite_existing_value() {
    seedFile("volumeChA=1\nvolumeChB=2\nauxserial3=n\ndomedecelzone=5\ndomeimu=y\n"
             "wifion=n\nwifissid=other\nwifipassword=otherpass\n"
             "btcontrolleron=n\nbtaddr=\nmutebutton=0\nb9=n\ndbtimeout=300\n");
    AmidalaParameters p = makeParams();  // btcontrolleron = true, wifion = true, etc.
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string got = configTxt();
    // The user's deliberately-set values must survive untouched.
    TEST_ASSERT_TRUE(got.find("btcontrolleron=n") != std::string::npos);
    TEST_ASSERT_FALSE(got.find("btcontrolleron=y") != std::string::npos);
    TEST_ASSERT_TRUE(got.find("wifion=n") != std::string::npos);
    TEST_ASSERT_FALSE(got.find("wifion=y") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Commented-out keys don't count as "present"

void test_commented_key_is_still_added() {
    seedFile("# btcontrolleron=y\nvolumeChA=1\nvolumeChB=2\nauxserial3=n\n"
             "domedecelzone=5\ndomeimu=y\nwifion=n\nwifissid=other\n"
             "wifipassword=otherpass\nbtaddr=\nmutebutton=0\nb9=n\ndbtimeout=300\n");
    AmidalaParameters p = makeParams();
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string got = configTxt();
    // The commented example must be preserved...
    TEST_ASSERT_TRUE(got.find("# btcontrolleron=y") != std::string::npos);
    // ...but a real, active line must still get added since it wasn't present.
    TEST_ASSERT_TRUE(got.find("\nbtcontrolleron=y") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Idempotency — running twice doesn't duplicate lines

void test_running_twice_does_not_duplicate() {
    seedFile("xbr=DEADBEEF\n");
    AmidalaParameters p = makeParams();
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string firstPass = configTxt();
    TEST_ASSERT_TRUE(ensureConfigDefaults(p));
    std::string secondPass = configTxt();
    TEST_ASSERT_EQUAL_STRING(firstPass.c_str(), secondPass.c_str());
    // Sanity: btcontrolleron= appears exactly once, not twice.
    size_t firstIdx = secondPass.find("btcontrolleron=y");
    TEST_ASSERT_TRUE(firstIdx != std::string::npos);
    TEST_ASSERT_TRUE(secondPass.find("btcontrolleron=y", firstIdx + 1) == std::string::npos);
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_returns_false_when_config_missing);

    RUN_TEST(test_no_change_when_all_keys_present);

    RUN_TEST(test_appends_missing_bool_key_as_y_or_n);
    RUN_TEST(test_appends_missing_int_key_with_current_value);
    RUN_TEST(test_appends_missing_string_key_with_current_value);
    RUN_TEST(test_appends_missing_char_key);
    RUN_TEST(test_appends_all_when_file_has_none_of_the_keys);

    RUN_TEST(test_does_not_overwrite_existing_value);

    RUN_TEST(test_commented_key_is_still_added);

    RUN_TEST(test_running_twice_does_not_duplicate);

    return UNITY_END();
}
