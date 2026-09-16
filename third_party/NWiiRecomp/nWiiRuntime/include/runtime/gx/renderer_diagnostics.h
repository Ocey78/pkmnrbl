#pragma once

#include <cstdint>

namespace nwii::runtime::gx {

struct RendererDiagnosticSnapshot {
  uint32_t first_gl_error = 0;
  bool cull_enabled = false;
  uint8_t cull_mode = 0;
  bool depth_enabled = false;
};

bool RendererDiagnosticsEnabled();
void RendererDiagnosticsSetCull(bool enabled, uint8_t gx_mode);
void RendererDiagnosticsSetDepth(bool enabled);
void RendererDiagnosticsRecordGlError(uint32_t error);
RendererDiagnosticSnapshot RendererDiagnosticsTakeSnapshot();
void RendererDiagnosticsReset();

} // namespace nwii::runtime::gx
