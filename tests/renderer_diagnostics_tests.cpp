#include "runtime/gx/renderer_diagnostics.h"

#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

} // namespace

int main() {
  using namespace nwii::runtime::gx;

  RendererDiagnosticsReset();
  RendererDiagnosticsSetCull(true, 2);
  RendererDiagnosticsSetDepth(true);
  RendererDiagnosticsRecordGlError(0);
  RendererDiagnosticsRecordGlError(0x0502);
  RendererDiagnosticsRecordGlError(0x0500);

  auto first = RendererDiagnosticsTakeSnapshot();
  expect(first.cull_enabled, "cull enable state should be retained");
  expect(first.cull_mode == 2, "GX cull mode should be retained");
  expect(first.depth_enabled, "depth enable state should be retained");
  expect(first.first_gl_error == 0x0502,
         "the first nonzero GL error should win the observation window");

  auto second = RendererDiagnosticsTakeSnapshot();
  expect(second.cull_enabled, "taking a snapshot must not clear cull state");
  expect(second.cull_mode == 2, "taking a snapshot must not clear cull mode");
  expect(second.depth_enabled, "taking a snapshot must not clear depth state");
  expect(second.first_gl_error == 0,
         "taking a snapshot should clear the GL error latch");

  if (failures != 0)
    return 1;

  std::cout << "renderer diagnostics tests passed\n";
  return 0;
}
