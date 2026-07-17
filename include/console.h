// console.h
// AmidalaConsole: serial command console for Amidala Firmware.
//
// Class declaration only — all method bodies are defined in src/console.cpp
// and src/buttons.cpp (they call back into the controller).
//
// Depends on: Print (Arduino / arduino_mock.h),
//             button_actions.h (ButtonAction)

#pragma once

#include "button_actions.h"

// ---- Console input buffer size ----------------------------------------------

#ifndef CONSOLE_BUFFER_SIZE
#define CONSOLE_BUFFER_SIZE 64
#endif

// ---- AmidalaConsole ---------------------------------------------------------

class AmidalaController;

class AmidalaConsole : public Print {
public:
  AmidalaConsole() : fPos(0) {}
  virtual ~AmidalaConsole() {}

  void init(AmidalaController *controller);
  void process();
  bool processConfig(const char *cmd);
  void processCommand(const char *cmd);
  bool process(char ch, bool config = false);

  void processGesture(const char *gesture);
  void process(ButtonAction &button);
  void processButton(unsigned num);
  void processLongButton(unsigned num);
  void processAltButton(unsigned num);
  void processDoubleButton(unsigned num);

  // write() uses CONSOLE_SERIAL, defined in pin_config.h.
  virtual size_t write(uint8_t ch) override;
  virtual size_t write(const uint8_t *buffer, size_t size) override;

  bool isMinimal() const { return fMinimal; }

  void printNum(unsigned num, unsigned width = 4) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", num);
    for (size_t len = strlen(buf); len < width;) {
      buf[len++] = ' ';
      buf[len] = '\0';
    }
    print(buf);
  }

  // printServoPos() uses servoDispatch (extern global from src/globals.cpp).
  // Body is in src/servo.cpp.
  void printServoPos(uint16_t num);

  void setServo();
  void setDigitalOut();
  void outputString();
  void showXBEE();
  void printVersion();
  void printHelp();
  void monitorToggle();
  void monitorOutput();
  void setMinimal(bool minimal) { fMinimal = minimal; }

  // Flushes a trailing partial line teed into the web serial monitor (see
  // write()) after a pause -- most console output ends in println(), but a
  // few call sites (e.g. the "a" command's "Aux Out") don't, so without this
  // periodic tick that text would just sit buffered and never show up.
  // Called from AmidalaWiFiAP::handle(), alongside the S0/S1/S2 port ticks.
  void tickMonitor();

private:
  AmidalaController *fController = nullptr;
  unsigned fPos;
  void processButtonLayer(unsigned num, const char *label,
                          ButtonAction *actions);
  char fBuffer[CONSOLE_BUFFER_SIZE];
  bool fMonitor = false;
  bool fPrompt = false;
  bool fMinimal = true;
};
