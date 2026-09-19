// test_hcr_link.cpp
// Tests for hcr_link.h: how an HCR frame is framed and routed for the link
// selected by params.hcrlink (Serial vs WCB Native) and params.outboundserial.
//
// The Serial-mode tests are regression tests: they pin the bytes and the
// fall-through behavior that existed before hcrlink did, so adding WCB Native
// can't change anything for an HCR on Serial0 or on a plain WCB serial port.

#include "hcr_link.h"
#include <string>
#include <vector>
#include <unity.h>

namespace {

const char *kFrame = "<SH50,QEH,QT>";  // what HCRVocalizer::Stimulate(HAPPY, 50) emits

struct FakeSink : HcrLinkSink {
  bool meshOk = true;
  std::vector<std::string> mesh;    // lines handed to sendMesh (incl. refused ones)
  std::vector<std::string> host;    // lines handed to sendMeshToHost (incl. refused ones)
  std::vector<std::string> serial;  // byte strings handed to writeSerial

  bool sendMesh(const char *line) override {
    mesh.push_back(line);
    return meshOk;
  }
  bool sendMeshToHost(const char *line) override {
    host.push_back(line);
    return meshOk;
  }
  void writeSerial(const char *bytes) override { serial.push_back(bytes); }
};

}  // namespace

void setUp() {}
void tearDown() {}

// ---- hcrWrapNative ----------------------------------------------------------

