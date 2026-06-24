#ifdef USE_BT_CONTROLLER

#include "bt_gamepad.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <BLEAdvertisedDevice.h>

// HID service / characteristic UUIDs (Bluetooth SIG assigned)
static BLEUUID HID_SERVICE_UUID((uint16_t)0x1812);
static BLEUUID HID_REPORT_UUID((uint16_t)0x2A4D);

// Singleton pointer used in static callbacks.
static BTGamepad* sInstance = nullptr;

// ---- BLE Advertised-Device callback ----------------------------------------

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override {
        if (!sInstance) return;
        bool isHID = dev.haveServiceUUID() && dev.isAdvertisingService(HID_SERVICE_UUID);
        if (!isHID) return;

        const char* addr = dev.getAddress().toString().c_str();
        const char* name = dev.haveName() ? dev.getName().c_str() : "";
        int rssi = dev.getRSSI();

        sInstance->_onScanResult(addr, name, rssi);

        // If we want this specific device or any HID device, stop and connect.
        const char* target = sInstance->connectedAddr()[0] == '\0'
                                 ? nullptr
                                 : sInstance->connectedAddr();
        bool matches = !target || strcasecmp(addr, target) == 0;
        // (we actually compare fTargetAddr — use the public accessor on a future
        //  refactor; for now let animate() decide after scan.)
    }
};

static ScanCallbacks sScanCallbacks;

// ---- BLE Client callback ---------------------------------------------------

class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient*) override {}
    void onDisconnect(BLEClient*) override {
        if (sInstance) sInstance->_onDisconnected();
    }
};

static ClientCallbacks sClientCallbacks;

// ---- HID Report notification callback --------------------------------------

static void notifyCallback(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (sInstance) sInstance->_onReport(data, len);
}

// ---- BTGamepad implementation ----------------------------------------------

BTGamepad gBTGamepad;

BTGamepad::BTGamepad() :
    fScanning(false),
    fScanDone(false),
    fDoConnect(false),
    fConnecting(false),
    fScanResultCount(0),
    fLastConnectAttemptMs(0)
{
    fTargetAddr[0]    = '\0';
    fConnectedAddr[0] = '\0';
}

void BTGamepad::setTargetAddr(const char* addr)
{
    strncpy(fTargetAddr, addr ? addr : "", sizeof(fTargetAddr) - 1);
    fTargetAddr[sizeof(fTargetAddr) - 1] = '\0';
}

void BTGamepad::setup()
{
    sInstance = this;
    BLEDevice::init("Amidala");
}

void BTGamepad::startScan()
{
    if (fScanning) return;
    fScanDone        = false;
    fScanResultCount = 0;
    fScanning        = true;

    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&sScanCallbacks, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    // Async 5-second scan; completion triggers _onScanDone via lambda.
    scan->start(5, [](BLEScanResults) {
        if (sInstance) sInstance->_onScanDone();
    }, false);
}

void BTGamepad::pairWith(const char* addr)
{
    setTargetAddr(addr);
    fDoConnect = true;
    fScanDone  = false;
    fScanning  = false;
}

void BTGamepad::forget()
{
    fTargetAddr[0] = '\0';
}

void BTGamepad::animate()
{
    if (fConnected) return;

    // Retry connection every 10 s.
    uint32_t now = millis();
    if (!fConnecting && (now - fLastConnectAttemptMs > 10000)) {
        fLastConnectAttemptMs = now;
        _attemptConnect();
    }
}

