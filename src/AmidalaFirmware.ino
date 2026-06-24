// AmidalaFirmware.ino — entry point.
// Hardware globals live in src/globals.cpp.
// Controller logic lives in src/controller.cpp.

#include "debug.h"
#include "drive_config.h"
#include "controller.h"
#include <esp_ota_ops.h>

AmidalaController amidala;

void setup() {
  esp_ota_mark_app_valid_cancel_rollback();

  REELTWO_READY();

  randomSeed(analogRead(ANALOG1_PIN));

  // Drive all SPI CS pins HIGH before touching the bus so no slave sees a
  // spurious chip-select during initialisation.  GPIOs boot as floating
  // inputs; without this, SD_CS can sit LOW long enough to confuse the card.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(XBEE_CS_PIN, OUTPUT);
  digitalWrite(XBEE_CS_PIN, HIGH);
  pinMode(SPI_SPARE_CS_PIN, OUTPUT);
  digitalWrite(SPI_SPARE_CS_PIN, HIGH);

  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);
  // MISO is open-drain on most SD cards; pull it up so the line isn't
  // floating when no device is actively driving it.
  pinMode(SPI_MISO_PIN, INPUT_PULLUP);

  CONSOLE_SERIAL.begin(DEFAULT_BAUD_RATE);
  // Wait up to 3 s for USB-CDC to connect so boot log messages (including SD
  // init warnings) are visible on the monitor before SD.begin() is called.
  { uint32_t t = millis(); while (!CONSOLE_SERIAL && millis() - t < 3000) delay(10); }

#ifdef HALL_SENSOR_TEST
  pinMode(DOME_HALL_PIN, INPUT_PULLUP);
  CONSOLE_SERIAL.println("Hall sensor test — GPIO " + String(DOME_HALL_PIN) + " (move magnet past sensor)");
  for (int lastState = -1;;) {
    int state = digitalRead(DOME_HALL_PIN);
    if (state != lastState) {
      CONSOLE_SERIAL.println(state == LOW ? "LOW  <- triggered" : "HIGH <- idle");
      lastState = state;
    }
  }
#endif

  // XBee serial init omitted — XBee 3 uses SPI (see PR esp32-2).
#ifdef VMUSIC_SERIAL
  VMUSIC_SERIAL.begin(9600);
#endif
#ifdef DRIVE_SERIAL
  DRIVE_SERIAL.begin(DRIVE_BAUD_RATE, SERIAL_8N1, SERIAL0_RX_PIN, SERIAL0_TX_PIN);
#elif defined(DOME_DRIVE_SERIAL)
  DOME_DRIVE_SERIAL.begin(DRIVE_BAUD_RATE, SERIAL_8N1, SERIAL0_RX_PIN, SERIAL0_TX_PIN);
#elif defined(RDH_SERIAL)
  RDH_SERIAL.begin(RDH_BAUD_RATE, SERIAL_8N1, SERIAL0_RX_PIN, SERIAL0_TX_PIN);
#else
  SERIAL.begin(115200, SERIAL_8N1, SERIAL0_RX_PIN, SERIAL0_TX_PIN);
#endif
#if defined(ROBOCLAW_SERIAL) && DOME_DRIVE == DOME_DRIVE_ROBOCLAW
  // ROBOCLAW_SERIAL (Serial1) is separate from DRIVE_SERIAL (Serial0) and must
  // be initialized independently. Explicit pins required: ESP32-S3 UART1 has no
  // hardware-fixed pins and will not default to GPIO17/18 without them.
  ROBOCLAW_SERIAL.begin(ROBOCLAW_BAUD_RATE, SERIAL_8N1, ROBOCLAW_RX_PIN, ROBOCLAW_TX_PIN);
#endif

  // ESP32 EEPROM emulation requires an explicit begin() before any read/write.
  // Size covers highest used offset: DOME_ROBOCLAW_EEPROM_ADDR (0x200) + 8 bytes.
  EEPROM.begin(1024);

  SetupEvent::ready();
}

void loop() { AnimatedEvent::process(); }
