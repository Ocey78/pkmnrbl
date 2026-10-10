#include "runtime/gx/renderer_diagnostics.h"

#include <cstdint>
#include <iostream>
#include <limits>

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

  // Hand-derived clip-space cases: the counters describe geometry, not pixels.
  const float ccw[3][4] = {{-0.5f,-0.5f,0,1},{0.5f,-0.5f,0,1},{0,0.5f,0,1}};
  const float cw[3][4] = {{-0.5f,-0.5f,0,1},{0,0.5f,0,1},{0.5f,-0.5f,0,1}};
  const float crossing[3][4] = {{-2,-2,0,1},{2,-2,0,1},{0,2,0,1}};
  const float outside[3][4] = {{2,0,0,1},{3,0,0,1},{2,1,0,1}};
  const float flat[3][4] = {{0,0,0,1},{0.5f,0,0,1},{1,0,0,1}};
  const float behind[3][4] = {{0,0,0,-1},{1,0,0,1},{0,1,0,1}};
  const float invalid[3][4] = {{0,0,0,1},{1,0,0,1},
      {0,std::numeric_limits<float>::quiet_NaN(),0,1}};
  for (const auto* triangle : {ccw,cw,crossing,outside,flat,behind,invalid})
    RendererDiagnosticsRecordTriangle(true, triangle);
  RendererDiagnosticsRecordBatch(true, 21);
  RendererDiagnosticsRecordShaderSkip(true);
  RendererDiagnosticsRecordBatch(false, 3);
  RendererDiagnosticsRecordTriangle(false, ccw);
  const auto geometry = RendererDiagnosticsTakeSnapshot();
  const auto& p = geometry.perspective;
  expect(p.batches == 1 && p.vertices == 21 && p.shader_skips == 1,
         "perspective submission must survive later orthographic draws");
  expect(p.triangles == 7 && p.nonfinite == 1 && p.nonpositive_w == 1,
         "invalid or behind-eye triangles must be identified");
  expect(p.outside == 1, "only common-plane rejection is definitely outside");
  for (unsigned plane = 0; plane < 6; ++plane)
    expect(p.outside_planes[plane] == (plane == 1 ? 1u : 0u),
           "outside classification must identify the actual rejection plane");
  expect(p.degenerate == 1 && p.clockwise == 1 && p.counterclockwise == 2,
         "visible-volume candidates should retain projected winding");
  expect(geometry.orthographic.batches == 1 && geometry.orthographic.vertices == 3 &&
         geometry.orthographic.counterclockwise == 1,
         "orthographic control counters must remain separate");
  const auto cleared = RendererDiagnosticsTakeSnapshot();
  expect(cleared.perspective.triangles == 0 && cleared.perspective.batches == 0 &&
         cleared.perspective.shader_skips == 0 && cleared.orthographic.vertices == 0,
         "geometry counters must reset per observation window");
  expect(cleared.cull_enabled && cleared.depth_enabled,
         "geometry reset must preserve last applied raster state");

  if (failures != 0)
    return 1;

  std::cout << "renderer diagnostics tests passed\n";
  return 0;
}
