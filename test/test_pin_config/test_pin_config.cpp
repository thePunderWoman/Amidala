// test_pin_config.cpp
// Compilation + value tests for include/pin_config.h on ESP32-S3.
//
// A failure here means a pin number was accidentally changed; a compile error
// means a constant was removed entirely. Both are regressions we want to catch.

#include "arduino_mock.h"
#include "pin_config.h"
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// ---- Servo output pins (4 channels on the Amidala custom PCB v1.1) -----------

void test_servo_pins() {
    TEST_ASSERT_EQUAL(3, SERVO1_PIN);
    TEST_ASSERT_EQUAL(4, SERVO2_PIN);
    TEST_ASSERT_EQUAL(5, SERVO3_PIN);
    TEST_ASSERT_EQUAL(6, SERVO4_PIN);
}

// ---- Digital output pins (4 channels, v1.1) ----------------------------------

void test_dout_pins() {
    TEST_ASSERT_EQUAL(39, DOUT1_PIN);
    TEST_ASSERT_EQUAL(40, DOUT2_PIN);
    TEST_ASSERT_EQUAL(41, DOUT3_PIN);
    TEST_ASSERT_EQUAL(42, DOUT4_PIN);
}

// ---- Dome hall-effect sensor -------------------------------------------------

void test_dome_hall_pin() {
    TEST_ASSERT_EQUAL(40, DOME_HALL_PIN);
}

// ---- PPM RC input -----------------------------------------------------------

void test_ppmin_pin() {
    TEST_ASSERT_EQUAL(47, PPMIN_PIN);
}

// ---- Analog inputs (raw GPIO numbers, not Arduino A0/A1 aliases) ------------

void test_analog_pins() {
    TEST_ASSERT_EQUAL(1, ANALOG1_PIN);
    TEST_ASSERT_EQUAL(2, ANALOG2_PIN);
}

// ---- SD card ----------------------------------------------------------------

void test_sd_cs_pin() {
    TEST_ASSERT_EQUAL(10, SD_CS_PIN);
}

// ---- SPI bus ----------------------------------------------------------------

void test_spi_pins() {
    TEST_ASSERT_EQUAL(11, SPI_MOSI_PIN);
    TEST_ASSERT_EQUAL(13, SPI_MISO_PIN);
    TEST_ASSERT_EQUAL(12, SPI_SCK_PIN);
    TEST_ASSERT_EQUAL(7,  XBEE_CS_PIN);
    TEST_ASSERT_EQUAL(14, SPI_SPARE_CS_PIN);
    TEST_ASSERT_EQUAL(16, XBEE_ATTN_PIN);
    TEST_ASSERT_EQUAL(15, XBEE_SLEEP_PIN);
}

// ---- I2C bus ----------------------------------------------------------------

void test_i2c_pins() {
    TEST_ASSERT_EQUAL(8, I2C_SDA_PIN);
    TEST_ASSERT_EQUAL(9, I2C_SCL_PIN);
}

// ---- AUX serial (Serial 3 / SW-UART header) ---------------------------------

void test_aux_serial_pins() {
    TEST_ASSERT_EQUAL(21, AUX_SERIAL_TX_PIN);
    TEST_ASSERT_EQUAL(38, AUX_SERIAL_RX_PIN);
}

// ---- SERIAL defined when drive serials absent -------------------------------

void test_serial_defined_when_drive_serials_absent() {
#ifndef SERIAL
    TEST_FAIL_MESSAGE("SERIAL must be defined when DOME_DRIVE_SERIAL and RDH_SERIAL are absent");
#else
    TEST_PASS();
#endif
}

// ---- main -------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_servo_pins);
    RUN_TEST(test_dout_pins);
    RUN_TEST(test_dome_hall_pin);
    RUN_TEST(test_ppmin_pin);
    RUN_TEST(test_analog_pins);
    RUN_TEST(test_sd_cs_pin);
    RUN_TEST(test_spi_pins);
    RUN_TEST(test_i2c_pins);
    RUN_TEST(test_aux_serial_pins);
    RUN_TEST(test_serial_defined_when_drive_serials_absent);

    return UNITY_END();
}
