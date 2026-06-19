#!/usr/bin/env python3
"""Local development server for the Amidala web UI.

Serves files from web/ and mocks the firmware REST API so you can preview
and iterate on the UI without flashing to hardware.

Usage:
    python3 scripts/web_dev.py
Then open http://localhost:8080 in your browser.
"""

import json
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

_HERE        = os.path.dirname(os.path.abspath(__file__))
_PROJECT_DIR = os.path.dirname(_HERE)
_WEB_DIR     = os.path.join(_PROJECT_DIR, "web")
_PORT        = 8080

# Flat config dict — mirrors buildFullConfigJson() in web_api.h.
# POST /api/config mutates this in memory.
_config = {
    # General
    "volume":        50,
    "startup":       "y",
    "rndon":         "y",
    "mindelay":      60,
    "maxdelay":      120,
    "ackon":         "n",
    "goslow":        "n",
    "mix12":         "n",
    "auto":          "n",
    "serialbaud":    9600,
    "serialdelim":   58,
    "serialeol":     13,
    "myi2c":         0,
    # WiFi
    "wifion":        "y",
    "wifissid":      "amidala",
    "wifipassword":  "Astromech",
    # XBee
    "xbr":           "00000000",
    "xbl":           "00000000",
    # Audio
    "audiohw":       "hcr",
    "volumeChA":     50,
    "volumeChB":     50,
    "volumewheel":   0,
    "altvolumewheel":0,
    "startupem":     0,
    "startuplvl":    0,
    "ackem":         0,
    "acklvl":        0,
    # RC Radio
    "rcchn":         6,
    "rcd":           30,
    "rcj":           5,
    "fst":           1000,
    "rvrmin":        0,
    "rvrmax":        1023,
    "rvlmin":        0,
    "rvlmax":        1023,
    "j1adjv":        0,
    "j1adjh":        0,
    # Dome
    "domespeed":     80,
    "domespeedhome": 50,
    "domespeedseek": 50,
    "domespeedmin":  10,
    "domedecelzone": 20,
    "domehome":      0,
    "domeflip":      "n",
    "domeimu":       "y",
    "domech6":       "n",
    "domeseekl":     90,
    "domeseekr":     90,
    "domefudge":     5,
    "domercaddr":    128,
    "domercchan":    1,
    "domercqpps":    1000,
    "domefront":     0,
    "domestall":     2000,
    # Servos — list of {min, max, n, d, t, sp, r}
    "servos": [
        {"min":  0, "max": 180, "n": 90, "d": 4, "t":  0, "sp": 50, "r": 0},
        {"min":  0, "max": 180, "n": 90, "d": 4, "t":  0, "sp": 50, "r": 0},
        {"min":  0, "max": 180, "n": 90, "d": 4, "t":  0, "sp": 50, "r": 0},
        {"min": 50, "max": 130, "n": 90, "d": 4, "t": -5, "sp": 30, "r": 1},
    ],
    # Serial strings — list of {n: name, s: serial_string}
    "sstr": [
        {"n": "Leia Sequence",  "s": ":LD00"},
        {"n": "Happy R2",       "s": ":001"},
        {"n": "Dome Home",      "s": "*dome=home"},
    ],
}

_monitor = {
    "seq": 3,
    "lines": [
        {"t": "Serial monitor initialized", "c": "info"},
        {"t": "Loaded 27 serial commands from config", "c": "info"},
        {"t": "Dome driver ready @ RoboClaw 128", "c": "info"},
    ],
}

_info = {
    "version":   "1.3",
    "board_rev": "1.1",
    "mcu":       "ESP32-S3 N16R8",
    "date":      "Jun 19 2026",
    "drive":    "roboteq-pwm",
    "dome":     "roboclaw",
    "audio":    "hcr",
    "wifi_ssid":  "amidala",
    "wifi_ip":    "192.168.4.1",
    "sstr_used":  3,
    "free_heap":  290816,
}


