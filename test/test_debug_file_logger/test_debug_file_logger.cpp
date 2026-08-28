// test_debug_file_logger.cpp
// Unit tests for DebugFileLogger (include/debug_file_logger.h) -- the SD
// log-file writer behind the runtime debug-mode toggle (issue #199).
//
// The SD mock (arduino_mock.h) provides an in-memory filesystem via
// MockSDClass::_fs so these run without hardware, same as
// test_config_file/test_ensure_config_defaults.

#include "arduino_mock.h"
#include "debug_file_logger.h"
#include <unity.h>
#include <string>

void setUp(void) { SD.reset(); }
void tearDown(void) {}

static std::string fileAt(const char* path) { return SD.getFile(path); }

// ---------------------------------------------------------------------------
// Enable / disable

void test_disabled_by_default() {
    DebugFileLogger logger;
    TEST_ASSERT_FALSE(logger.isEnabled());
}

void test_current_file_empty_before_any_session() {
    DebugFileLogger logger;
    TEST_ASSERT_EQUAL_STRING("", logger.currentFile().c_str());
}

void test_enable_creates_session_file_with_header() {
    DebugFileLogger logger;
    logger.setEnabled(true);
    TEST_ASSERT_TRUE(logger.isEnabled());
    TEST_ASSERT_EQUAL_STRING("/logs/debug_00001.log", logger.currentFile().c_str());
    std::string content = fileAt("/logs/debug_00001.log");
    TEST_ASSERT_TRUE(content.find("=== debug log session start ===") != std::string::npos);
}

void test_enable_twice_is_idempotent() {
    DebugFileLogger logger;
    logger.setEnabled(true);
    logger.writeLine("first", 'i');
    logger.setEnabled(true);  // already on -- must not start a second session
    std::string content = fileAt("/logs/debug_00001.log");
    size_t first = content.find("session start");
    TEST_ASSERT_TRUE(first != std::string::npos);
    TEST_ASSERT_TRUE(content.find("session start", first + 1) == std::string::npos);
}

