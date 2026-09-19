// test_xbee_at_session.cpp
// Tests for include/xbee_at_session.h / src/xbee_at_session.cpp -- the
// request/response correlation and set-then-persist chaining for reading
// and writing the XBee's own PAN ID / coordinator role over local AT
// commands (issue #213).
//
// Unlike BTGamepad/DomeController (which depend on BLE/Reeltwo classes that
// can't compile natively and so get hand-mirrored -- see test_bt_gamepad.cpp/
// test_rc_mode.cpp), XBeeATSession's only external dependency is the free
// function xbeeSPISendATCommand() (src/xbee_spi.cpp, real SPI hardware) --
// small enough to fake directly and compile the REAL src/xbee_at_session.cpp
// natively, same pattern as test_dome_drive_roboclaw.cpp. This tests the
// actual implementation, not a hand-kept-in-sync copy of it.

#include "arduino_mock.h"
#include "xbee_at_command.h"

// Fakes the one external call xbee_at_session.cpp makes -- records every
// frame it would have sent over SPI so tests can assert on it, and lets a
// test script a canned response to be delivered on the next handleResponse()
// call the test drives manually (there's no real SPI link here).
struct FakeATSend {
    int callCount = 0;
    uint8_t lastFrameId = 0;
    char lastCommand[2] = {0, 0};
    uint8_t lastParam[8] = {0};
    uint8_t lastParamLength = 0;
};
static FakeATSend gFakeSend;

void xbeeSPISendATCommand(uint8_t frameId, const char command[2],
                           const uint8_t *param, uint8_t paramLength) {
    gFakeSend.callCount++;
    gFakeSend.lastFrameId = frameId;
    gFakeSend.lastCommand[0] = command[0];
    gFakeSend.lastCommand[1] = command[1];
    gFakeSend.lastParamLength = paramLength;
    for (uint8_t i = 0; i < paramLength && i < sizeof(gFakeSend.lastParam); i++) {
        gFakeSend.lastParam[i] = param[i];
    }
}

// xbeeSPISetATSession()/xbeeSPIPumpATResponsesOnly() live in xbee_spi.cpp,
// not xbee_at_session.cpp -- provide trivial stand-ins so the linker (this
// is all one translation unit via the #include below) is satisfied without
// pulling in real SPI hardware code.
class XBeeATSession;
void xbeeSPISetATSession(XBeeATSession*) {}
void xbeeSPIPumpATResponsesOnly() {}

#include "../../src/xbee_at_session.cpp"
#include <unity.h>
#include <string.h>

void setUp(void) {
    mock_millis_value = 0;
    gFakeSend = FakeATSend{};
}
void tearDown(void) {}

// Builds a Local AT Command Response (0x88) frame body for the test to feed
// into handleResponse() -- mirrors XBeeATCommand::parseResponse()'s layout.
static void buildResponse(uint8_t *buf, uint8_t frameId, const char cmd[2],
                           uint8_t status, const uint8_t *value, uint8_t valueLen) {
    buf[0] = 0x88;
    buf[1] = frameId;
    buf[2] = (uint8_t)cmd[0];
    buf[3] = (uint8_t)cmd[1];
    buf[4] = status;
    if (valueLen) memcpy(buf + 5, value, valueLen);
}

// ---- startQuery ---------------------------------------------------------------

void test_query_sends_request_with_no_parameter_bytes() {
    XBeeATSession session;
    TEST_ASSERT_TRUE(session.startQuery("ID"));
    TEST_ASSERT_EQUAL(1, gFakeSend.callCount);
    TEST_ASSERT_EQUAL('I', gFakeSend.lastCommand[0]);
    TEST_ASSERT_EQUAL('D', gFakeSend.lastCommand[1]);
    TEST_ASSERT_EQUAL(0, gFakeSend.lastParamLength);
    TEST_ASSERT_EQUAL(XBeeATSession::kAwaitingValue, session.state());
}

void test_query_completes_on_matching_response_and_stores_value() {
    XBeeATSession session;
    session.startQuery("ID");
    uint8_t value[] = {0x41, 0x33};
    uint8_t buf[16];
    buildResponse(buf, gFakeSend.lastFrameId, "ID", XBeeATCommand::kOk, value, sizeof(value));

    session.handleResponse(buf, 5 + sizeof(value));

    TEST_ASSERT_EQUAL(XBeeATSession::kDone, session.state());
    TEST_ASSERT_EQUAL(sizeof(value), session.valueLength());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(value, session.value(), sizeof(value));
}

void test_query_fails_on_error_status() {
    XBeeATSession session;
    session.startQuery("ZZ");
    uint8_t buf[16];
    buildResponse(buf, gFakeSend.lastFrameId, "ZZ", XBeeATCommand::kInvalidCommand, nullptr, 0);

    session.handleResponse(buf, 5);

    TEST_ASSERT_EQUAL(XBeeATSession::kFailed, session.state());
    TEST_ASSERT_EQUAL(XBeeATCommand::kInvalidCommand, session.status());
}

void test_response_with_wrong_frame_id_is_ignored() {
    XBeeATSession session;
    session.startQuery("ID");
    uint8_t buf[16];
    buildResponse(buf, (uint8_t)(gFakeSend.lastFrameId + 1), "ID", XBeeATCommand::kOk, nullptr, 0);

    session.handleResponse(buf, 5);

    TEST_ASSERT_EQUAL(XBeeATSession::kAwaitingValue, session.state());  // still waiting
}

