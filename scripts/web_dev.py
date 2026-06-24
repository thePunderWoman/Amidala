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
import re
from http.server import HTTPServer, SimpleHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

_HERE        = os.path.dirname(os.path.abspath(__file__))
_PROJECT_DIR = os.path.dirname(_HERE)
_WEB_DIR     = os.path.join(_PROJECT_DIR, "web")
_PORT        = 8080


# ---------------------------------------------------------------------------
# example_config.txt parser
# Mirrors the firmware's processConfig() so the dev server shows real data.
# ---------------------------------------------------------------------------

def _parse_action(parts, offset=0):
    """Parse Action[,Arg1[,Arg2]] from a list of string parts starting at offset."""
    t = int(parts[offset]) if offset < len(parts) else 0
    x = int(parts[offset + 1]) if offset + 1 < len(parts) else 0
    y = int(parts[offset + 2]) if offset + 2 < len(parts) else 0
    act = {"t": t}
    if x: act["x"] = x
    if y: act["y"] = y
    return act


def _make_button():
    return {"p": {"t": 0}, "l": {"t": 0}, "a": {"t": 0}}


def parse_example_config(path):
    """Read path and return a _config dict matching buildFullConfigJson()."""
    # Firmware defaults
    cfg = {
        "volume":        50,
        "startup":       "n",
        "rndon":         "n",
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
        "wifion":        "y",
        "wifissid":      "amidala",
        "wifipassword":  "Astromech",
        "xbr":           "00000000",
        "xbl":           "00000000",
        "audiohw":       "hcr",
        "volumeChA":     50,
        "volumeChB":     50,
        "volumewheel":   0,
        "altvolumewheel":0,
        "startupem":     0,
        "startuplvl":    0,
        "ackem":         0,
        "acklvl":        0,
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
        "domespeed":     80,
        "domespeedhome": 50,
        "domespeedseek": 50,
        "domespeedmin":  10,
        "domedecelzone": 20,
        "domehome":      0,
        "domeflip":      "n",
        "domeimu":       "n",
        "domech6":       "n",
        "domeseekl":     90,
        "domeseekr":     90,
        "domefudge":     5,
        "domercaddr":    128,
        "domercchan":    1,
        "domercqpps":    1000,
        "domefront":     0,
        "domestall":     2000,
        "minpulse":      1000,
        "maxpulse":      2000,
        "domehw":        "roboclaw",
        "altbtn":        0,
        "mutebutton":    0,
        "altdomestick":  0,
        "sbs":         [],
        "servos":      [],
        "sstr":        [],
        "estop_cmds":  [],
        "resume_cmds": [],
        "buttons":  [_make_button() for _ in range(9)],
        "gestures": [],
        "gadgets_cfg": [{"type": 0, "sstr": []} for _ in range(7)],
        "sstr_user_cnt": 0,
    }

    # Simple scalar keys that map directly
    _int_keys  = {"volume", "mindelay", "maxdelay", "serialbaud", "serialdelim",
                  "serialeol", "myi2c", "rcchn", "rcd", "rcj", "fst",
                  "rvrmin", "rvrmax", "rvlmin", "rvlmax", "j1adjv", "j1adjh",
                  "domespeed", "domespeedhome", "domespeedseek", "domespeedmin",
                  "domedecelzone", "domehome", "domeseekl", "domeseekr",
                  "domefudge", "domercaddr", "domercchan", "domercqpps",
                  "domefront", "domestall", "minpulse", "maxpulse",
                  "altbtn", "mutebutton", "altdomestick",
                  "startupem", "startuplvl", "ackem", "acklvl",
                  "volumeChA", "volumeChB", "volumewheel", "altvolumewheel"}
    _str_keys  = {"startup", "rndon", "ackon", "goslow", "mix12", "auto",
                  "wifion", "wifissid", "wifipassword", "xbr", "xbl",
                  "audiohw", "domeflip", "domeimu", "domech6"}

    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"  [web_dev] Warning: {path} not found — using default config")
        return cfg

    for raw in lines:
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("//"):
            continue
        if "=" not in line:
            continue
        key, _, val = line.partition("=")
        key = key.strip()
        val = val.strip()

        if key in _int_keys:
            try:
                cfg[key] = int(val)
            except ValueError:
                pass
            continue

        if key in _str_keys:
            cfg[key] = val
            continue

        # Servo: s=N,min,max,neutral,deadzone,trim,speed,reversed
        if key == "s":
            p = val.split(",")
            if len(p) >= 1:
                idx = int(p[0]) - 1
                while len(cfg["servos"]) <= idx:
                    cfg["servos"].append({"min": 0, "max": 180, "n": 90,
                                          "d": 4, "t": 0, "sp": 50, "r": 0})
                sv = cfg["servos"][idx]
                if len(p) > 1: sv["min"] = int(p[1])
                if len(p) > 2: sv["max"] = int(p[2])
                if len(p) > 3: sv["n"]   = int(p[3])
                if len(p) > 4: sv["d"]   = int(p[4])
                if len(p) > 5: sv["t"]   = int(p[5])
                if len(p) > 6: sv["sp"]  = int(p[6])
                if len(p) > 7: sv["r"]   = int(p[7])
            continue

        # Sound bank: sb=dir,numfiles[,r]  (r = random flag for VMusic)
        if key == "sb":
            p = val.split(",")
            if len(p) >= 2:
                cfg["sbs"].append({
                    "dir": p[0],
                    "n":   int(p[1]),
                    "r":   len(p) > 2 and p[2].strip() == "r",
                })
            continue

        # Serial string: sstr=Name|command  (or sstr=command if no name)
        if key == "sstr":
            if "|" in val:
                name, s = val.split("|", 1)
            else:
                name, s = "", val
            cfg["sstr"].append({"n": name, "s": s})
            continue

        # Safety broadcast commands: estopstr=cmd  resumestr=cmd
        if key == "estopstr" and val and len(cfg["estop_cmds"]) < 16:
            cfg["estop_cmds"].append(val)
            continue
        if key == "resumestr" and val and len(cfg["resume_cmds"]) < 16:
            cfg["resume_cmds"].append(val)
            continue

        # Button: b=N,Action[,Arg1[,Arg2]]
        if key in ("b", "lb", "ab"):
            p = val.split(",")
            if len(p) >= 2:
                btn_idx = int(p[0]) - 1
                if 0 <= btn_idx < 9:
                    act = _parse_action(p, offset=1)
                    layer = {"b": "p", "lb": "l", "ab": "a"}[key]
                    cfg["buttons"][btn_idx][layer] = act
            continue

        # Gesture: g=GestureStr,Action[,Arg1[,Arg2]]
        if key == "g":
            p = val.split(",")
            if len(p) >= 2:
                seq = p[0]
                act = _parse_action(p, offset=1)
                cfg["gestures"].append({
                    "seq": seq,
                    "t": act.get("t", 0),
                    "x": act.get("x", 0),
                    "y": act.get("y", 0),
                })
            continue

        # Ignored firmware-only keys: domemode, domegest, rnd, ackgest, etc.

    cfg["sstr_user_cnt"] = len(cfg["sstr"])
    return cfg