class _Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=_WEB_DIR, **kwargs)

    # ------------------------------------------------------------------ GET --
    def do_GET(self):
        path = urlparse(self.path).path

        if path == "/api/config":
            self._json(_config)
            return
        if path == "/api/info":
            self._json(_info)
            return
        if path == "/api/monitor":
            self._json(_monitor)
            return

        # Map extension-less paths to .html (e.g. /config/general → general.html)
        local = os.path.join(_WEB_DIR, path.lstrip("/"))
        if not os.path.exists(local) and not os.path.splitext(path)[1]:
            candidate = local + ".html"
            if os.path.exists(candidate):
                self.path = path + ".html"

        super().do_GET()

    # ----------------------------------------------------------------- POST --
    def do_POST(self):
        path   = urlparse(self.path).path
        length = int(self.headers.get("Content-Length", 0))
        body   = self.rfile.read(length).decode()
        params = {k: v[0] for k, v in parse_qs(body).items()}
        key    = params.get("key", "")
        value  = params.get("value", "")

        if path == "/update":
            print(f"  OTA    {length} bytes received — simulating flash")
            _info["version"] = "1.4"
            _info["date"]    = "Jun 19 2026"
            _monitor["lines"].append({"t": "OTA: flash complete (simulated)", "c": "info"})
            _monitor["seq"] += 1
            self._text("OK")
            return
        if path == "/api/estop":
            print("  ESTOP!")
            _monitor["lines"].append({"t": "! EMERGENCY STOP", "c": "tx"})
            _monitor["seq"] += 1
            self._text("OK")
            return
        if path == "/api/monitor":
            cmd = params.get("cmd", "")
            print(f"  SERIAL  {cmd!r}")
            _monitor["lines"].append({"t": "> " + cmd, "c": "tx"})
            _monitor["lines"].append({"t": "  (echoed by dev server)", "c": "info"})
            _monitor["seq"] += 1
            if len(_monitor["lines"]) > 32:
                _monitor["lines"] = _monitor["lines"][-32:]
            self._text("OK")
            return
        if path != "/api/config":
            self._text("Not Found", 404)
            return

        print(f"  CONFIG  {key} = {value!r}")

        # s=N,min,max,n,d,t,speed,reversed — servo channel config
        if key == "s":
            parts = value.split(",")
            if len(parts) >= 2:
                idx = int(parts[0]) - 1
                if 0 <= idx < len(_config["servos"]):
                    sv = _config["servos"][idx]
                    if len(parts) > 1: sv["min"] = int(parts[1])
                    if len(parts) > 2: sv["max"] = int(parts[2])
                    if len(parts) > 3: sv["n"]   = int(parts[3])
                    if len(parts) > 4: sv["d"]   = int(parts[4])
                    if len(parts) > 5: sv["t"]   = int(parts[5])
                    if len(parts) > 6: sv["sp"]  = int(parts[6])
                    if len(parts) > 7: sv["r"]   = int(parts[7])
        # sstr_del_N — delete serial string at index N
        if key.startswith("sstr_del_"):
            idx = int(key[9:])
            if 0 <= idx < len(_config["sstr"]):
                _config["sstr"].pop(idx)
        # sstr_N — update or append serial string at index N
        elif key.startswith("sstr_"):
            idx = int(key[5:])
            name, s = (value.split("|", 1) + [""])[:2] if "|" in value else ("", value)
            if idx == len(_config["sstr"]):
                _config["sstr"].append({"n": name, "s": s})
            elif 0 <= idx < len(_config["sstr"]):
                _config["sstr"][idx] = {"n": name, "s": s}
        elif key in _config:
            try:
                _config[key] = int(value)
            except ValueError:
                _config[key] = value
        else:
            print(f"           (unknown key — not stored)")

        self._text("OK")

    # ------------------------------------------------------------ helpers ---
    def _json(self, data: dict, status: int = 200) -> None:
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def _text(self, text: str, status: int = 200) -> None:
        body = text.encode()
        self.send_response(status)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        # Only log non-200 responses to keep the console clean
        if len(args) >= 2 and args[1] != "200":
            super().log_message(fmt, *args)


if __name__ == "__main__":
    print(f"Amidala web UI  —  dev server")
    print(f"  Serving : {_WEB_DIR}")
    print(f"  Open    : http://localhost:{_PORT}")
    print(f"  Stop    : Ctrl+C\n")
    httpd = HTTPServer(("", _PORT), _Handler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
