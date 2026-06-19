#include "controller.h"

// isdigit(const char*, int) and atoi(const char*, int) are file-scope helpers
// used only by processCommand() for servo/digital-out command parsing.
// They are kept static to preserve internal linkage and avoid shadowing the
// standard library isdigit(int) and atoi(const char*) in other TUs.

static bool isdigit(const char *cmd, int numdigits) {
  for (int i = 0; i < numdigits; i++)
    if (!isdigit(cmd[i]))
      return false;
  return true;
}

static int atoi(const char *cmd, int numdigits) {
  int result = 0;
  for (int i = 0; i < numdigits; i++)
    result = result * 10 + (cmd[i] - '0');
  return result;
}

size_t AmidalaConsole::write(uint8_t ch) {
  return write(&ch, 1);
}

size_t AmidalaConsole::write(const uint8_t *buffer, size_t size) {
  if (fPrompt) {
    CONSOLE_SERIAL.println();
    fPrompt = false;
  }
  return CONSOLE_SERIAL.write(buffer, size);
}

void AmidalaConsole::init(AmidalaController *controller) {
  fController = controller;
  print(FIRMWARE_NAME);
  print(F(" - "));
  print(VERSION_NUM);
  print(F(" Build "));
  print(BUILD_NUM);
  print(F(" ("));
  print(BUILD_DATE);
  print(F(") Serial: "));
  println(fController->params.serial);
}

void AmidalaConsole::setDigitalOut() {
  // Not supported
  println(F("Invalid"));
}

void AmidalaConsole::outputString() {
  // Not supported
  println(F("Aux Out"));
}

void AmidalaConsole::showXBEE() {
  AmidalaParameters &params = fController->params;
  print(F("J1:"));
  print(params.xbr, HEX);
  print(F(" J2:"));
  println(params.xbl, HEX);
}

void AmidalaConsole::printVersion() {
  print(F("Amidala - "));
  print(VERSION_NUM);
  print(F(" Build "));
  print(BUILD_NUM);
  print(F(" ("));
  print(BUILD_DATE);
  print(F(") Serial: "));
  println(fController->params.serial);
}

void AmidalaConsole::printHelp() {
  printVersion();
  println();
  println(F("h - This message"));
  println(F("v - Firmware version"));
  println(F("d - Current configuration"));
  println(F("r - Random sounds on/off"));
  println(F("$ - Play sound, ${nn}[{mm}]. nn=bank, mm= file number"));
  println(F("s - Set Servo. s{nn},{mmm},{ooo}. nn=#, mmm=pos, ooo=spd"));
  println(F("o - Set DOUT. o{nn},{m}. nn=#, m=1 or 0, 2=toggle, 3=mom"));
  println(F("a - Send string to primary serial output, a[string]"));
  println(F("i - Aux i2c Command, i{nnn},{mmm}. nnn=Dest Addr, mmm=Cmd"));
  println(F("x - Display/Set XBEE address,  x{[n]=[hhhhhhhh]}. n=1 or 2, "
            "hhhhhhhh=Hex address"));
  println(F("w - Write config to EEPROM"));
  println(F("l - Load config from EEPROM"));
  println(F("c - Show config in EEPROM"));
  println(F("m - Servo Monitor Toggle on/off"));
  println();
}

void AmidalaConsole::monitorOutput() {
  AmidalaParameters &params = fController->params;
  if (!fMonitor)
    return;
  // Servos  1 2 3 4
  print(F("\e[3;5H"));
  printServoPos(0);
  print(F("\e[3;14H"));
  printServoPos(1);
  print(F("\e[3;23H"));
  printServoPos(2);
  print(F("\e[3;32H"));
  printServoPos(3);
  // Servos  5 6 7 8
  print(F("\e[4;5H"));
  printServoPos(4);
  print(F("\e[4;14H"));
  printServoPos(5);
  print(F("\e[4;23H"));
  printServoPos(6);
  print(F("\e[4;32H"));
  printServoPos(7);
  // Servos  9 10 11 12
  print(F("\e[5;5H"));
  printServoPos(8);
  print(F("\e[5;14H"));
  printServoPos(9);
  print(F("\e[5;23H"));
  printServoPos(10);
  print(F("\e[5;32H"));
  printServoPos(11);

  // Digital out status
  print(F("\e[9;4H"));
  printNum(params.D[0].state);
  print(F("\e[9;10H"));
  printNum(params.D[1].state);
  print(F("\e[9;16H"));
  printNum(params.D[2].state);
  print(F("\e[9;22H"));
  printNum(params.D[3].state);
  print(F("\e[9;28H"));
  printNum(params.D[4].state);
  print(F("\e[9;34H"));
  printNum(params.D[5].state);
  print(F("\e[9;40H"));
  printNum(params.D[6].state);
  print(F("\e[9;46H"));
  printNum(params.D[7].state);

  // Volume
  print(F("\e[11;6H"));
  printNum(fController->getVolume(), 3);
  // Sound delay
  print(F("\e[11;24H60 to 120 secs"));
  // Dome mode
  print(F("\e[11;44H"));
  printNum(fController->getDomeMode(), 1);
  // Dome position
  print(F("\e[12;6H"));
  printNum(fController->getDomePosition(), 3);
  // Dome delay
  print(F("\e[12;24H1 to 10 secs"));
  // Dome home
  print(F("\e[12;44H"));
  printNum(fController->getDomeHome(), 3);
  // Dome imu
  print(F("\e[13;6H"));
  printNum(fController->getDomeIMU(), 1);

  print(F("\e[0m"));
  print(F("\e[16;1H"));
}