# ---------------------------------------------------------------------------
# Config state — loaded from example_config.txt, mutated by POST /api/config
# ---------------------------------------------------------------------------

_EXAMPLE_CONFIG = os.path.join(_PROJECT_DIR, "example_config.txt")
_config = parse_example_config(_EXAMPLE_CONFIG)

_monitor = {
    "seq": 3,
    "lines": [
        {"t": "Serial monitor initialized", "c": "info"},
        {"t": f"Loaded {len(_config['sstr'])} serial commands from config", "c": "info"},
        {"t": "Dome driver ready @ RoboClaw 128", "c": "info"},
    ],
}

_info = {
    "version":   "1.3",
    "board_rev": "1.1",
    "mcu":       "ESP32-S3 N16R8",
    "date":      "Jun 23 2026",
    "drive":    "roboteq-pwm",
    "dome":     "roboclaw",
    "audio":    _config.get("audiohw", "hcr"),
    "wifi_ssid":  _config.get("wifissid", "amidala"),
    "wifi_ip":    "192.168.4.1",
    "sstr_used":  len(_config["sstr"]),
    "free_heap":  290816,
}


_CSP = ("default-src 'self'; "
        "script-src 'self' 'unsafe-inline'; "
        "style-src 'self' 'unsafe-inline'; "
        "connect-src 'self'")