void test_wrap_prefixes_raw_verb() {
  char out[HCR_LINK_LINE_BUF];
  TEST_ASSERT_TRUE(hcrWrapNative(kFrame, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(";H,RAW,<SH50,QEH,QT>", out);
}

void test_wrap_rejects_empty_and_null() {
  char out[HCR_LINK_LINE_BUF];
  TEST_ASSERT_FALSE(hcrWrapNative("", out, sizeof(out)));
  TEST_ASSERT_FALSE(hcrWrapNative(nullptr, out, sizeof(out)));
}

void test_wrap_rejects_frame_too_long_for_one_mesh_packet() {
  std::string big(HCR_LINK_MAX_LINE, 'x');  // + prefix pushes it past the ceiling
  char out[HCR_LINK_LINE_BUF];
  TEST_ASSERT_FALSE(hcrWrapNative(big.c_str(), out, sizeof(out)));
}

void test_wrap_accepts_frame_exactly_at_the_ceiling() {
  std::string fit(HCR_LINK_MAX_LINE - strlen(HCR_LINK_NATIVE_PREFIX), 'x');
  char out[HCR_LINK_LINE_BUF];
  TEST_ASSERT_TRUE(hcrWrapNative(fit.c_str(), out, sizeof(out)));
  TEST_ASSERT_EQUAL(HCR_LINK_MAX_LINE, (int)strlen(out));
}

void test_wrap_rejects_when_output_buffer_too_small() {
  char out[10];
  TEST_ASSERT_FALSE(hcrWrapNative(kFrame, out, sizeof(out)));
}

// ---- Serial mode (default): nothing changes ---------------------------------

void test_serial_uart0_is_not_handled_so_library_writes_bare_frame() {
  FakeSink s;
  TEST_ASSERT_FALSE(hcrLinkRoute(HCR_LINK_SERIAL, false, kFrame, s));
  TEST_ASSERT_EQUAL(0, (int)s.mesh.size());
  TEST_ASSERT_EQUAL(0, (int)s.serial.size());
}

void test_serial_mesh_sends_bare_frame_unchanged() {
  FakeSink s;
  TEST_ASSERT_TRUE(hcrLinkRoute(HCR_LINK_SERIAL, true, kFrame, s));
  TEST_ASSERT_EQUAL(1, (int)s.mesh.size());
  TEST_ASSERT_EQUAL_STRING(kFrame, s.mesh[0].c_str());
  TEST_ASSERT_EQUAL(0, (int)s.serial.size());
}

void test_serial_mesh_failure_falls_back_to_library_write() {
  FakeSink s;
  s.meshOk = false;
  TEST_ASSERT_FALSE(hcrLinkRoute(HCR_LINK_SERIAL, true, kFrame, s));
  TEST_ASSERT_EQUAL(0, (int)s.serial.size());  // the library does the UART write itself
}

void test_unknown_link_value_behaves_as_serial() {
  FakeSink s;
  TEST_ASSERT_FALSE(hcrLinkRoute(7, false, kFrame, s));
  TEST_ASSERT_TRUE(hcrLinkRoute(7, true, kFrame, s));
  TEST_ASSERT_EQUAL_STRING(kFrame, s.mesh[0].c_str());
}

// ---- WCB Native mode --------------------------------------------------------

void test_native_mesh_sends_wrapped_line_toward_the_hcr_host() {
  FakeSink s;
  TEST_ASSERT_TRUE(hcrLinkRoute(HCR_LINK_WCB_NATIVE, true, kFrame, s));
  TEST_ASSERT_EQUAL(1, (int)s.host.size());
  TEST_ASSERT_EQUAL_STRING(";H,RAW,<SH50,QEH,QT>", s.host[0].c_str());
  TEST_ASSERT_EQUAL(0, (int)s.mesh.size());
  TEST_ASSERT_EQUAL(0, (int)s.serial.size());
}

// Regression: a bare frame must always reach EVERY WCB, since any of them may
// have the HCR on a plain serial port. Only native lines may target the host.
void test_serial_mode_never_narrows_to_the_hcr_host() {
  FakeSink s;
  hcrLinkRoute(HCR_LINK_SERIAL, true, kFrame, s);
  TEST_ASSERT_EQUAL(1, (int)s.mesh.size());
  TEST_ASSERT_EQUAL(0, (int)s.host.size());
}

void test_native_uart0_writes_wrapped_line_with_priming_newline() {
  FakeSink s;
  TEST_ASSERT_TRUE(hcrLinkRoute(HCR_LINK_WCB_NATIVE, false, kFrame, s));
  TEST_ASSERT_EQUAL(0, (int)s.mesh.size());
  TEST_ASSERT_EQUAL(0, (int)s.host.size());
  TEST_ASSERT_EQUAL(1, (int)s.serial.size());
  TEST_ASSERT_EQUAL_STRING("\n;H,RAW,<SH50,QEH,QT>\n", s.serial[0].c_str());
}

void test_native_mesh_failure_falls_back_to_wrapped_uart0() {
  FakeSink s;
  s.meshOk = false;
  TEST_ASSERT_TRUE(hcrLinkRoute(HCR_LINK_WCB_NATIVE, true, kFrame, s));
  TEST_ASSERT_EQUAL(1, (int)s.host.size());
  TEST_ASSERT_EQUAL(1, (int)s.serial.size());
  TEST_ASSERT_EQUAL_STRING("\n;H,RAW,<SH50,QEH,QT>\n", s.serial[0].c_str());
}

void test_native_never_emits_an_unwrapped_frame() {
  const char *frames[] = {"<SH50,QEH,QT>", "<PSV,QT>", "<PVV75>", "<MN10,MX30>"};
  for (int wantMesh = 0; wantMesh <= 1; wantMesh++) {
    for (int meshOk = 0; meshOk <= 1; meshOk++) {
      for (const char *f : frames) {
        FakeSink s;
        s.meshOk = meshOk;
        TEST_ASSERT_TRUE(hcrLinkRoute(HCR_LINK_WCB_NATIVE, wantMesh, f, s));
        TEST_ASSERT_EQUAL(0, (int)s.mesh.size());  // native never takes the broadcast leg
        for (const std::string &m : s.host)
          TEST_ASSERT_EQUAL(0, m.compare(0, strlen(HCR_LINK_NATIVE_PREFIX), HCR_LINK_NATIVE_PREFIX));
        for (const std::string &b : s.serial)
          TEST_ASSERT_EQUAL(0, b.compare(1, strlen(HCR_LINK_NATIVE_PREFIX), HCR_LINK_NATIVE_PREFIX));
      }
    }
  }
}

void test_native_unsendable_frame_is_swallowed_not_leaked_bare() {
  FakeSink s;
  std::string big(HCR_LINK_MAX_LINE, 'x');
  TEST_ASSERT_TRUE(hcrLinkRoute(HCR_LINK_WCB_NATIVE, true, big.c_str(), s));
  TEST_ASSERT_EQUAL(0, (int)s.mesh.size());
  TEST_ASSERT_EQUAL(0, (int)s.host.size());
  TEST_ASSERT_EQUAL(0, (int)s.serial.size());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_wrap_prefixes_raw_verb);
  RUN_TEST(test_wrap_rejects_empty_and_null);
  RUN_TEST(test_wrap_rejects_frame_too_long_for_one_mesh_packet);
  RUN_TEST(test_wrap_accepts_frame_exactly_at_the_ceiling);
  RUN_TEST(test_wrap_rejects_when_output_buffer_too_small);
  RUN_TEST(test_serial_uart0_is_not_handled_so_library_writes_bare_frame);
  RUN_TEST(test_serial_mesh_sends_bare_frame_unchanged);
  RUN_TEST(test_serial_mesh_failure_falls_back_to_library_write);
  RUN_TEST(test_unknown_link_value_behaves_as_serial);
  RUN_TEST(test_native_mesh_sends_wrapped_line_toward_the_hcr_host);
  RUN_TEST(test_serial_mode_never_narrows_to_the_hcr_host);
  RUN_TEST(test_native_uart0_writes_wrapped_line_with_priming_newline);
  RUN_TEST(test_native_mesh_failure_falls_back_to_wrapped_uart0);
  RUN_TEST(test_native_never_emits_an_unwrapped_frame);
  RUN_TEST(test_native_unsendable_frame_is_swallowed_not_leaked_bare);
  return UNITY_END();
}
