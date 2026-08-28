// AmidalaFirmware.ino — entry point.
// Hardware globals live in src/globals.cpp.
// Controller logic lives in src/controller.cpp.

#include "debug.h"
#include "drive_config.h"
#include "controller.h"
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>

AmidalaController amidala;

void setup() {
  // Internal DRAM is scarce (WiFi's own DMA buffers alone use ~50KB of the
  // ~140KB available), while PSRAM sits almost entirely unused by default --
  // the stock threshold below which malloc()/new always stay internal is
  // 4096 bytes, so most of WebServer/mDNS/lwIP's smaller allocations never
  // get a chance at PSRAM. Lower it so they do, freeing internal DRAM for
  // WiFi's own buffers (which can't be moved -- they require DMA-capable
  // memory) and the per-connection state it needs when a client joins.
  heap_caps_malloc_extmem_enable(64);

  esp_ota_mark_app_valid_cancel_rollback();

  REELTWO_READY();

  // Intentionally the compiled-in default, not a params.pinRole-derived
  // pin (issue #133) -- this runs before config.txt is even loaded, and
  // entropy quality doesn't depend on which ADC1 pin is sampled.
  randomSeed(analogRead(ANALOG1_PIN));

  // Drive all SPI CS pins HIGH before touching the bus so no slave sees a
  // spurious chip-select during initialisation.  GPIOs boot as floating
  // inputs; without this, SD_CS can sit LOW long enough to confuse the card.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(XBEE_CS_PIN, OUTPUT);
  digitalWrite(XBEE_CS_PIN, HIGH);
  // SPI_~ATTN is an open-drain, active-low line on the XBee3 with no
  // pull-up on the Amidala PCB (confirmed against the schematic net data --
  // see issue #185) and no other pinMode() call anywhere in the firmware.
  // Without this it floats whenever the module isn't actively driving it,
  // which reads as noise on the ESP32 side. INPUT_PULLUP is a no-op if the
  // line is actually driven, so this is safe regardless of module state.
  pinMode(XBEE_ATTN_PIN, INPUT_PULLUP);
  pinMode(SPI_SPARE_CS_PIN, OUTPUT);
  digitalWrite(SPI_SPARE_CS_PIN, HIGH);

  // MISO is open-drain on most SD cards; pull it up so the line isn't
  // floating when no device is actively driving it. Must happen BEFORE
  // SPI.begin() claims the pin -- arduino-esp32 3.x's peripheral manager
  // deinitializes a pin's existing peripheral claim (e.g. SPI_MASTER_MISO)
  // whenever pinMode() reassigns it to plain GPIO, silently disconnecting
  // MISO from the SPI controller's receive line if this runs afterward.
  pinMode(SPI_MISO_PIN, INPUT_PULLUP);
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);

  // Settle time before the XBee3 sees any SPI traffic (issue #185's cold-boot
  // report: pocket remotes intermittently never connect on power-up, but any
  // reset that doesn't remove power from the XBee -- UI or the board's reset
  // button -- fixes it immediately). The XBee3's ~RESET pin has no GPIO
  // driving it, only a passive R1/C1 RC network (10k/100nF -- see the PCB
  // schematic), giving a ~1.2ms release delay. The ESP32 reaches this point
  // in setup() well inside that same order of magnitude, so on a true cold
  // power-on both chips can be racing out of reset together, and this side
  // starts driving CS/SPI before the XBee3's own oscillator/boot sequence
  // (which needs meaningfully longer than 1.2ms) has actually finished. A
  // reset-only restart never hits this: the XBee's power, and therefore its
  // already-settled oscillator, was never interrupted.
  // 200ms is a conservative starting margin, not a spec'd number -- there's
  // no GPIO feedback from the module to know precisely when it's ready, and
  // this hasn't been confirmed against Digi's own hardware reference. Safe
  // to tune down later if field testing shows it's more than needed; it's a
  // one-time boot cost either way.
  delay(200);

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
  // DRIVE_SERIAL/DOME_DRIVE_SERIAL (AUX_SERIAL) and ROBOCLAW_SERIAL are no
  // longer opened here (issue #147) -- both are now begin()'d in
  // AmidalaController::setup() (src/controller.cpp), at the point where
  // fTankDrive/fDomeDrive are constructed, since that's the earliest point
  // config has loaded and (eventually) the port each subsystem uses becomes
  // a runtime choice rather than a fixed macro. RDH_SERIAL shares Serial0's
  // own pins by design (see pin_config.h) and is unrelated to that work, so
  // it keeps its own early begin() here; the previous plain "SERIAL" fallback
  // branch is dropped as redundant with controller.cpp's later
  // SERIAL.begin(params.serialbaud, ...) call, which fires at the correct,
  // config-loaded baud rather than this hardcoded 115200.
#ifdef RDH_SERIAL
  RDH_SERIAL.begin(RDH_BAUD_RATE, SERIAL_8N1, SERIAL0_RX_PIN, SERIAL0_TX_PIN);
#endif

  // ESP32 EEPROM emulation requires an explicit begin() before any read/write.
  // Size covers highest used offset: DOME_ROBOCLAW_EEPROM_ADDR (0x200) + 8 bytes.
  EEPROM.begin(1024);

  SetupEvent::ready();
}

void loop() { AnimatedEvent::process(); }
