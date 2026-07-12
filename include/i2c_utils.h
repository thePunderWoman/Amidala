// i2c_utils.h
// I2C bus recovery and safe transmission helpers for Amidala Firmware.
//
// recoverI2CBus() — bit-bangs the SCL line to release a stuck slave, then
//                   reinitialises Wire at 100 kHz.
// sendI2CCmd()    — transmits a single command byte; calls recoverI2CBus()
//                   and prints a diagnostic on error.
// sendI2CStr()    — transmits a null-terminated string; same error handling.
//
// Depends on: Wire (Arduino <Wire.h> / arduino_mock.h),
//             I2C_SDA_PIN / I2C_SCL_PIN (pin_config.h),
//             pinMode / digitalWrite / digitalRead / delayMicroseconds / delay
//             (Arduino / arduino_mock.h),
//             Serial (Arduino / arduino_mock.h), F() macro

#pragma once

#include <stdint.h>

inline void recoverI2CBus() {
  Wire.end();
  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, OUTPUT);
  // Clock out up to 9 bits to release a stuck slave
  for (int i = 0; i < 9; i++) {
    digitalWrite(I2C_SCL_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(5);
    if (digitalRead(I2C_SDA_PIN) == HIGH)
      break;
  }
  // Generate a STOP condition
  pinMode(I2C_SDA_PIN, OUTPUT);
  digitalWrite(I2C_SDA_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(I2C_SCL_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(I2C_SDA_PIN, HIGH);
  delayMicroseconds(5);
  Wire.begin();
  Wire.setClock(100000L);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);
#endif
}

inline uint8_t sendI2CCmd(uint8_t addr, uint8_t cmd) {
  Wire.beginTransmission(addr);
  Wire.write(cmd);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    Serial.print(F("I2C err "));
    Serial.print(err);
    Serial.print(F(" @"));
    Serial.println(addr);
    recoverI2CBus();
  }
  delay(5);
  return err;
}

inline uint8_t sendI2CStr(uint8_t addr, const char *str) {
  Wire.beginTransmission(addr);
  Wire.write(str);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    Serial.print(F("I2C err "));
    Serial.print(err);
    Serial.print(F(" @"));
    Serial.println(addr);
    recoverI2CBus();
  }
  delay(5);
  return err;
}
