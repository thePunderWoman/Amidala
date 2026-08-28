// debug_file_logger.h
// Persists monAppend() ring-buffer lines to a size-capped SD file while
// runtime debug-mode logging (params.debugmode, issue #199) is enabled.
//
// Independent of the USE_DEBUG compile flag (include/debug.h) et al -- those
// gate whether DEBUG_PRINT* output exists at all; debug_monitor_tee.h already
// tees it into monAppend() alongside every other monitor source (serial
// drains, console) when USE_DEBUG is on, so hooking monAppend() itself here
// (via monitor_buf.h's sink mechanism -- see monSetAppendSink()) picks up
// whatever's currently flowing through it, regardless of which compile-time
// debug flags happen to be enabled. No special-casing needed.
//
// Header-only so it compiles against both the real ESP32 SD library and
// test/arduino_mock.h's in-memory SD mock -- same pattern as config_file.h.
#pragma once

#include <stdint.h>
#include <stdio.h>

// Hard-coded ceilings so a debug session left running by accident can't fill
// the SD card (issue #199 explicitly asked for this): one 1 MiB file per
// session, and only the 5 most recent sessions kept on disk.
#ifndef DEBUG_LOG_MAX_BYTES
#define DEBUG_LOG_MAX_BYTES (1024u * 1024u)
#endif
#ifndef DEBUG_LOG_MAX_FILES
#define DEBUG_LOG_MAX_FILES 5
#endif

#define DEBUG_LOG_DIR       "/logs"
#define DEBUG_LOG_SEQ_FILE  "/logs/seq.txt"

class DebugFileLogger {
public:
  DebugFileLogger(size_t maxBytes = DEBUG_LOG_MAX_BYTES,
                   uint8_t maxFiles = DEBUG_LOG_MAX_FILES)
    : fMaxBytes(maxBytes), fMaxFiles(maxFiles) {}

  bool isEnabled() const { return fEnabled; }

  // Starts (true) or stops (false) a logging session. Idempotent -- calling
  // with the value already in effect is a no-op, so both setup()'s
  // boot-time restore and cfg_debugmode()'s live toggle can call this
  // unconditionally.
  void setEnabled(bool on) {
    if (on == fEnabled) return;
    if (on) startSession();
    else stopSession();
  }

  // Called for every line that reaches monAppend(). No-op unless a session
  // is active and still under its size cap.
  void writeLine(const char* text, char cls) {
    if (!fEnabled || fCapped || !fFile) return;
    String line = String(cls) + " " + text;
    fBytesWritten += fFile.println(line);
    fFile.flush();
    if (fBytesWritten >= fMaxBytes) {
      fFile.println("*** log file size limit reached; further output discarded until debug mode is toggled off and back on ***");
      fFile.flush();
      fCapped = true;
    }
  }

  // Path of the file the current (or most recently started) session wrote
  // to. Empty if no session has ever started this boot.
  const String& currentFile() const { return fPath; }

private:
  void startSession() {
    SD.mkdir(DEBUG_LOG_DIR);
    uint32_t index = nextIndex();
    char path[40];
    snprintf(path, sizeof(path), "%s/debug_%05lu.log", DEBUG_LOG_DIR, (unsigned long)index);
    fPath = path;
    fFile = SD.open(path, "w");
    fEnabled = (bool)fFile;
    fBytesWritten = 0;
    fCapped = false;
    if (fEnabled) {
      fFile.println("=== debug log session start ===");
      fFile.flush();
    }
    // Retention: drop the file fMaxFiles sessions back, if it exists.
    if (index > fMaxFiles) {
      char oldPath[40];
      snprintf(oldPath, sizeof(oldPath), "%s/debug_%05lu.log", DEBUG_LOG_DIR,
                (unsigned long)(index - fMaxFiles));
      SD.remove(oldPath);
    }
  }

  void stopSession() {
    if (fFile) fFile.close();
    fEnabled = false;
  }

  // Reads, increments, and persists a monotonic session counter at
  // DEBUG_LOG_SEQ_FILE. Returns 1 (and (re)creates the seq file) the first
  // time this is ever called on a fresh/wiped SD card.
  uint32_t nextIndex() {
    uint32_t idx = 1;
    File f = SD.open(DEBUG_LOG_SEQ_FILE, "r");
    if (f) {
      String s = f.readStringUntil('\n');
      f.close();
      long v = s.toInt();
      if (v > 0) idx = (uint32_t)v;
    }
    SD.remove(DEBUG_LOG_SEQ_FILE);
    File wf = SD.open(DEBUG_LOG_SEQ_FILE, "w");
    if (wf) {
      wf.println(String(idx + 1));
      wf.close();
    }
    return idx;
  }

  File     fFile;
  String   fPath;
  bool     fEnabled = false;
  bool     fCapped = false;
  size_t   fBytesWritten = 0;
  size_t   fMaxBytes;
  uint8_t  fMaxFiles;
};

inline DebugFileLogger& debugFileLogger() {
  static DebugFileLogger inst;
  return inst;
}

// monitor_buf.h sink adapter -- see monSetAppendSink().
inline void debugFileLoggerSink(const char* text, char cls) {
  debugFileLogger().writeLine(text, cls);
}
