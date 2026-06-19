// config_file.h
// SD card config.txt write-back helpers.
//
// Header-only so it compiles in both the firmware (SD.h from the ESP32 Arduino
// framework) and native unit tests (SD / File from test/arduino_mock.h).
//
// Nothing here depends on WiFi, WebServer, or controller state — all it needs
// is an SD global that provides open(path, mode) / remove(path) and a File
// type with available() / readStringUntil() / print() / close().

#pragma once

// ---------------------------------------------------------------------------
// updateConfigFile — update or insert a single key=value line in /config.txt.
//
// Behaviour:
//   - If the file does not exist, creates a minimal one: #START / key=value / #END
//   - If the file exists and contains an uncommented "key=" line, replaces it.
//   - If the key is not present, inserts key=value immediately before the
//     last "#END" marker (or appends if there is no #END).
//   - Lines starting with '#' or '//' are never treated as key matches.
//   - Windows-style \r\n endings are normalised (trailing \r stripped).
//
// Returns false only on SD write failure.
// ---------------------------------------------------------------------------
inline bool updateConfigFile(const char* key, const char* value) {
    String path    = "/config.txt";
    String prefix  = String(key) + "=";
    String newLine = prefix + String(value);

    File f = SD.open(path, "r");
    if (!f) {
        File wf = SD.open(path, "w");
        if (!wf) return false;
        wf.println("#START");
        wf.println(newLine);
        wf.println("#END");
        wf.close();
        return true;
    }

    String out;
    out.reserve(8192);
    bool found = false;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.endsWith("\r")) line.remove(line.length() - 1);

        if (!line.startsWith("#") && !line.startsWith("//") && line.startsWith(prefix)) {
            out += newLine + "\n";
            found = true;
        } else {
            out += line + "\n";
        }
    }
    f.close();

    if (!found) {
        int endIdx = out.lastIndexOf("#END");
        if (endIdx >= 0)
            out = out.substring(0, endIdx) + newLine + "\n" + out.substring(endIdx);
        else
            out += newLine + "\n";
    }

    SD.remove(path);
    File wf = SD.open(path, "w");
    if (!wf) return false;
    wf.print(out);
    wf.close();
    return true;
}
