// arduino_mock.h
// Minimal Arduino API stubs for native (host) unit testing via PlatformIO.
// Include this BEFORE any project headers in test files.

#pragma once

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <map>
#include <string>

// ---- Arduino integer types ----
typedef bool    boolean;
typedef uint8_t byte;

// ---- Arduino character-class macros ----
#ifndef isDigit
#  define isDigit(c) isdigit((unsigned char)(c))
#endif

// ---- LOW / HIGH ----
#define LOW  0
#define HIGH 1

// ---- Analog pin constants (native test fallback values) ----
// ESP32-S3 pin_config.h uses raw GPIO numbers for analog pins (not A0/A1).
// These are kept so any test or library code that references A0-A3 compiles.
#ifndef A0
#define A0 1
#define A1 2
#define A2 3
#define A3 4
#endif

// ---- SD card chip-select (native test fallback) ----
// The mock SD.begin() ignores the pin argument; this just satisfies the
// compiler when config.h's SD path references SD_CS_PIN.
#ifndef SD_CS_PIN
#define SD_CS_PIN 0
#endif

// ---- Arduino pin mode constants ----
#define INPUT         0
#define OUTPUT        1
#define INPUT_PULLUP  2

// ---- Arduino digital I/O and timing stubs ----
static inline uint8_t digitalRead(uint8_t /*pin*/) { return LOW; }
static inline void    digitalWrite(uint8_t /*pin*/, uint8_t /*val*/) {}
static inline void    pinMode(uint8_t /*pin*/, uint8_t /*mode*/) {}
static inline uint32_t micros() { return 0; }
// mock_millis_value can be set directly in tests to control the current time.
static uint32_t mock_millis_value = 0;
static inline uint32_t millis() { return mock_millis_value; }
static inline void    delay(unsigned long /*ms*/) {}
static inline void    delayMicroseconds(unsigned int /*us*/) {}
static inline void    noInterrupts() {}
static inline void    interrupts()   {}
static inline long    random(long max)           { return (max > 0) ? (::rand() % max) : 0; }
static inline long    random(long min, long max) { return (max > min) ? (min + ::rand() % (max - min)) : min; }

// ---- EEPROM mock ----
struct MockEEPROM {
  uint8_t data[4096];
  MockEEPROM() { memset(data, 0, sizeof(data)); }
  uint8_t read(int addr)             { return (addr >= 0 && addr < (int)sizeof(data)) ? data[addr] : 0; }
  void    write(int addr, uint8_t v) { if (addr >= 0 && addr < (int)sizeof(data)) data[addr] = v; }

  // put<T>: write sizeof(T) bytes starting at addr, in memory order.
  // Matches the Arduino EEPROM.put() template behaviour (byte-by-byte, LSB first).
  template<typename T>
  const T& put(int addr, const T& t) {
    const uint8_t* ptr = (const uint8_t*)&t;
    for (int i = 0; i < (int)sizeof(T); i++)
      write(addr + i, ptr[i]);
    return t;
  }

  // get<T>: read sizeof(T) bytes starting at addr into t, in memory order.
  template<typename T>
  T& get(int addr, T& t) {
    uint8_t* ptr = (uint8_t*)&t;
    for (int i = 0; i < (int)sizeof(T); i++)
      ptr[i] = read(addr + i);
    return t;
  }

  void commit() {}  // no-op: ESP32 NVS flush not needed in tests
};
static MockEEPROM EEPROM;

// ---- Wire (I2C) mock ----
struct MockWire {
  byte endTransmissionResult = 0;  // configurable for tests
  void begin()                          {}
  void end()                            {}
  void beginTransmission(byte /*addr*/) {}
  void write(byte /*b*/)                {}
  void write(const char * /*s*/)        {}
  void setClock(uint32_t /*hz*/)        {}
  void setWireTimeout(uint32_t /*us*/, bool /*reset*/) {}
  byte endTransmission() { return endTransmissionResult; }
};
static MockWire Wire;

// ---- DEBUG macros (Reeltwo ReelTwo.h provides these in real builds) ----
#ifndef DEBUG_PRINTLN
#define DEBUG_PRINTLN(s) do {} while (0)
#define DEBUG_PRINT(s)   do {} while (0)
#endif

