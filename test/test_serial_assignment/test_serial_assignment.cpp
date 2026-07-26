// test_serial_assignment.cpp
// Unit tests for include/serial_assignment.h (issue #147): the dome and
// drive serial-consuming subsystems each pick one of two physical ports
// (Serial1 / Serial2-AUX_SERIAL); the only rule is they can't both claim the
// same port when both are active in a given build. Pure logic, no Arduino
// dependency.

#include "serial_assignment.h"
#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

// ---- serialPortToString() / serialPortFromString() ------------------------

void test_port_to_string_round_trip() {
    TEST_ASSERT_EQUAL_STRING("serial1", serialPortToString(SerialPortId::kSerial1));
    TEST_ASSERT_EQUAL_STRING("serial2", serialPortToString(SerialPortId::kSerial2));

    SerialPortId p;
    TEST_ASSERT_TRUE(serialPortFromString("serial1", &p));
    TEST_ASSERT_TRUE(SerialPortId::kSerial1 == p);
    TEST_ASSERT_TRUE(serialPortFromString("serial2", &p));
    TEST_ASSERT_TRUE(SerialPortId::kSerial2 == p);
}

void test_port_from_string_rejects_garbage() {
    SerialPortId p = SerialPortId::kSerial1;
    TEST_ASSERT_FALSE(serialPortFromString("serial3", &p));
    TEST_ASSERT_FALSE(serialPortFromString("", &p));
    TEST_ASSERT_FALSE(serialPortFromString("Serial1", &p));  // case-sensitive
    // Untouched on failure.
    TEST_ASSERT_TRUE(SerialPortId::kSerial1 == p);
}

// ---- validateSerialPortChange() --------------------------------------------

void test_validate_accepts_reselecting_own_current_port() {
    // Dome re-picks Serial1 while drive (active) already holds Serial2 --
    // always valid regardless of the other consumer, since re-selection
    // never changes who owns what.
    SerialPortValidationResult r = validateSerialPortChange(
        SerialConsumer::kDome, SerialPortId::kSerial1,
        /*otherConsumerActive=*/true, SerialPortId::kSerial2);
    TEST_ASSERT_TRUE(r.ok);
}

void test_validate_rejects_when_other_active_consumer_holds_requested_port() {
    // Drive wants Serial1, but dome (active) already has it.
    SerialPortValidationResult r = validateSerialPortChange(
        SerialConsumer::kDrive, SerialPortId::kSerial1,
        /*otherConsumerActive=*/true, SerialPortId::kSerial1);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_NOT_NULL(r.reason);
}

void test_validate_allows_same_port_when_other_consumer_is_inactive() {
    // e.g. a PWM-only drive doesn't consume a port at all -- nothing for
    // dome to conflict with, even though otherConsumerPort still holds
    // whatever default/inert value it was initialized to.
    SerialPortValidationResult r = validateSerialPortChange(
        SerialConsumer::kDome, SerialPortId::kSerial2,
        /*otherConsumerActive=*/false, SerialPortId::kSerial2);
    TEST_ASSERT_TRUE(r.ok);
}

void test_validate_allows_moving_to_a_port_the_other_consumer_just_vacated() {
    // Simulates: drive used to be on Serial1 but the caller already recorded
    // its move to Serial2 before dome's change is validated -- dome should
    // now be free to take Serial1.
    SerialPortValidationResult r = validateSerialPortChange(
        SerialConsumer::kDome, SerialPortId::kSerial1,
        /*otherConsumerActive=*/true, SerialPortId::kSerial2);
    TEST_ASSERT_TRUE(r.ok);
}

void test_validate_rejects_both_consumers_on_serial2() {
    SerialPortValidationResult r = validateSerialPortChange(
        SerialConsumer::kDrive, SerialPortId::kSerial2,
        /*otherConsumerActive=*/true, SerialPortId::kSerial2);
    TEST_ASSERT_FALSE(r.ok);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_port_to_string_round_trip);
    RUN_TEST(test_port_from_string_rejects_garbage);

    RUN_TEST(test_validate_accepts_reselecting_own_current_port);
    RUN_TEST(test_validate_rejects_when_other_active_consumer_holds_requested_port);
    RUN_TEST(test_validate_allows_same_port_when_other_consumer_is_inactive);
    RUN_TEST(test_validate_allows_moving_to_a_port_the_other_consumer_just_vacated);
    RUN_TEST(test_validate_rejects_both_consumers_on_serial2);

    return UNITY_END();
}
