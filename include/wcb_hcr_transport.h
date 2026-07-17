// wcb_hcr_transport.h
// Adapter bridging HumanCyborgRelationsAPI's HCRTransport interface (hcr.h)
// to a live WCB_Client. This is the ONLY file that includes both hcr.h and
// WCB_Client.h — the coupling between the two vendored libraries lives here,
// in the integration point, not in either library itself.
#pragma once

#include <hcr.h>
#include <WCB_Client.h>

class WCBHcrTransport : public HCRTransport {
public:
  explicit WCBHcrTransport(WCB_Client *client) : fClient(client) {}

  bool send(const char *command) override {
    return fClient->broadcast(command);
  }

private:
  WCB_Client *fClient;
};
