# Display-list bank refresh implementation plan

> Use superpowers:executing-plans; continue autonomously as requested.

**Goal:** Cached per-vertex matrix draws use current position and normal banks
without mutating earlier queued draws.

**Architecture:** Extend the existing copy-before-refresh cache-hit path to
already captured full banks. Preserve single-matrix refresh, indexed-load
replay, parser ordering, and the AOT/runtime boundary.

**Tech stack:** C++20, production GX FIFO parser/MMU, asset-free CTest fixtures.

**Spec:** docs/FIRST_NATIVE_BUILD.md, native rendering correctness requirements.

## Constraints and review focus

Keep WPSE01_01 native AOT and interpreter OFF. No Nintendo files or generated
title code in commits. Do not treat this separate reproduced cache defect as
the cause of the real title's default-matrix zero collapse: its current sampled
draws defer transformation instead of using a full-bank snapshot.

Both memory banks, an actual cache hit, changed normal and position banks,
queued-draw ownership, and exact decoded positions are covered by the fixture.
Previously uncaptured banks and stale lighting selection are separate cases.

## One task: refresh existing full-bank snapshots

- [x] Extend tests/gx_display_list_xf_tests.cpp with original MEM1/MEM2 fixtures
  changing external matrices A to B before a confirmed cache hit.
- [x] Observe RED: 110 checks, 6 failures (second position/normal bank and decoded
  positions in each memory bank), while earlier draws and existing tests pass.
- [x] In third_party/NWiiRecomp/nWiiRuntime/src/hle/gx/fifo_parser.cpp, include
  existing full-bank snapshots in the cache replay copy/refresh branch; copy
  both current banks at that command's ordered stream position.
- [x] Build and run complete Release/Debug suites; fresh independent review.
- [ ] Record limits and commit source-only. Do not replace the running title
  during the unchanged matrix-source diagnostic; native verification follows.
