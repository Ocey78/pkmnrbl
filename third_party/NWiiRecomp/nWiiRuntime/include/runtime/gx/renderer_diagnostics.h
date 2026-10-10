#pragma once

#include <cstdint>

namespace nwii::runtime::gx {

struct RendererGeometryDiagnostics {
  uint64_t batches = 0, vertices = 0, shader_skips = 0;
  uint64_t triangles = 0, nonfinite = 0, nonpositive_w = 0;
  uint64_t outside = 0, degenerate = 0, clockwise = 0, counterclockwise = 0;
  // Left, right, bottom, top, near, far common-plane rejections (can overlap).
  uint64_t outside_planes[6]{};
};

struct RendererDiagnosticSnapshot {
  uint32_t first_gl_error = 0;
  bool cull_enabled = false;
  uint8_t cull_mode = 0;
  bool depth_enabled = false;
  RendererGeometryDiagnostics perspective, orthographic;
};

bool RendererDiagnosticsEnabled();
void RendererDiagnosticsSetCull(bool enabled, uint8_t gx_mode);
void RendererDiagnosticsSetDepth(bool enabled);
void RendererDiagnosticsRecordGlError(uint32_t error);
void RendererDiagnosticsRecordBatch(bool perspective, uint64_t vertices);
void RendererDiagnosticsRecordShaderSkip(bool perspective);
void RendererDiagnosticsRecordTriangle(bool perspective, const float clip[3][4]);
RendererDiagnosticSnapshot RendererDiagnosticsTakeSnapshot();
void RendererDiagnosticsReset();

} // namespace nwii::runtime::gx
