// test_console.cpp
// Tests for include/console.h:
//   AmidalaConsole — construction, interface, CONSOLE_BUFFER_SIZE constant
//
// All method bodies are defined in src/console.cpp and src/controller.cpp;
// they are not exercised here.  Stubs satisfy the linker.

#include "arduino_mock.h"
#include "pin_config.h"
#include "console.h"
#include <unity.h>
#include <string.h>

// ---- Linker stubs -----------------------------------------------------------
// The real implementations live in AmidalaFirmware.ino and require
// AmidalaController to be fully defined.

void AmidalaConsole::init(AmidalaController *) {}
void AmidalaConsole::process() {}
void AmidalaConsole::processCommand(const char *) {}
bool AmidalaConsole::process(char, bool) { return false; }
void AmidalaConsole::processGesture(const char *) {}
void AmidalaConsole::process(ButtonAction &) {}
void AmidalaConsole::processButton(unsigned) {}
void AmidalaConsole::processLongButton(unsigned) {}
size_t AmidalaConsole::write(uint8_t) { return 1; }
size_t AmidalaConsole::write(const uint8_t *, size_t n) { return n; }
void AmidalaConsole::printServoPos(uint16_t) {}
void AmidalaConsole::setServo() {}
void AmidalaConsole::setDigitalOut() {}
void AmidalaConsole::outputString() {}
void AmidalaConsole::showXBEE() {}
void AmidalaConsole::printVersion() {}
void AmidalaConsole::printHelp() {}
void AmidalaConsole::monitorToggle() {}
void AmidalaConsole::monitorOutput() {}
void AmidalaConsole::tickMonitor() {}

// -----------------------------------------------------------------------------

void setUp(void) {}
void tearDown(void) {}

// ---- CONSOLE_BUFFER_SIZE constant -------------------------------------------

void test_console_buffer_size_is_64() {
    TEST_ASSERT_EQUAL(64, CONSOLE_BUFFER_SIZE);
}

// ---- Construction -----------------------------------------------------------

void test_console_constructs() {
    AmidalaConsole c;
    (void)c;
}

// ---- AmidalaConsole is a Print ----------------------------------------------

void test_console_is_print() {
    AmidalaConsole c;
    Print *p = &c;
    TEST_ASSERT_NOT_NULL(p);
}

// ---- setMinimal (inline) ----------------------------------------------------

void test_set_minimal_does_not_crash() {
    AmidalaConsole c;
    c.setMinimal(true);
    c.setMinimal(false);
}

// ---- printNum (inline) ------------------------------------------------------

// printNum writes into a local buffer via print() which calls write().
// Our stub write() is a no-op, so we just verify it doesn't crash.
void test_print_num_does_not_crash() {
    AmidalaConsole c;
    c.printNum(42);
    c.printNum(0, 6);
    c.printNum(9999, 2);
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_console_buffer_size_is_64);
    RUN_TEST(test_console_constructs);
    RUN_TEST(test_console_is_print);
    RUN_TEST(test_set_minimal_does_not_crash);
    RUN_TEST(test_print_num_does_not_crash);

    return UNITY_END();
}
