# Supplied normal preservation implementation plan

> **For agentic workers:** Use superpowers:executing-plans. Continue autonomously in the existing feature checkout; this is separate from native locked-cache verification.

**Goal:** Preserve supplied vertex normals when parsing ahead of renderer state replay.

**Architecture:** Select normal decoding from the draw's own VAT/VCD attribute presence, not prior render-time lighting state. Keep matrix snapshots, renderer ordering, cache identity and native AOT architecture unchanged.

**Tech Stack:** C++20 production FIFO parser and original GX fixtures.

**Spec:** docs/FIRST_NATIVE_BUILD.md; parse/decode/render ordering in gx.cpp and current FIFO vertex attribute contract.

## Constraints and review focus

- No title code/data or generated assets in Git; interpreter OFF.
- Test lit-at-draw followed by disabled lighting, plus supplied unlit normals.
- Check exact transformed normals, presence flags, positions and vertex counts.
- Do not claim normal-based texture generation is implemented.
- Do not attribute real scene failure to normals without post-LC evidence.

## One task

Files: tests/gx_display_list_xf_tests.cpp, third_party/NWiiRecomp/nWiiRuntime/src/hle/gx/fifo_parser.cpp, third_party/NWiiRecomp/nWiiRuntime/include/runtime/gx/fifo_parser.h.

- [x] Add original direct-F32 normal fixtures, parse before any renderer replay, and observe normal-preservation assertions fail (4/122 checks).
- [x] Make DrawRaw::need_normal follow the draw's normal attribute presence; update its comment. No lighting shadow or renderer mutation.
- [x] Run full Debug/Release suites and independent review (42/42,84/84; no review issues).
- [ ] Source-only commit/push.
- [ ] Keep this fix out of the in-progress LC-only native build; apply to a subsequent real-title run after that comparison.
