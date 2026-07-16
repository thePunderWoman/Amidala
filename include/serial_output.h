#pragma once
// Lightweight serial output helpers — depend only on Print (Arduino / mock).
// Kept in a separate header so they can be unit-tested without pulling in
// the full AmidalaController / ReelTwo dependency chain.

// writeEolTo: write the configured EOL byte(s) to a Print destination.
// eol == 0 is the CRLF sentinel (\r\n); any other value writes that byte.
inline void writeEolTo(Print& out, uint8_t eol) {
  if (eol == 0) {
    out.write('\r');
    out.write('\n');
  } else {
    out.write(eol);
  }
}

// sendSerialStringTo: write str to out, replacing every occurrence of delim
// with the configured EOL, then append the EOL at the end.
inline void sendSerialStringTo(Print& out, const char* str,
                                uint8_t delim, uint8_t eol) {
  char ch;
  while ((ch = *str++) != '\0') {
    if (ch == (char)delim)
      writeEolTo(out, eol);
    else
      out.write((uint8_t)ch);
  }
  writeEolTo(out, eol);
}

// splitOnDelimiter: fills `segments` with the pieces of `str` cut on every
// occurrence of `delim` -- the same compound-command convention
// sendSerialStringTo() implements for a byte-stream Print destination
// (e.g. "DM:ALARM|:PL4:PP100:PR30:PW20:PH" split on '|' into two pieces),
// but for transports that send one complete command per call instead
// (e.g. WCB_Client::broadcast()), where there's no stream to write EOLs
// into. Unlike sendSerialStringTo(), empty segments (leading/trailing/
// consecutive delimiters) are skipped rather than preserved as empty
// writes -- broadcasting an empty command has no UART-line equivalent and
// would just waste a mesh packet. Returns the number of segments written;
// extra segments beyond maxSegments, or characters beyond segLen - 1 within
// one segment, are silently dropped (callers size these generously against
// their known max input length).
inline uint8_t splitOnDelimiter(const char* str, uint8_t delim,
                                 char* segments, uint8_t segLen,
                                 uint8_t maxSegments) {
  uint8_t count = 0;
  uint8_t pos = 0;
  for (const char* p = str; ; p++) {
    char ch = *p;
    bool atEnd = (ch == '\0');
    if (atEnd || ch == (char)delim) {
      if (pos > 0 && count < maxSegments) {
        segments[count * segLen + pos] = '\0';
        count++;
      }
      pos = 0;
      if (atEnd) break;
    } else if (count < maxSegments && pos < segLen - 1) {
      segments[count * segLen + pos++] = ch;
    }
  }
  return count;
}
