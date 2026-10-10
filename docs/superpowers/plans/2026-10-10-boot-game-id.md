# WiiWare Boot Game ID Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Supply the supported title's WPSE boot identity so unchanged AOT code selects its USA resources.

**Architecture:** Extend the existing WPSE-specific boot adapter after successful memory-layout validation. Populate the WiiWare boot game-code word at 80003180; leave the 80003194 disc-ID override untouched. Do not patch the title's region getter or its missing-texture assertion.

**Tech Stack:** C++20, real NWii MMU, standalone CTest, Windows x64.

**Spec:** User continuation request; docs/FIRST_NATIVE_BUILD.md; local trace shows 802B2EA0 copying four zero bytes from 80003180, after rejecting empty 80003194, into 804B9468. Guest region selection defaults unknown fourth byte to Japanese.

## Global Constraints

- Identity WPSE01_01 / WPSE / 0001000157505345; keep AOT interpreter-OFF.
- All Nintendo-derived inputs, generated sources, title binaries and traces stay local.
- Preserve current worktree and unrelated user files.

## Review Focus

- Correct big-endian byte order and physical/cached alias visibility.
- Only the four-byte title code changes; adjacent low-memory words preserved.
- Invalid arena layout must cause no identity write.
- Do not overwrite disc-ID override or conflate title low word with disc header.
- Actual English resource lookup and rendering must be verified, not inferred from tests.

### Task 1: Install the WiiWare game code

**Files:** Modify runtime/boot/nwii_guest_memory.cpp and tests/nwii_boot_integration_tests.cpp; document evidence in docs/FIRST_NATIVE_BUILD.md.

**Interfaces:** install_wpse01_boot_memory(CPUContext&, uint32_t, uint32_t, std::string&) retains its signature. Reuse runtime/title_identity.h's pack_game_id(kPokemonRumbleGameId).

- [x] Add real-MMU tests requiring word57505345 at80003180, physical bytes'W','P','S','E', preserved neighbors and disc override; invalid arena leaves existing identity unchanged.
- [x] Build/run nwii_boot_integration_tests; expect failure for missing WPSE identity.
- [x] After successful layout installation, write only80003180 with the established WPSE identity constant and return true; retain false on validation error.
- [x] Run full Release/Debug suites, rebuild native-only title, capture bounded resource/render trace. Trace the next proven blocker if no title screen.
- [ ] Independent review, policy checks, explicit safe commit/push, document first-build remaining work.

## Evidence ledger

Base: f23f080. Pre-flight: independent of the synthetic SYSCONF change; its control run is recorded separately.
Ruling: preserve and execute inline in the existing worktree per the user's autonomous request; plan and evidence remain together for portability.

RED: nwii_boot_integration_tests failed on missing WPSE identity. GREEN: Release24/24, Debug8/8, export and policy tests pass.
Real native-only run post-boot-id-20261010.log exited0 at35seconds; missing texture now9003FBC0; screenshot displays English controller instructions. At30seconds775 submissions/6297draws/13shaders; allGLerror0, stderr empty, noOFF coverage guard. Title menu not reached within this run.
Independent review: no actionable findings; dynamic claims verified separately by the root agent, not inferred from review. Temporary private generated-source lookup diagnostics removed; rebuild initially hit a transient executable link lock, then succeeded on retry with no source change.