void AmidalaConsole::monitorToggle() {
  fMonitor = !fMonitor;
  print(F("\ec"));
  if (!fMonitor) {
    println("Monitor Off");
    return;
  }
  print(F("\e[?25l"));
  print(F("\e[1;63H=============="));
  print(F("\e[2;63H"));
  print(FIRMWARE_NAME);
  print(' ');
  print(VERSION_NUM);
  print(F("\e[3;63H=============="));
  print(F("\e[1;1H"));
  print(F("\e[0m"));
  print(F("\e[1mServo Output:"));
  print(F("\e[3;1H1:"));
  print(F("\e[3;10H2:"));
  print(F("\e[3;19H3:"));
  print(F("\e[3;28H4:"));
  print(F("\e[4;1H5:"));
  print(F("\e[4;10H6:"));
  print(F("\e[4;19H7:"));
  print(F("\e[4;28H8:"));
  print(F("\e[5;1H9:"));
  print(F("\e[5;10H10:"));
  print(F("\e[5;19H11:"));
  print(F("\e[5;28H12:"));
  print(F("\e[1m"));
  print(F("\e[7;1HDigital Out:"));
  print(F("\e[9;1H1:"));
  print(F("\e[9;7H2:"));
  print(F("\e[9;13H3:"));
  print(F("\e[9;19H4:"));
  print(F("\e[9;25H5:"));
  print(F("\e[9;31H6:"));
  print(F("\e[9;37H7:"));
  print(F("\e[9;43H8:"));
  print(F("\e[11;1HVol:"));
  print(F("\e[11;11HSound Delay:"));
  print(F("\e[11;39HMode:"));
  print(F("\e[12;1HDome:"));
  print(F("\e[12;11HDome Delay:"));
  print(F("\e[12;39HHome:"));
  print(F("\e[13;1HIMU:"));
}

