// wifi_ap.h
// AmidalaWiFiAP: manages the on-board WiFi soft access point, DNS redirect,
// mDNS, and the HTTP web UI / REST API.
//
// Call begin() once during setup (after config is loaded so params are
// available), then handle() on every loop iteration.

#pragma once

class AmidalaController;

class AmidalaWiFiAP {
public:
  void begin(const char* ssid, const char* password, AmidalaController* ctrl);
  void handle();
};
