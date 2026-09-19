// console_prompt.h
// Bookkeeping for AmidalaConsole's interactive "> " prompt.
//
// The prompt only means something on a real terminal, so it is written to the
// terminal stream it is handed and nowhere else. In particular it must never go
// through AmidalaConsole::write(), which tees everything into the web Monitor
// as "LOG: " lines: the prompt is re-shown every time the console goes idle, so
// teeing it left a standalone "LOG: > " line after every burst of output.
// Taking only a terminal Print& makes that impossible by construction, and
// keeps the logic testable natively (console.cpp isn't compiled there).
//
// Depends on: Print (Arduino / arduino_mock.h)

#pragma once

class ConsolePrompt {
public:
  // Idle tick. Shows the prompt on `terminal` -- once per burst of output, not
  // on every tick -- unless `quiet` (the ANSI status monitor owns the screen).
  void idle(Print &terminal, bool quiet) {
    if (fShown) return;
    if (!quiet) terminal.print("> ");
    fShown = true;
  }

  // Call before any other output reaches the console: ends the prompt's line on
  // `terminal` so the output starts on a fresh line, and re-arms the prompt.
  void beforeOutput(Print &terminal) {
    if (fShown) {
      terminal.println();
      fShown = false;
    }
  }

private:
  bool fShown = false;
};
