#include "runtime/gx/renderer_diagnostics.h"

#include <cstdlib>
#include <cmath>

namespace nwii::runtime::gx {
namespace {

RendererDiagnosticSnapshot g_snapshot;

} // namespace

void RendererDiagnosticsRecordBatch(bool perspective, uint64_t vertices) {
  auto& d = perspective ? g_snapshot.perspective : g_snapshot.orthographic;
  ++d.batches;
  d.vertices += vertices;
}

void RendererDiagnosticsRecordShaderSkip(bool perspective) {
  ++(perspective ? g_snapshot.perspective : g_snapshot.orthographic).shader_skips;
}

void RendererDiagnosticsRecordTriangle(bool perspective, const float clip[3][4]) {
  auto& d = perspective ? g_snapshot.perspective : g_snapshot.orthographic;
  ++d.triangles;
  for (unsigned i = 0; i < 3; ++i)
    for (unsigned j = 0; j < 4; ++j)
      if (!std::isfinite(clip[i][j])) {
        ++d.nonfinite;
        return;
      }
  for (unsigned i = 0; i < 3; ++i)
    if (clip[i][3] <= 0) {
      ++d.nonpositive_w;
      return;
    }
  unsigned common = 0x3f;
  for (unsigned i = 0; i < 3; ++i) {
    unsigned planes = 0;
    for (unsigned j = 0; j < 3; ++j) {
      if (clip[i][j] < -clip[i][3]) planes |= 1u << (j * 2);
      if (clip[i][j] >  clip[i][3]) planes |= 2u << (j * 2);
    }
    common &= planes;
  }
  if (common) {
    ++d.outside;
    for (unsigned plane = 0; plane < 6; ++plane)
      if (common & (1u << plane)) ++d.outside_planes[plane];
    return;
  }
  double x[3], y[3];
  for (unsigned i = 0; i < 3; ++i) {
    x[i] = double(clip[i][0]) / clip[i][3];
    y[i] = double(clip[i][1]) / clip[i][3];
  }
  const double area = (x[1]-x[0])*(y[2]-y[0]) - (y[1]-y[0])*(x[2]-x[0]);
  if (area == 0) ++d.degenerate;
  else if (area < 0) ++d.clockwise;
  else ++d.counterclockwise;
}

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
  g_snapshot.perspective = {};
  g_snapshot.orthographic = {};
  return result;
}

void RendererDiagnosticsReset() { g_snapshot = {}; }

} // namespace nwii::runtime::gx
