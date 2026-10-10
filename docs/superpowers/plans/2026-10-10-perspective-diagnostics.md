# Perspective visibility diagnostics implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline; the user authorized continuous debugging.

**Goal:** Distinguish absent GPU submission from clipped scene geometry without changing rendering.

**Architecture:** Extend the default-off renderer diagnostic snapshot with per-window perspective/orthographic submission counters and clip-space triangle classifications. Inspect bounded perspective batch state at the existing GPU boundary, not final caption state.

**Tech Stack:** C++20, existing OpenGL renderer and native synthetic tests.

**Spec:** User's continued cutscene/gameplay request; captions-only scene confirmed locally and by user after 2a08bbc.

## Global constraints

- Preserve AOT architecture, interpreter OFF and all local files.
- No Nintendo files, generated title code, traces or screenshots in Git.
- No cull/depth/TEV overrides or title-specific drawing changes.

## Review focus

- A triangle with all vertices outside on different planes is not trivially rejected.
- Nonfinite coordinates and nonpositive W cannot yield a trustworthy winding.
- Perspective counts must survive subsequent orthographic caption draws.
- Snapshot counters reset without losing existing last-applied cull/depth state.
- Diagnostics must not change pixels or run unbounded per-draw logging.

## Task 1: Observe actual batch geometry

Files: renderer_diagnostics.h/.cpp, renderer_gl.cpp, core/main.cpp under the existing runtime; tests/renderer_diagnostics_tests.cpp and tests/tev_shader_gl_tests.cpp.

- [ ] Add failing tests for triangle classification, mixed-projection window counts/reset and a real GL draw's diagnostic submission. Use original synthetic vertices only.
- [ ] Implement RendererDiagnosticsRecordTriangle(bool perspective, const float clip[3][4]) and RendererDiagnosticsRecordBatch(bool perspective, uint64_t vertices); add shader-skip counter. Track nonfinite, nonpositive W, common-plane rejection, degenerate and CW/CCW triangles; these are geometry observations, not fragment visibility claims.
- [ ] Wire actual s_batch/s_proj before glDrawArrays; sample first 12 perspective batches then every 4096th, capped at 128 records. Log scalar projection, BP and applied GL raster/program state only. Add separate bounded snapshot line to main's existing five-second diagnostics.
- [ ] Run full Release tests and Debug diagnostics; inspect isolated runtime diff, copy only changed runtime files and rebuild title OFF.
- [ ] Run lighter real title trace (no per-call/watch logging), get Save A input and inspect scene geometry evidence. Fix only after the evidence selects a cause.
- [ ] Independent review and source-only checkpoint after verification.
