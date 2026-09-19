// hcr_link_transport.h
// Adapter between HumanCyborgRelationsAPI's HCRTransport hook (hcr.h) and the
// framing/routing rules in hcr_link.h. Installed on HCRVocalizer whenever HCR
// is the audio hardware; it re-reads params on every command, so changing
// hcrlink or outboundserial applies live with no reboot.
#pragma once

#include <hcr.h>

#include "hcr_link.h"
#include "params.h"

class HcrLinkTransport : public HCRTransport {
public:
  HcrLinkTransport(const AmidalaParameters &params, HcrLinkSink &sink)
      : fParams(params), fSink(sink) {}

  bool send(const char *command) override {
    return hcrLinkRoute(fParams.hcrlink, fParams.outboundserial == 1, command, fSink);
  }

private:
  const AmidalaParameters &fParams;
  HcrLinkSink &fSink;
};