// ---- Arduino min / max / map() ----
// Arduino.h defines min/max as macros (not templates) so they handle
// mixed-type arguments.  Use macros here and set the core.h sentinel
// so it does not try to add template versions on top.
#ifndef _AMIDALA_MINMAX_DEFINED
#define _AMIDALA_MINMAX_DEFINED
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif
static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ---- F() macro: on native just passes the string through ----
#define F(s) (s)

// ---- Minimal Print stub (used by ButtonAction::printDescription) ----
class Print {
public:
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buf, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buf++);
    return n;
  }
  size_t print(const char *s)  { return write((const uint8_t*)s, strlen(s)); }
  size_t print(char c)         { return write((uint8_t)c); }
  size_t print(int n, int = 10) {
    char buf[32]; snprintf(buf, sizeof(buf), "%d", n);
    return print(buf);
  }
  size_t print(unsigned int n, int base = 10) {
    char buf[32];
    if (base == 16) snprintf(buf, sizeof(buf), "%x", n);
    else snprintf(buf, sizeof(buf), "%u", n);
    return print(buf);
  }
  size_t print(long n, int = 10) {
    char buf[32]; snprintf(buf, sizeof(buf), "%ld", n);
    return print(buf);
  }
  size_t print(unsigned long n, int base = 10) {
    char buf[32];
    if (base == 16) snprintf(buf, sizeof(buf), "%lx", n);
    else snprintf(buf, sizeof(buf), "%lu", n);
    return print(buf);
  }
  size_t println(const char *s) { size_t n = print(s); return n + print("\n"); }
  size_t println(char c)        { size_t n = print(c); return n + print("\n"); }
  size_t println()              { return print("\n"); }
  size_t println(int n, int b = 10)          { size_t r = print(n, b); return r + print("\n"); }
  size_t println(unsigned int n, int b = 10) { size_t r = print(n, b); return r + print("\n"); }
  size_t println(long n, int b = 10)         { size_t r = print(n, b); return r + print("\n"); }
  size_t println(unsigned long n, int b = 10){ size_t r = print(n, b); return r + print("\n"); }
};

// ---- Stream stub (Print + read-side for serial I/O) ----
class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
};

// MockStream: captures written bytes and serves pre-loaded input bytes.
class MockStream : public Stream {
public:
  char outBuf[512];
  size_t outLen;

  MockStream() : outLen(0) {
    outBuf[0] = '\0';
    inBuf[0] = '\0';
    inPos = 0;
    inLen = 0;
  }

  void reset() {
    outLen = 0; outBuf[0] = '\0';
    inPos = 0;  inLen = 0; inBuf[0] = '\0';
  }

  // Pre-load bytes that subsequent read()/available() calls will return.
  void feedInput(const char *data) {
    inLen = strlen(data);
    memcpy(inBuf, data, inLen + 1);
    inPos = 0;
  }

  // Print interface
  virtual size_t write(uint8_t c) override {
    if (outLen < sizeof(outBuf) - 1) { outBuf[outLen++] = (char)c; outBuf[outLen] = '\0'; }
    return 1;
  }

  // Stream interface
  virtual int available() override { return (int)(inLen - inPos); }
  virtual int read()      override { return (inPos < inLen) ? (uint8_t)inBuf[inPos++] : -1; }
  virtual int peek()      override { return (inPos < inLen) ? (uint8_t)inBuf[inPos]   : -1; }

private:
  char   inBuf[512];
  size_t inPos;
  size_t inLen;
};

// ---- Minimal String stub (capture output in tests) ----
class StringPrint : public Print {
public:
  char buf[512];
  size_t len;
  StringPrint() : len(0) { buf[0] = '\0'; }
  void reset() { len = 0; buf[0] = '\0'; }
  virtual size_t write(uint8_t c) override {
    if (len < sizeof(buf) - 1) { buf[len++] = (char)c; buf[len] = '\0'; }
    return 1;
  }
};

