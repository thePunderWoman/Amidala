# Libraries

Third-party libraries are not included in this repository. Clone or download
each one into this `lib/` directory before building.

| Library | Purpose | Source |
|---|---|---|
| Reeltwo | Core robot control framework (drive, dome, servos, events) | https://github.com/reeltwo/Reeltwo |
| ReeltwoAudio | Audio playback integration for Reeltwo | https://github.com/reeltwo/ReeltwoAudio |
| HumanCyborgRelationsAPI | HCR vocalizer control (audio + emotions over I2C/serial) | https://github.com/thePunderWoman/HumanCyborgRelationsAPI (fork of roy86/HumanCyborgRelationsAPI with ESP32/IDF5 compile fixes — use tag 1.0.1 or later) |
| XBee-Arduino_library | XBee wireless remote control (ZigBee packet handling) | https://github.com/andrewrapp/xbee-arduino |
| Adafruit_NeoPixel | Addressable LED strip control (WS2812 etc.) | https://github.com/adafruit/Adafruit_NeoPixel |
| Adafruit_PWM_Servo_Driver_Library | PCA9685 I2C 16-channel PWM/servo driver | https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library |
| FastLED | Advanced LED animation library | https://github.com/FastLED/FastLED |
| LedControl | MAX7219/MAX7221 LED matrix driver | https://github.com/wayoda/LedControl |
| SlowServoPCA9685 | Smooth multi-servo movement on PCA9685 boards (Graham Short) | No public repository — include source files directly |