void BTGamepad::_attemptConnect()
{
    // If we have no target, scan first to find any HID device.
    if (fTargetAddr[0] == '\0') {
        startScan();
        return;
    }

    fConnecting = true;
    BLEAddress bleAddr(fTargetAddr);
    BLEClient* client = BLEDevice::createClient();
    client->setClientCallbacks(&sClientCallbacks);

    if (!client->connect(bleAddr)) {
        client->disconnect();
        fConnecting = false;
        return;
    }
    client->setMTU(517);

    BLERemoteService* hid = client->getService(HID_SERVICE_UUID);
    if (!hid) {
        client->disconnect();
        fConnecting = false;
        return;
    }

    // Subscribe to all HID Report characteristics (there can be multiple).
    bool gotOne = false;
    auto* chars = hid->getCharacteristics();
    for (auto& [uuid, ch] : *chars) {
        if (ch->getUUID().equals(HID_REPORT_UUID) && ch->canNotify()) {
            ch->registerForNotify(notifyCallback);
            gotOne = true;
        }
    }

    if (!gotOne) {
        client->disconnect();
        fConnecting = false;
        return;
    }

    _onConnected(fTargetAddr);
    fConnecting = false;
}

// Called from BLE task.
void BTGamepad::_onConnected(const char* addr)
{
    strncpy(fConnectedAddr, addr ? addr : "", sizeof(fConnectedAddr) - 1);
    fConnectedAddr[sizeof(fConnectedAddr) - 1] = '\0';
    fConnected = true;
    onConnect();
    notify();
}

void BTGamepad::_onDisconnected()
{
    fConnectedAddr[0] = '\0';
    fConnected = false;
    onDisconnect();
}

void BTGamepad::_onScanResult(const char* addr, const char* name, int rssi)
{
    if (fScanResultCount >= BT_SCAN_MAX_RESULTS) return;
    BTScanResult& r = fScanResults[fScanResultCount++];
    strncpy(r.addr, addr, sizeof(r.addr) - 1);
    r.addr[sizeof(r.addr) - 1] = '\0';
    strncpy(r.name, name, sizeof(r.name) - 1);
    r.name[sizeof(r.name) - 1] = '\0';
    r.rssi = rssi;

    // If this matches our target (or we'll take any), trigger connect.
    if (fTargetAddr[0] == '\0' || strcasecmp(addr, fTargetAddr) == 0) {
        setTargetAddr(addr);
        BLEDevice::getScan()->stop();
        fDoConnect = true;
    }
}

void BTGamepad::_onScanDone()
{
    fScanning = false;
    fScanDone = true;
    if (fDoConnect) {
        fDoConnect = false;
        _attemptConnect();
    }
}

