// wcb_config_validator.h
// Pure decision logic for whether the WCB Client's configured identity is
// complete enough to attempt joining the mesh.
//
// Deliberately free of any WCB_Client/ESP-NOW dependency so it's unit-testable
// natively (WCB_Client itself is hardware-only, same constraint noted for
// BTGamepad's connection layer in test/test_bt_gamepad/test_bt_gamepad.cpp).
//
// Called once at boot (only when params.wcbenable is set) before attempting
// to construct/begin() the mesh client. On failure, the caller logs the
// reason to the console and skips joining entirely rather than attempting a
// connection with incomplete identity.
#pragma once

#include "params.h"

struct WCBConfigValidation {
    bool        ok;
    const char* reason;  // nullptr when ok
};

class WCBConfigValidator {
public:
    static WCBConfigValidation validate(const AmidalaParameters &params) {
        if (params.wcbpassword[0] == '\0')
            return {false, "wcbpassword is not set"};
        if (params.wcbquantity == 0)
            return {false, "wcbquantity is not set"};
        if (params.wcbid == 0 || params.wcbid > 20)
            return {false, "wcbid must be 1-20"};
        // wcboct2/wcboct3 have no invalid value (any uint8_t is a legal MAC
        // octet) -- there is nothing further to check for them.
        return {true, nullptr};
    }
};
