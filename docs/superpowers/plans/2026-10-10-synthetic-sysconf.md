# Synthetic SYSCONF Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the runtime-owned system configuration readable by the guest, with an English default for the supported USA title.

**Architecture:** Keep the existing in-memory FS device. Replace its malformed synthetic SYSCONF with an original, minimal SCv0 document; do not patch guest language functions or import a NAND image.

**Tech Stack:** C++20, MSVC, CMake/CTest, real FS device and guest MMU.

**Spec:** User's trace-driven continuation request and docs/FIRST_NATIVE_BUILD.md.

## Global Constraints

- Preserve WPSE01_01 / WPSE / 0001000157505345 and the interpreter-OFF AOT architecture.
- Never publish Nintendo files, generated title code, traces, or game binaries.
- Preserve all user changes in the existing worktree.
- A valid SYSCONF is independently required; do not claim it causes the title's Japanese resource selection without execution evidence.

## Review Focus

- Full 0x4000-byte file and SCv0 / SCed framing, not just a language byte.
- Big-endian count and count+1 absolute offsets, contiguous records and terminal offset.
- Byte type and name-length encoding; no C-string terminators inside names.
- Device read cursor, physical RAM alias, EOF and untouched destination tail.
- Only original synthetic settings; no persistent NAND support or unrelated FS changes.

### Task 1: Guest-readable synthetic configuration

**Files:** Modify CMakeLists.txt and third_party/NWiiRecomp/nWiiRuntime/src/hle/devices/fs_device.cpp. Create tests/sysconf_tests.cpp. Update docs/FIRST_NATIVE_BUILD.md.

**Interfaces:** Consume devices::create_fs_device(), IDevice::open/read/seek/close and CPUContext::mmu. Produce the same /shared2/sys/SYSCONF file, now structurally valid; no new public API.

- [x] Write a real FS/MMU regression that parses all byte records independently, expects IPL.LNG=1 and IPL.AR=0, validates framing/offsets, and checks chunked reads/EOF/tail preservation.
- [x] Build and run sysconf_tests; expect failure at the SCv0 header with the existing implementation.
- [x] Replace make_sysconf with a zero-filled 0x4000-byte SCv0 file, two byte records (IPL.AR, IPL.LNG), count=2, offsets 0xC/0x14/0x1D, SCed at 0x3FFC.
- [x] Run all Release and Debug CTests; sync only the runtime source into ignored generated output, rebuild the interpreter-OFF title, run a bounded trace and record actual results.
- [ ] Obtain independent review, run repository-policy checks, commit and push only redistributable files.

## Evidence ledger

Base: f23f080. Pre-flight: one task, no shared implementation interfaces.
Ruling: continue inline in the existing checkout under the user's autonomous continuation request; do not pause for plan approval or create a replacement worktree. Plan and evidence are kept together here for Windows portability.
Baseline: guest parser rejects the old SCv15 header, missing footer and invalid offsets. Language trace did not call SCGetLanguage before the failed texture lookup, so locale causality is not established.

RED: sysconf_tests failed "SYSCONF header must be SCv0". GREEN: standalone test passes; complete Release24/24, Debug8/8. Synthetic export and policy tests pass.
Control title run post-sysconf-20261010.log still chose Japanese and reached the same TPL panic; missing low-memory identity is the separate proven cause, addressed by the boot-game-id plan.
Independent review: no actionable findings. Unchanged general FS seek/read safety and setting.txt are deferred, not claimed solved. Original synthetic file only; no imported NAND.