void test_second_action_rejected_while_busy() {
    XBeeATSession session;
    TEST_ASSERT_TRUE(session.startQuery("ID"));
    TEST_ASSERT_FALSE(session.startQuery("CE"));
    TEST_ASSERT_EQUAL(1, gFakeSend.callCount);  // second call never sent anything
}

void test_new_action_allowed_after_previous_one_completes() {
    XBeeATSession session;
    session.startQuery("ID");
    uint8_t buf[16];
    buildResponse(buf, gFakeSend.lastFrameId, "ID", XBeeATCommand::kOk, nullptr, 0);
    session.handleResponse(buf, 5);

    TEST_ASSERT_TRUE(session.startQuery("CE"));
    TEST_ASSERT_EQUAL(2, gFakeSend.callCount);
}

// ---- startSetAndPersist (set, then WR) -----------------------------------------

void test_set_success_follows_up_with_write() {
    XBeeATSession session;
    uint8_t value[] = {0x41, 0x33};
    session.startSetAndPersist("ID", value, sizeof(value));
    TEST_ASSERT_EQUAL(1, gFakeSend.callCount);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(value, gFakeSend.lastParam, sizeof(value));
    uint8_t setFrameId = gFakeSend.lastFrameId;

    uint8_t buf[16];
    buildResponse(buf, setFrameId, "ID", XBeeATCommand::kOk, nullptr, 0);
    session.handleResponse(buf, 5);

    // Set succeeded -- must not be done yet, WR should have been sent next.
    TEST_ASSERT_EQUAL(XBeeATSession::kAwaitingWrite, session.state());
    TEST_ASSERT_EQUAL(2, gFakeSend.callCount);
    TEST_ASSERT_EQUAL('W', gFakeSend.lastCommand[0]);
    TEST_ASSERT_EQUAL('R', gFakeSend.lastCommand[1]);
    TEST_ASSERT_TRUE(gFakeSend.lastFrameId != setFrameId);  // a fresh frame ID, not reused

    uint8_t buf2[16];
    buildResponse(buf2, gFakeSend.lastFrameId, "WR", XBeeATCommand::kOk, nullptr, 0);
    session.handleResponse(buf2, 5);

    TEST_ASSERT_EQUAL(XBeeATSession::kDone, session.state());
}

void test_set_failure_never_sends_write() {
    XBeeATSession session;
    uint8_t value[] = {0xFF};
    session.startSetAndPersist("ID", value, sizeof(value));
    uint8_t buf[16];
    buildResponse(buf, gFakeSend.lastFrameId, "ID", XBeeATCommand::kInvalidParameter, nullptr, 0);

    session.handleResponse(buf, 5);

    TEST_ASSERT_EQUAL(XBeeATSession::kFailed, session.state());
    TEST_ASSERT_EQUAL(XBeeATCommand::kInvalidParameter, session.status());
    TEST_ASSERT_EQUAL(1, gFakeSend.callCount);  // WR never sent
}

// ---- checkTimeout ---------------------------------------------------------------

void test_timeout_after_no_response() {
    XBeeATSession session;
    session.startQuery("ID");
    mock_millis_value = 3000;  // past the 2s timeout
    session.checkTimeout(mock_millis_value);
    TEST_ASSERT_EQUAL(XBeeATSession::kTimedOut, session.state());
}

void test_no_timeout_before_deadline() {
    XBeeATSession session;
    session.startQuery("ID");
    mock_millis_value = 500;
    session.checkTimeout(mock_millis_value);
    TEST_ASSERT_EQUAL(XBeeATSession::kAwaitingValue, session.state());
}

void test_timeout_is_a_noop_when_idle() {
    XBeeATSession session;
    mock_millis_value = 9999;
    session.checkTimeout(mock_millis_value);
    TEST_ASSERT_EQUAL(XBeeATSession::kIdle, session.state());
}

// ---- abort ----------------------------------------------------------------

void test_abort_returns_to_idle_regardless_of_elapsed_time() {
    // Regression: a caller with its own shorter deadline than kTimeoutMs
    // (the boot-time coordinator check) must be able to abandon a pending
    // request immediately, not wait for checkTimeout()'s own 2s window.
    XBeeATSession session;
    session.startQuery("CE");
    mock_millis_value = 1;  // barely any time has passed
    session.abort();
    TEST_ASSERT_EQUAL(XBeeATSession::kIdle, session.state());
    TEST_ASSERT_TRUE(session.startQuery("ID"));  // a fresh action works right away
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_query_sends_request_with_no_parameter_bytes);
    RUN_TEST(test_query_completes_on_matching_response_and_stores_value);
    RUN_TEST(test_query_fails_on_error_status);
    RUN_TEST(test_response_with_wrong_frame_id_is_ignored);
    RUN_TEST(test_second_action_rejected_while_busy);
    RUN_TEST(test_new_action_allowed_after_previous_one_completes);

    RUN_TEST(test_set_success_follows_up_with_write);
    RUN_TEST(test_set_failure_never_sends_write);

    RUN_TEST(test_timeout_after_no_response);
    RUN_TEST(test_no_timeout_before_deadline);
    RUN_TEST(test_timeout_is_a_noop_when_idle);

    RUN_TEST(test_abort_returns_to_idle_regardless_of_elapsed_time);

    return UNITY_END();
}