class _Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=_WEB_DIR, **kwargs)

    def end_headers(self):
        self.send_header("Content-Security-Policy", _CSP)
        super().end_headers()

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
            _monitor["lines"].append({"t": "! EMERGENCY STOP", "c": "info"})
            for cmd in _config.get("estop_cmds", []):
                _monitor["lines"].append({"t": cmd, "c": "tx"})
            _monitor["seq"] += 1
            self._text("OK")
            return
        if path == "/api/resume":
            print("  RESUME")
            _monitor["lines"].append({"t": "RESUME", "c": "info"})
            for cmd in _config.get("resume_cmds", []):
                _monitor["lines"].append({"t": cmd, "c": "tx"})
            _monitor["seq"] += 1
            self._text("OK")
            return
        if path == "/api/monitor":
            cmd = params.get("cmd", "")
            print(f"  SERIAL  {cmd!r}")
            _monitor["lines"].append({"t": "> " + cmd, "c": "tx"})
            _monitor["lines"].append({"t": "  (echoed by dev server)", "c": "info"})
            _monitor["seq"] += 1
            if len(_monitor["lines"]) > 256:
                _monitor["lines"] = _monitor["lines"][-256:]
            self._text("OK")
            return
        if path == "/api/dome":
            cmd = params.get("cmd", "")
            print(f"  DOME    {cmd!r}")
            _monitor["lines"].append({"t": "dome=" + cmd, "c": "tx"})
            _monitor["seq"] += 1
            self._text("OK")
            return
        if path == "/api/gadget-cmd":
            cmd = params.get("cmd", "")
            print(f"  GADGET  {cmd!r}")
            _monitor["lines"].append({"t": "> " + cmd, "c": "tx"})
            _monitor["seq"] += 1
            self._text("OK")
            return
        if path == "/api/serial":
            idx = int(params.get("idx", 0))
            sstr = _config.get("sstr", [])
            if 1 <= idx <= len(sstr):
                s = sstr[idx - 1]
                print(f"  SERIAL  idx={idx} name={s['n']!r} str={s['s']!r}")
                _monitor["lines"].append({"t": "> " + s["s"], "c": "tx"})
                _monitor["seq"] += 1
                self._text("OK")
            else:
                self._text("idx out of range", 400)
            return
        if path == "/api/hcr":
            cmd = params.get("cmd", "")
            if cmd == "muse":
                print("  HCR     muse toggle")
                _monitor["lines"].append({"t": "HCR: muse toggle", "c": "tx"})
            elif cmd == "emote":
                emo = int(params.get("emotion", 0))
                lvl = int(params.get("level", 0))
                print(f"  HCR     emote={emo} level={lvl}")
                _monitor["lines"].append({"t": f"HCR: emote={emo} level={lvl}", "c": "tx"})
            else:
                self._text("unknown cmd", 400)
                return
            _monitor["seq"] += 1
            self._text("OK")
            return
        if path != "/api/config":
            self._text("Not Found", 404)
            return

        # gadget_N_type — set gadget type
        if key.startswith("gadget_") and key.endswith("_type"):
            idx = int(key[7:-5])
            if 0 <= idx < len(_config["gadgets_cfg"]):
                _config["gadgets_cfg"][idx]["type"] = int(value)
                print(f"  GADGET  [{idx}] type={value}")
            self._text("OK")
            return

        # gadget_N_sstr — set gadget serial string list
        if key.startswith("gadget_") and key.endswith("_sstr"):
            idx = int(key[7:-5])
            if 0 <= idx < len(_config["gadgets_cfg"]):
                parts = [int(x) for x in value.split(",") if x.strip().isdigit() and int(x) > 0]
                _config["gadgets_cfg"][idx]["sstr"] = parts
                print(f"  GADGET  [{idx}] sstr={parts}")
            self._text("OK")
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
        # sb_del_N — delete sound bank at index N
        if key.startswith("sb_del_"):
            idx = int(key[7:])
            if 0 <= idx < len(_config["sbs"]):
                _config["sbs"].pop(idx)
            self._text("OK")
            return
        # sb_N — update or append sound bank at index N; value: DIR,numfiles,[s|r]
        if key.startswith("sb_") and key[3:].isdigit():
            idx = int(key[3:])
            parts = value.split(",", 2)
            if len(parts) == 3:
                entry = {"dir": parts[0], "n": int(parts[1]), "r": parts[2] == "r"}
                if idx == len(_config["sbs"]):
                    _config["sbs"].append(entry)
                elif 0 <= idx < len(_config["sbs"]):
                    _config["sbs"][idx] = entry
            self._text("OK")
            return
        # btn_N_press / btn_N_long / btn_N_alt — update button layer action
        if key.startswith("btn_"):
            parts = key.split("_")  # ["btn", N, "press"|"long"|"alt"]
            if len(parts) == 3:
                n   = int(parts[1]) - 1
                lyr = {"press": "p", "long": "l", "alt": "a"}.get(parts[2])
                if 0 <= n < 9 and lyr:
                    if value == "altbtn":
                        _config["altbtn"] = n + 1
                        _config["buttons"][n]["p"] = {"t": 0}
                    elif value == "mutebutton":
                        _config["mutebutton"] = n + 1
                        _config["buttons"][n]["p"] = {"t": 0}
                    else:
                        nums = [int(x) for x in value.split(",") if x] if value and value != "0" else [0]
                        t = nums[0] if nums else 0
                        act = {"t": t}
                        if t == 5 and len(nums) > 1:    act["x"] = nums[1]
                        if t == 7 and len(nums) > 2:    act["x"] = nums[1]; act["y"] = nums[2]
                        if t == 9 and len(nums) > 1:    act["x"] = nums[1]
                        _config["buttons"][n][lyr] = act
            self._text("OK")
            return

        # gesture_del_N — delete gesture at index N
        if key.startswith("gesture_del_"):
            idx = int(key[12:])
            if 0 <= idx < len(_config["gestures"]):
                _config["gestures"].pop(idx)
            self._text("OK")
            return

        # gesture_add — append gesture; value: "SEQ,type[,p1[,p2]]"
        if key == "gesture_add":
            parts = value.split(",", 1)
            seq   = parts[0]
            nums  = [int(x) for x in parts[1].split(",")] if len(parts) > 1 else [0]
            t = nums[0] if nums else 0
            g = {"seq": seq, "t": t, "x": nums[1] if len(nums) > 1 else 0, "y": nums[2] if len(nums) > 2 else 0}
            _config["gestures"].append(g)
            self._text("OK")
            return

        # gesture_N — update gesture at index N; value: "SEQ,type[,p1[,p2]]"
        if key.startswith("gesture_") and key[8:].isdigit():
            idx   = int(key[8:])
            parts = value.split(",", 1)
            seq   = parts[0]
            nums  = [int(x) for x in parts[1].split(",")] if len(parts) > 1 else [0]
            t = nums[0] if nums else 0
            if 0 <= idx < len(_config["gestures"]):
                _config["gestures"][idx] = {"seq": seq, "t": t,
                                             "x": nums[1] if len(nums) > 1 else 0,
                                             "y": nums[2] if len(nums) > 2 else 0}
            self._text("OK")
            return

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
        # estopstr_del_N / estopstr_N / estopstr_add
        elif key.startswith("estopstr_del_"):
            idx = int(key[13:])
            if 0 <= idx < len(_config["estop_cmds"]):
                _config["estop_cmds"].pop(idx)
        elif key == "estopstr_add":
            if value and len(_config["estop_cmds"]) < 16:
                _config["estop_cmds"].append(value)
        elif key.startswith("estopstr_") and key[9:].isdigit():
            idx = int(key[9:])
            if 0 <= idx < len(_config["estop_cmds"]) and value:
                _config["estop_cmds"][idx] = value
        # resumestr_del_N / resumestr_N / resumestr_add
        elif key.startswith("resumestr_del_"):
            idx = int(key[14:])
            if 0 <= idx < len(_config["resume_cmds"]):
                _config["resume_cmds"].pop(idx)
        elif key == "resumestr_add":
            if value and len(_config["resume_cmds"]) < 16:
                _config["resume_cmds"].append(value)
        elif key.startswith("resumestr_") and key[10:].isdigit():
            idx = int(key[10:])
            if 0 <= idx < len(_config["resume_cmds"]) and value:
                _config["resume_cmds"][idx] = value
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