// ---- Arduino String class (std::string wrapper) ----
// Provides the subset of the Arduino String API used by project headers.
// Must appear before any header that uses String.
#include <string>
#ifndef ARDUINO  // guard: real Arduino builds have their own String
class String {
public:
  String() {}
  String(const char* s) : _s(s ? s : "") {}      // NOLINT
  String(char c) : _s(1, c) {}
  String(int v)           { char b[24]; snprintf(b,sizeof(b),"%d",  v);  _s=b; }
  String(unsigned int v)  { char b[24]; snprintf(b,sizeof(b),"%u",  v);  _s=b; }
  String(long v)          { char b[24]; snprintf(b,sizeof(b),"%ld", v);  _s=b; }
  String(unsigned long v) { char b[24]; snprintf(b,sizeof(b),"%lu", v);  _s=b; }
  String(uint8_t v)       { char b[24]; snprintf(b,sizeof(b),"%u",  (unsigned)v); _s=b; }
  String(uint16_t v)      { char b[24]; snprintf(b,sizeof(b),"%u",  (unsigned)v); _s=b; }

  bool operator==(const String& o) const { return _s == o._s; }
  bool operator!=(const String& o) const { return _s != o._s; }
  bool operator==(const char* s)   const { return _s == (s?s:""); }

  String  operator+ (const String& o) const { String r(_s.c_str()); r._s += o._s; return r; }
  String  operator+ (const char* s)   const { String r(_s.c_str()); if(s) r._s += s; return r; }
  String& operator+=(const String& o) { _s += o._s; return *this; }
  String& operator+=(const char* s)   { if(s) _s += s; return *this; }

  const char* c_str()   const { return _s.c_str(); }
  size_t      length()  const { return _s.length(); }
  bool        isEmpty() const { return _s.empty(); }

  int  indexOf(const char* s)    const { auto p=_s.find(s?s:""); return p==std::string::npos?-1:(int)p; }
  int  indexOf(const String& s)  const { return indexOf(s.c_str()); }
  int  lastIndexOf(const char* s) const {
    if (!s || !*s) return -1;
    auto p = _s.rfind(s);
    return p == std::string::npos ? -1 : (int)p;
  }
  bool startsWith(const char*   s) const { return s && _s.find(s) == 0; }
  bool startsWith(const String& s) const { return startsWith(s.c_str()); }
  bool endsWith  (const char* s) const {
    if (!s) return false;
    size_t sl = strlen(s);
    return _s.size() >= sl && _s.compare(_s.size()-sl, sl, s) == 0;
  }
  String substring(size_t from, size_t to=(size_t)-1) const {
    return String(_s.substr(from, to==(size_t)-1 ? std::string::npos : to-from).c_str());
  }
  long   toInt()             const { return atol(_s.c_str()); }
  char   charAt(size_t i)   const { return i < _s.size() ? _s[i] : '\0'; }
  void remove(size_t idx, size_t cnt=1)  { _s.erase(idx, cnt); }
  void reserve(size_t n)                 { _s.reserve(n); }
  void trim() {
    auto a=_s.find_first_not_of(" \t\r\n");
    if(a==std::string::npos){_s.clear();return;}
    auto b=_s.find_last_not_of(" \t\r\n");
    _s=_s.substr(a,b-a+1);
  }

private:
  std::string _s;
};
inline String operator+(const char* lhs, const String& rhs) {
  return String(lhs) + rhs;
}
#endif  // ARDUINO

// ---- Serial stub (used by config_reader.h) ----
struct SerialClass {
  void println(const char*) {}
  void println()            {}
  void print(const char*)   {}
  void print(char)          {}
  template<typename T> void println(T) {}
  template<typename T> void print(T)   {}
  template<typename... Args> void printf(const char*, Args...) {}
};
static SerialClass Serial;
// Serial0 is UART0 on ESP32-S3; pin_config.h defines SERIAL = Serial0.
// Alias it to Serial so native tests compile without a second stub.
static SerialClass& Serial0 = Serial;

// ---- SPI stub (used by config.h SD.begin(pin, SPI)) ----
struct MockSPIClass {};
static MockSPIClass SPI;

