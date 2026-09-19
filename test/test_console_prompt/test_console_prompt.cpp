// test_console_prompt.cpp
// Tests for console_prompt.h: the interactive "> " prompt must reach the
// terminal only, never the web Monitor tee (which showed up as a standalone
// "LOG: > " line after every burst of console output).

#include "arduino_mock.h"
#include "console_prompt.h"
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

// Hand-mirror of AmidalaConsole::write()/process()'s wiring (console.cpp isn't
// compiled natively): output goes to the terminal and, unless the ANSI status
// monitor is up, the tee; the prompt goes through ConsolePrompt to the terminal.
struct ConsoleHarness {
  ConsolePrompt prompt;
  MockStream terminal;
  MockStream tee;  // stands in for the web Monitor's "LOG: " mirror
  bool monitor = false;

  void write(const char *s) {
    prompt.beforeOutput(terminal);
    if (!monitor) tee.print(s);
    terminal.print(s);
  }
  void idleTick() { prompt.idle(terminal, monitor); }
};

// ---- ConsolePrompt ----------------------------------------------------------

void test_idle_shows_prompt_on_terminal() {
  ConsolePrompt p;
  MockStream terminal;
  p.idle(terminal, false);
  TEST_ASSERT_EQUAL_STRING("> ", terminal.outBuf);
}

void test_idle_shows_prompt_only_once_per_burst() {
  ConsolePrompt p;
  MockStream terminal;
  p.idle(terminal, false);
  p.idle(terminal, false);
  p.idle(terminal, false);
  TEST_ASSERT_EQUAL_STRING("> ", terminal.outBuf);
}

void test_output_ends_the_prompt_line_and_rearms_it() {
  ConsolePrompt p;
  MockStream terminal;
  p.idle(terminal, false);
  p.beforeOutput(terminal);
  TEST_ASSERT_NOT_NULL(strchr(terminal.outBuf, '\n'));  // prompt line ended
  size_t after = terminal.outLen;
  p.beforeOutput(terminal);                              // nothing further to end
  TEST_ASSERT_EQUAL(after, terminal.outLen);
  p.idle(terminal, false);                               // and the prompt is back
  TEST_ASSERT_EQUAL_STRING("> ", terminal.outBuf + after);
}

void test_before_output_with_no_prompt_shown_writes_nothing() {
  ConsolePrompt p;
  MockStream terminal;
  p.beforeOutput(terminal);
  TEST_ASSERT_EQUAL(0, (int)terminal.outLen);
}

void test_quiet_idle_suppresses_prompt_text() {
  ConsolePrompt p;
  MockStream terminal;
  p.idle(terminal, true);
  TEST_ASSERT_EQUAL(0, (int)terminal.outLen);
}

// ---- Regression: the prompt never reaches the monitor tee -------------------

void test_prompt_never_reaches_the_monitor_tee() {
  ConsoleHarness c;
  c.write("Processing Button 7\n");
  c.idleTick();  // the console goes idle after logging -> prompt
  TEST_ASSERT_EQUAL_STRING("Processing Button 7\n", c.tee.outBuf);
  TEST_ASSERT_NULL(strstr(c.tee.outBuf, ">"));
  // ...while the terminal still gets it.
  TEST_ASSERT_EQUAL_STRING("Processing Button 7\n> ", c.terminal.outBuf);
}

void test_repeated_bursts_leave_the_tee_free_of_prompts() {
  ConsoleHarness c;
  for (int i = 0; i < 5; i++) {
    c.write("line\n");
    c.idleTick();
    c.idleTick();
  }
  TEST_ASSERT_NULL(strstr(c.tee.outBuf, ">"));
  TEST_ASSERT_EQUAL(5 * (int)strlen("line\n"), (int)c.tee.outLen);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_shows_prompt_on_terminal);
  RUN_TEST(test_idle_shows_prompt_only_once_per_burst);
  RUN_TEST(test_output_ends_the_prompt_line_and_rearms_it);
  RUN_TEST(test_before_output_with_no_prompt_shown_writes_nothing);
  RUN_TEST(test_quiet_idle_suppresses_prompt_text);
  RUN_TEST(test_prompt_never_reaches_the_monitor_tee);
  RUN_TEST(test_repeated_bursts_leave_the_tee_free_of_prompts);
  return UNITY_END();
}
