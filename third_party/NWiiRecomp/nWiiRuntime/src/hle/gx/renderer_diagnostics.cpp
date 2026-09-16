#include "runtime/gx/renderer_diagnostics.h"

#include <cstdlib>

namespace nwii::runtime::gx {
namespace {

RendererDiagnosticSnapshot g_snapshot;

} // namespace

bool RendererDiagnosticsEnabled() {
  static const bool enabled = std::getenv("NWII_RENDER_DIAG") != nullptr;
  return enabled;
}

void RendererDiagnosticsSetCull(bool enabled, uint8_t gx_mode) {
  g_snapshot.cull_enabled = enabled;
  g_snapshot.cull_mode = gx_mode;
}

void RendererDiagnosticsSetDepth(bool enabled) {
  g_snapshot.depth_enabled = enabled;
}

void RendererDiagnosticsRecordGlError(uint32_t error) {
  if (error != 0 && g_snapshot.first_gl_error == 0)
    g_snapshot.first_gl_error = error;
}

RendererDiagnosticSnapshot RendererDiagnosticsTakeSnapshot() {
  RendererDiagnosticSnapshot result = g_snapshot;
  g_snapshot.first_gl_error = 0;
  return result;
}

void RendererDiagnosticsReset() { g_snapshot = {}; }

} // namespace nwii::runtime::gx
