// test_wcb_status_json.cpp
// Unit tests for buildWCBStatusJson() (include/wcb_status_json.h).
//
// Deliberately header-only with zero WCB_Client/ESP-NOW dependency, unlike
// WCBClientController itself (which transitively pulls in real WiFi/ESP-NOW
// headers via WCB_Client.h and can't compile under env:native at all).

#include "arduino_mock.h"
#include "wcb_status_json.h"
#include <string.h>
#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

static bool contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != nullptr;
}

void test_disabled_state() {
    String json = buildWCBStatusJson(false, false, false, false, 0, 0, 0, false);
    const char *s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"enabled\":false"));
    TEST_ASSERT_TRUE(contains(s, "\"configured\":false"));
    TEST_ASSERT_TRUE(contains(s, "\"running\":false"));
    TEST_ASSERT_TRUE(contains(s, "\"joined\":false"));
    TEST_ASSERT_TRUE(contains(s, "\"neighbor_count\":0"));
    TEST_ASSERT_TRUE(contains(s, "\"reboot_required\":false"));
}

void test_enabled_but_not_configured() {
    // enabled=true, configured=false -- the item-4 "enabled but incomplete
    // identity" state. Must never claim running/joined even though enabled.
    String json = buildWCBStatusJson(true, false, false, false, 0, 0, 0, false);
    const char *s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"enabled\":true"));
    TEST_ASSERT_TRUE(contains(s, "\"configured\":false"));
    TEST_ASSERT_TRUE(contains(s, "\"running\":false"));
}

void test_running_but_no_neighbors() {
    // begin() succeeded but nobody else is on the mesh yet -- "running" must
    // be true while "joined" stays false (neighborCount() == 0 is the
    // honest signal, not just begin()==true; see the design decision in
    // the plan).
    String json = buildWCBStatusJson(true, true, true, false, 0, 5, 8, false);
    const char *s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"running\":true"));
    TEST_ASSERT_TRUE(contains(s, "\"joined\":false"));
    TEST_ASSERT_TRUE(contains(s, "\"neighbor_count\":0"));
}

void test_fully_joined() {
    String json = buildWCBStatusJson(true, true, true, true, 3, 5, 8, false);
    const char *s = json.c_str();
    TEST_ASSERT_TRUE(contains(s, "\"joined\":true"));
    TEST_ASSERT_TRUE(contains(s, "\"neighbor_count\":3"));
    TEST_ASSERT_TRUE(contains(s, "\"device_id\":5"));
    TEST_ASSERT_TRUE(contains(s, "\"quantity\":8"));
}

void test_reboot_required_flag() {
    String json = buildWCBStatusJson(true, true, true, true, 3, 5, 8, true);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"reboot_required\":true"));
}

void test_special_id_20() {
    String json = buildWCBStatusJson(true, true, true, true, 1, 20, 8, false);
    TEST_ASSERT_TRUE(contains(json.c_str(), "\"device_id\":20"));
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_disabled_state);
    RUN_TEST(test_enabled_but_not_configured);
    RUN_TEST(test_running_but_no_neighbors);
    RUN_TEST(test_fully_joined);
    RUN_TEST(test_reboot_required_flag);
    RUN_TEST(test_special_id_20);

    return UNITY_END();
}