void test_disable_stops_capture() {
    DebugFileLogger logger;
    logger.setEnabled(true);
    logger.setEnabled(false);
    TEST_ASSERT_FALSE(logger.isEnabled());
    logger.writeLine("dropped", 'i');
    TEST_ASSERT_TRUE(fileAt("/logs/debug_00001.log").find("dropped") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Writing lines

void test_writeLine_noop_when_never_enabled() {
    DebugFileLogger logger;
    logger.writeLine("should not appear", 'i');
    TEST_ASSERT_TRUE(SD._fs.empty());
}

void test_writeLine_appends_line_with_class_prefix() {
    DebugFileLogger logger;
    logger.setEnabled(true);
    logger.writeLine("hello world", 't');
    std::string content = fileAt("/logs/debug_00001.log");
    TEST_ASSERT_TRUE(content.find("t hello world") != std::string::npos);
}

void test_writeLine_multiple_lines_appear_in_order() {
    DebugFileLogger logger;
    logger.setEnabled(true);
    logger.writeLine("one", 'i');
    logger.writeLine("two", 'i');
    std::string content = fileAt("/logs/debug_00001.log");
    TEST_ASSERT_TRUE(content.find("one") < content.find("two"));
}

// ---------------------------------------------------------------------------
// Session numbering -- persisted via /logs/seq.txt so it survives both a
// disable/re-enable cycle and a brand new DebugFileLogger instance (i.e. a
// real firmware restart, which always constructs a fresh singleton).

void test_second_session_gets_incremented_filename() {
    DebugFileLogger logger;
    logger.setEnabled(true);
    logger.setEnabled(false);
    logger.setEnabled(true);
    TEST_ASSERT_EQUAL_STRING("/logs/debug_00002.log", logger.currentFile().c_str());
}

void test_sequence_counter_persists_across_logger_instances() {
    {
        DebugFileLogger logger;
        logger.setEnabled(true);
    }
    DebugFileLogger logger2;
    logger2.setEnabled(true);
    TEST_ASSERT_EQUAL_STRING("/logs/debug_00002.log", logger2.currentFile().c_str());
}

// ---------------------------------------------------------------------------
// Size cap (issue #199: bound SD usage so a forgotten session can't fill
// the card) -- constructor overrides let the test use a tiny threshold
// instead of the real 1 MiB default.

void test_size_cap_stops_writing_and_appends_marker() {
    DebugFileLogger logger(/*maxBytes=*/20, /*maxFiles=*/5);
    logger.setEnabled(true);
    for (int i = 0; i < 10; i++) logger.writeLine("0123456789", 'i');
    std::string content = fileAt(logger.currentFile().c_str());
    TEST_ASSERT_TRUE(content.find("size limit reached") != std::string::npos);
}

void test_size_cap_further_writes_are_dropped() {
    DebugFileLogger logger(/*maxBytes=*/20, /*maxFiles=*/5);
    logger.setEnabled(true);
    for (int i = 0; i < 10; i++) logger.writeLine("0123456789", 'i');
    size_t sizeAtCap = fileAt(logger.currentFile().c_str()).size();
    logger.writeLine("more data that should be dropped entirely", 'i');
    TEST_ASSERT_EQUAL_INT((int)sizeAtCap, (int)fileAt(logger.currentFile().c_str()).size());
}

void test_size_cap_does_not_trigger_before_threshold() {
    DebugFileLogger logger(/*maxBytes=*/10000, /*maxFiles=*/5);
    logger.setEnabled(true);
    logger.writeLine("short line", 'i');
    std::string content = fileAt(logger.currentFile().c_str());
    TEST_ASSERT_TRUE(content.find("size limit reached") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Retention (issue #199: bound total SD usage, not just per-file)

void test_retention_deletes_oldest_beyond_max_files() {
    DebugFileLogger logger(DEBUG_LOG_MAX_BYTES, /*maxFiles=*/2);
    logger.setEnabled(true); logger.setEnabled(false);  // #1
    logger.setEnabled(true); logger.setEnabled(false);  // #2
    logger.setEnabled(true); logger.setEnabled(false);  // #3 -- must delete #1
    TEST_ASSERT_TRUE(SD._fs.find("/logs/debug_00001.log") == SD._fs.end());
    TEST_ASSERT_TRUE(SD._fs.find("/logs/debug_00002.log") != SD._fs.end());
    TEST_ASSERT_TRUE(SD._fs.find("/logs/debug_00003.log") != SD._fs.end());
}

void test_retention_keeps_files_within_limit() {
    DebugFileLogger logger(DEBUG_LOG_MAX_BYTES, /*maxFiles=*/5);
    for (int i = 0; i < 3; i++) { logger.setEnabled(true); logger.setEnabled(false); }
    TEST_ASSERT_TRUE(SD._fs.find("/logs/debug_00001.log") != SD._fs.end());
    TEST_ASSERT_TRUE(SD._fs.find("/logs/debug_00002.log") != SD._fs.end());
    TEST_ASSERT_TRUE(SD._fs.find("/logs/debug_00003.log") != SD._fs.end());
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_disabled_by_default);
    RUN_TEST(test_current_file_empty_before_any_session);
    RUN_TEST(test_enable_creates_session_file_with_header);
    RUN_TEST(test_enable_twice_is_idempotent);
    RUN_TEST(test_disable_stops_capture);

    RUN_TEST(test_writeLine_noop_when_never_enabled);
    RUN_TEST(test_writeLine_appends_line_with_class_prefix);
    RUN_TEST(test_writeLine_multiple_lines_appear_in_order);

    RUN_TEST(test_second_session_gets_incremented_filename);
    RUN_TEST(test_sequence_counter_persists_across_logger_instances);

    RUN_TEST(test_size_cap_stops_writing_and_appends_marker);
    RUN_TEST(test_size_cap_further_writes_are_dropped);
    RUN_TEST(test_size_cap_does_not_trigger_before_threshold);

    RUN_TEST(test_retention_deletes_oldest_beyond_max_files);
    RUN_TEST(test_retention_keeps_files_within_limit);

    return UNITY_END();
}