// ---- SD / File stubs (used by config_reader.h and config_file.h) ----
// File supports both read mode (from a const char* or std::string snapshot)
// and write mode (accumulates output into a std::string owned by MockSDClass).

class File {
public:
  File() : fPos(0), fValid(false), fWriteTarget(nullptr) {}

  // Read mode: legacy path — const char* (e.g. SD.fileContent)
  explicit File(const char* data)
    : fContent(data ? data : ""), fPos(0), fValid(data != nullptr),
      fWriteTarget(nullptr) {}

  // Read mode: from an owned std::string snapshot (map-based SD)
  explicit File(const std::string& content)
    : fContent(content), fPos(0), fValid(true), fWriteTarget(nullptr) {}

  // Write mode: output accumulates in *target (owned by MockSDClass::_fs)
  explicit File(std::string* target)
    : fPos(0), fValid(target != nullptr), fWriteTarget(target) {}

  // Copy: fWriteTarget is a raw pointer (shared); fContent is value-copied.
  File(const File& o) = default;
  File& operator=(const File& o) = default;

  explicit operator bool() const { return fValid; }

  // ---- Read interface ----
  bool available() const {
    return fValid && !fWriteTarget && fPos < fContent.size();
  }
  char read() {
    return (fValid && !fWriteTarget && fPos < fContent.size())
           ? fContent[fPos++] : '\0';
  }
  String readStringUntil(char terminator) {
    if (!fValid || fWriteTarget) return String("");
    std::string result;
    while (fPos < fContent.size() && fContent[fPos] != terminator)
      result += fContent[fPos++];
    if (fPos < fContent.size()) fPos++;  // consume terminator
    return String(result.c_str());
  }

  // ---- Write interface ----
  size_t print(const char* s) {
    if (!fWriteTarget || !s) return 0;
    *fWriteTarget += s;
    return strlen(s);
  }
  size_t print(const String& s) { return print(s.c_str()); }
  size_t println(const char* s = "") {
    size_t n = s ? print(s) : 0;
    if (fWriteTarget) fWriteTarget->push_back('\n');
    return n + 1;
  }
  size_t println(const String& s) {
    size_t n = print(s.c_str());
    if (fWriteTarget) fWriteTarget->push_back('\n');
    return n + 1;
  }

  void close() { fValid = false; }

private:
  std::string  fContent;       // read-mode buffer (owned copy)
  size_t       fPos;
  bool         fValid;
  std::string* fWriteTarget;   // non-null in write mode; points into _fs
};

struct MockSDClass {
  bool        beginResult = true;
  const char* fileContent = nullptr;      // legacy: used by 1-arg open()
  std::map<std::string, std::string> _fs; // in-memory filesystem

  bool begin(int /*pin*/)                       { return beginResult; }
  bool begin(int /*pin*/, MockSPIClass& /*spi*/){ return beginResult; }

  // 1-arg open: legacy read-only path (test_config_reader etc.)
  File open(const char* /*name*/) {
    return fileContent ? File(fileContent) : File();
  }

  // 2-arg open: "r" or "w" (matches ESP32 SD.h signature)
  File open(const char* name, const char* mode) {
    std::string n(name ? name : "");
    if (mode && mode[0] == 'r') {
      auto it = _fs.find(n);
      if (it != _fs.end()) return File(it->second);
      return fileContent ? File(fileContent) : File();
    }
    if (mode && mode[0] == 'w') {
      _fs[n] = "";
      return File(&_fs[n]);  // stable pointer: std::map doesn't invalidate on insert
    }
    return File();
  }
  File open(const String& name, const char* mode) { return open(name.c_str(), mode); }

  bool remove(const char* name) {
    return name && _fs.erase(std::string(name)) > 0;
  }
  bool remove(const String& name) { return remove(name.c_str()); }

  // Test helper: read back written content
  std::string getFile(const char* name) const {
    auto it = _fs.find(std::string(name ? name : ""));
    return it != _fs.end() ? it->second : "";
  }

  void reset() { _fs.clear(); fileContent = nullptr; beginResult = true; }
};
static MockSDClass SD;
