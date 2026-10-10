# Locked-cache DMA implementation plan

> **For agentic workers:** Use superpowers:executing-plans; continue autonomously as requested. An independent worker supplies the original AOT regression fixtures.

**Goal:** Execute the title's existing model-matrix producer through native locked-cache memory and DMA support.

**Architecture:** Extend the current MMU with separate allocated 32-byte lines for the SDK's identity-mapped E0000000-E0003FFF window. AOT instructions call native HID2/DMAU/DMAL and cache-line helpers; synchronous, ordered DMA completes real copies before reporting an empty queue. No guest function replacement or interpreter is involved.

**Tech Stack:** C++20, NWiiRecomp AOT emitter, MSVC x64, asset-free CTest fixtures.

**Spec:** docs/FIRST_NATIVE_BUILD.md and IBM Gekko User's Manual sections 2.1.2.4, 2.1.2.8, 9.2-9.3, 12 (https://doc.kodewerx.org/documents/gekko_user_manual.pdf). The fixed address window is a software convention, not a generic hardware address mask.

## Global constraints

- WPSE01_01 / WPSE / 0001000157505345; native AOT, interpreter OFF.
- No copyrighted payload, generated title code, logs or title executable in Git.
- Preserve existing local files and solved boot behavior.
- No full cache/TLB or cycle-accurate DMA claim. Unsupported tag/machine-check cases must fail diagnostically rather than silently invent data.

## Review focus

- DMA length split and zero encoding: original tests for 1, 3, 4, 127 and 128 lines.
- LC versus MEM1/MEM2 aliasing: distinct storage and sentinel tests.
- Disabled enable bit, no trigger, and flush precedence: no transfer; command bits clear.
- Window ends and RAM ends: whole-line validation, no wrapping or host overrun.
- Native emitter operands: RA=0, RA+RB, SPR readback, no interpreter fallback.

## Task 1: Native cache/DMA contract

Files: tests/aot_locked_cache_fixture.cpp, tests/aot_locked_cache_tests.cpp,
cmake/NWiiRecomp.cmake, .github/workflows/windows.yml,
third_party/NWiiRecomp/nWiiRuntime/include/runtime/cpu_context.h,
third_party/NWiiRecomp/nWiiRuntime/include/runtime/locked_cache.h,
third_party/NWiiRecomp/nWiiRecomp/src/recompiler.cpp.

Interface: generated calls use MMU::read_cache_spr(unsigned),
MMU::write_cache_spr(unsigned,uint32_t,uint32_t msr), and
MMU::zero_cache_line(uint32_t,bool locked,uint32_t msr).

- [x] Compile original PPC snippets with the real recompiler; observe expected behavioral failures against current stubs.
- [x] Add separate LC backing/allocation, safe address resolution and synchronous DMA; retain register state and enforce command controls.
- [x] Emit native SPR/cache calls with correct operands. Test invalid spans and explicit unsupported paths as well as valid transfers.
- [x] Run targeted tests and full Release/Debug suites; obtain independent review. Its invalidation finding passed RED/GREEN; final84/84 Release and42/42 Debug.
- [ ] Update handoff, enforce source-only repository policy, commit and push verified milestone.

## Task 2: Real-title verification

- [x] Confirm the actual producer selects the LC path and matches zero matrix destinations.
- [x] Regenerate to a fresh ignored directory; compare changes against emitter scope and back up local generated preimages before syncing.
- [ ] Rebuild PokemonRumble.exe with interpreter OFF; run WPSE and inspect matrix source values, transformed positions and rendered output.
- [ ] Record the observed outcome without equating successful tests/builds with successful cutscenes/gameplay. Continue from the next proven boundary.