void AmidalaConsole::processCommand(const char *cmd) {
  if (cmd[1] == '\0') {
    switch (cmd[0]) {
    case '?':
    case 'h':
      printHelp();
      return;
    case 'v':
      printVersion();
      return;
    case 'd':
      fController->fConfig.showCurrentConfiguration();
      return;
    case 'r':
      fController->fAudio.randomToggle();
      return;
    case 'x':
      showXBEE();
      return;
    case 'l':
      fController->fConfig.showLoadEEPROM(true);
      return;
    case 'c':
      fController->fConfig.showLoadEEPROM(false);
      return;
    case 'm':
      monitorToggle();
      return;
    case 'w':
      fController->fConfig.writeCurrentConfiguration();
      return;
    }
  } else if (cmd[0] == '*') {
    if (!fMonitor) {
      println("Interpreting as config command");
      println(cmd + 1);
    }
    if (fController->fConfig.processConfig(cmd + 1)) {
      if (!fMonitor)
        println("Done");
    } else {
      if (!fMonitor) {
        println("Invalid Param:");
      } else {
        print("Invalid Param: ");
        println(cmd);
      }
    }
    return;
  } else if (cmd[0] == 's' && isdigit(cmd + 1, 2) && cmd[3] == ',' &&
             isdigit(cmd + 4, 3) && cmd[7] == ',' && isdigit(cmd + 8, 3) &&
             cmd[11] == '\0') {
    // Servo Command. Set Servo Position/Speed.
    // s{nn},{mmm},{ooo}. nn=Servo # (06-12), mmm=position (000-180), ooo=speed
    // (001-100)
    int snum = atoi(cmd + 1, 2);
    int sval = atoi(cmd + 4, 3);
    int sspd = atoi(cmd + 8, 3);
    println("SERVO snum=" + String(snum) + ", " + String(sval) + ", " +
            String(sspd));
    return;
  } else if (cmd[0] == 'o' && isdigit(cmd + 1, 2) && cmd[3] == ',' &&
             isdigit(cmd + 4, 1) && cmd[5] == '\0') {
    // Digital Out Command.
    // o{nn},{m}. nn=Digital Out # (01-08), m=1 (on), m=0 (off), 2=toggle,
    // 3=momentary/blink/blip
    int dout = atoi(cmd + 1, 2);
    int dval = atoi(cmd + 4, 1);
    println("DIGITAL dout=" + String(dout) + ", " + String(dval));
    return;
  } else if (cmd[0] == 'i' && isdigit(cmd + 1, 3) && cmd[4] == ',' &&
             isdigit(cmd + 5, 3) && cmd[8] == '\0') {
    // i2c Command
    // i{nnn},{mmm}. nnn=Dest Addr, mmm=Cmd
    int i2caddr = atoi(cmd + 1, 3);
    int i2ccmd = atoi(cmd + 5, 3);
    println("I2C addr=" + String(i2caddr) + ", " + String(i2ccmd));
    sendI2CCmd(i2caddr, i2ccmd);
    return;
  } else if (startswith(cmd, "autod=") && isdigit(cmd, 1) && cmd[1] == '\0') {
    int autod = atoi(cmd, 1);
    println("AUTOD=" + String(autod));
    return;
  } else if (startswith(cmd, "tmpvol=") && isdigit(cmd, 3) && cmd[3] == ',' &&
             isdigit(cmd + 4, 2)) {
    int tmpvol = atoi(cmd, 3);
    int tmpsec = atoi(cmd + 4, 2);
    println("TMPVOL=" + String(tmpvol) + " for " + String(tmpsec));
    return;
  } else if (startswith(cmd, "tmprnd=") && isdigit(cmd, 2) && cmd[2] == '\0') {
    int tmprnd = atoi(cmd, 2);
    println("TMPRND=" + String(tmprnd));
    return;
  } else if (startswith(cmd, "dp=") && isdigit(cmd, 3) && cmd[3] == '\0') {
    // set dome position
    int dp = atoi(cmd, 3);
    println("DP=" + String(dp));
    return;
  } else if (strncmp(cmd, "dome=", 5) == 0) {
    // RoboClaw dome drive commands: home, calibrate, stop, front, rand,
    // status, <N>, +<N>, -<N>
    fController->processDomeCommand(cmd + 5);
    return;
  } else if (cmd[0] == 'a') {
    print("Aux Out");
    return;
  } else if (cmd[0] == '$' && isdigit(cmd + 1, 2)) {
    // Play Sound from Sound Bank nn
    int sndbank = atoi(cmd + 1, 2);
    int snd = -1;
    if (isdigit(cmd + 3, 2) && cmd[5] == '\0') {
      // Play Sound mm from Sound Bank nn
      snd = atoi(cmd + 3, 2);
      fController->fAudio.playSound(sndbank, snd);
      return;
    } else if (cmd[3] == '\0') {
      print("SB #");
      println(sndbank);
      fController->fAudio.playSound(sndbank);
      return;
    }
  }
  println("Invalid");
}

bool AmidalaConsole::process(char ch, bool config) {
  if (ch == 0x0A || ch == 0x0D) {
    fBuffer[fPos] = '\0';
    fPos = 0;
    if (*fBuffer != '\0') {
      if (config)
        fController->fConfig.processConfig(fBuffer);
      else
        processCommand(fBuffer);
      return true;
    }
  } else if (fPos < SizeOfArray(fBuffer) - 1) {
    fBuffer[fPos++] = ch;
  }
  return false;
}

void AmidalaConsole::process() {
  monitorOutput();
  if (!fPrompt) {
    if (!fMonitor)
      print("> ");
    fPrompt = true;
  }
  if (CONSOLE_SERIAL.available()) {
    static bool reentry;
    if (reentry)
      return;
    char ch = CONSOLE_SERIAL.read();
    if (ch != '\r' && !fMonitor)
      CONSOLE_SERIAL.print(ch);
    reentry = true;
    process(ch);
    reentry = false;
  }
}