// Parse a raw HID input report into JoystickController state.
//
// A single standard mapping cannot cover every BLE gamepad — HID report
// descriptors vary per device.  We implement the layout used by most common
// BLE HID gamepads (Nintendo Switch Pro, 8BitDo, many USB-BLE adapters, and
// the "Generic Gamepad" profile):
//
//   Byte 0:  buttons 0–7  (bit 0 = A/Cross, 1 = B/Circle, 2 = X/Square, 3 = Y/Triangle,
//                           4 = LB/L1, 5 = RB/R1, 6 = Select/Share, 7 = Start/Options)
//   Byte 1:  buttons 8–15 (bit 0 = L3, 1 = R3, 2 = Home/PS)
//   Byte 2:  hat switch   (0=N,1=NE,2=E,3=SE,4=S,5=SW,6=W,7=NW, 8=center)
//   Byte 3:  LX  0–255
//   Byte 4:  LY  0–255
//   Byte 5:  RX  0–255
//   Byte 6:  RY  0–255
//   Byte 7:  LT/L2  0–255
//   Byte 8:  RT/R2  0–255
//
// Xbox Series BLE uses 16-bit axis values (little-endian) in a different
// layout.  We detect it by report length and adjust accordingly.
void BTGamepad::_parseReport(const uint8_t* d, size_t len)
{
    // Some devices prepend a 1-byte report ID — skip it if len is 16 and
    // first byte is 0x01 (most common report ID).
    size_t off = 0;
    if (len >= 10 && d[0] == 0x01) off = 1;

    if (len - off >= 9) {
        // Standard generic gamepad layout.
        uint8_t b0 = d[off + 0];
        uint8_t b1 = d[off + 1];
        uint8_t hat = d[off + 2];
        int8_t lx = (int8_t)((int)d[off + 3] - 128);
        int8_t ly = (int8_t)((int)d[off + 4] - 128);
        int8_t rx = (int8_t)((int)d[off + 5] - 128);
        int8_t ry = (int8_t)((int)d[off + 6] - 128);
        uint8_t lt = d[off + 7];
        uint8_t rt = d[off + 8];

        state.analog.stick.lx = lx;
        state.analog.stick.ly = ly;
        state.analog.stick.rx = rx;
        state.analog.stick.ry = ry;
        state.analog.button.l2 = lt;
        state.analog.button.r2 = rt;

        state.button.cross    = (b0 >> 0) & 1;  // A / ×
        state.button.circle   = (b0 >> 1) & 1;  // B / ○
        state.button.square   = (b0 >> 2) & 1;  // X / □
        state.button.triangle = (b0 >> 3) & 1;  // Y / △
        state.button.l1       = (b0 >> 4) & 1;
        state.button.r1       = (b0 >> 5) & 1;
        state.button.select   = (b0 >> 6) & 1;
        state.button.start    = (b0 >> 7) & 1;
        state.button.l3       = (b1 >> 0) & 1;
        state.button.r3       = (b1 >> 1) & 1;
        state.button.ps       = (b1 >> 2) & 1;

        // Hat to d-pad
        state.button.up    = (hat == 0 || hat == 1 || hat == 7);
        state.button.right = (hat == 1 || hat == 2 || hat == 3);
        state.button.down  = (hat == 3 || hat == 4 || hat == 5);
        state.button.left  = (hat == 5 || hat == 6 || hat == 7);

        notify();
    } else if (len - off >= 14) {
        // Xbox Series BLE: 16-bit axes (little-endian), different byte layout.
        // Buttons at bytes 14–15, axes at 2–9, triggers at 10–13.
        uint16_t lx16 = (uint16_t)d[off+2] | ((uint16_t)d[off+3] << 8);
        uint16_t ly16 = (uint16_t)d[off+4] | ((uint16_t)d[off+5] << 8);
        uint16_t rx16 = (uint16_t)d[off+6] | ((uint16_t)d[off+7] << 8);
        uint16_t ry16 = (uint16_t)d[off+8] | ((uint16_t)d[off+9] << 8);
        uint16_t lt16 = (uint16_t)d[off+10] | ((uint16_t)d[off+11] << 8);
        uint16_t rt16 = (uint16_t)d[off+12] | ((uint16_t)d[off+13] << 8);

        state.analog.stick.lx = (int8_t)((int)(lx16 >> 8) - 128);
        state.analog.stick.ly = (int8_t)((int)(ly16 >> 8) - 128);
        state.analog.stick.rx = (int8_t)((int)(rx16 >> 8) - 128);
        state.analog.stick.ry = (int8_t)((int)(ry16 >> 8) - 128);
        state.analog.button.l2 = (uint8_t)(lt16 >> 2); // 10-bit → 8-bit
        state.analog.button.r2 = (uint8_t)(rt16 >> 2);

        if (len - off >= 16) {
            uint8_t b = d[off + 14];
            state.button.cross    = (b >> 4) & 1; // A
            state.button.circle   = (b >> 5) & 1; // B
            state.button.square   = (b >> 6) & 1; // X
            state.button.triangle = (b >> 7) & 1; // Y
            uint8_t b2 = d[off + 15];
            state.button.l1     = (b2 >> 0) & 1;
            state.button.r1     = (b2 >> 1) & 1;
            state.button.select = (b2 >> 2) & 1;
            state.button.start  = (b2 >> 3) & 1;
            state.button.l3     = (b2 >> 4) & 1;
            state.button.r3     = (b2 >> 5) & 1;
            uint8_t hat = d[off + 0] & 0xF;
            state.button.up    = (hat == 1);
            state.button.right = (hat == 3);
            state.button.down  = (hat == 5);
            state.button.left  = (hat == 7);
        }
        notify();
    }
}

void BTGamepad::_onReport(const uint8_t* data, size_t len)
{
    _parseReport(data, len);
}

#endif // USE_BT_CONTROLLER
