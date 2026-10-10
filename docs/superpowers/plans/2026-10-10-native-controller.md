# Basic Native Controller Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect an actual native input source to the title's verified WPADProbe/KPADRead API seam and test progression past the disconnected-controller screen.

**Architecture:** Retain NWiiRecomp's native SDK compatibility layer and all title game logic. Opt in only the two verified SDK functions through per-title HLE hooks. Produce a complete 0xB0 basic-remote KPAD record from the native source; do not fabricate Bluetooth ACL/HID completion or patch the game's disconnect checks.

**Tech Stack:** C++20, InputManager, real MMU, generated AOT hooks, CTest/MSVC.

**Spec:** User's native AOT/title-screen goal; docs/FIRST_NATIVE_BUILD.md. Guest main poll801B1810 calls WPADProbe802FA010 and KPADRead803070D0 with capacity1. The game initializes connected state directly from Probe==0 in8017A3E0. Raw Bluetooth state is not sufficient input support.

## Global Constraints

- WPSE01_01 / WPSE / 0001000157505345; no interpreter, emulator runtime, or gameplay rewrite.
- Never publish game data, generated translated title code, executable, or private diagnostics.
- Preserve current checkout and unrelated changes; continue autonomously.
- Basic native remote only. Do not claim physical Bluetooth, extension, calibration, speaker or full callback support.

## Review Focus

- Complete0xB0 record, signed error at5D/type5C; no old stub's partial50-byte writes.
- Press/hold/release and disconnect/reconnect behavior, independent channels.
- Invalid capacity/channel/null/out-of-RAM pointers leave memory and edge state untouched.
- Coherent native input snapshots between host/render thread and guest CPU thread.
- Hook names link in both generated layouts and only opt in verified addresses; no global automatic hooks.

### Task 1: Basic SDK read/probe compatibility

**Files:** tests/native_wii_input_tests.cpp; CMakeLists.txt; runtime hle/pad.cpp; input/input_manager.h/.cpp.
**Interfaces:** existing extern C KPADRead(CPUContext&) returns sample count; new WPADProbe(CPUContext&) returns signed connection status and optional u32 remote type. Consume InputManager snapshots; no title-specific state writes.

- [x] Add real InputManager/MMU regression for a source reporting connected remote and a full0xB0 destination prefilledA5. Require one sample, valid basic type/error, deterministic unused bytes and untouched tail. Run against old stub and observe failed status byte.
- [x] Implement complete basic record and Probe; add edge, disconnect, channel, pointer/range and capacity checks. Synchronize snapshot publication/reads.
- [x] Build/run focused tests and full Release/Debug suites. Input mappings must provide real buttons1/2 plus directions without bit collisions.

### Task 2: Explicit title bindings and real boot

**Files:** config/WPSE01_01/recomp.toml; recompiler.cpp declarations; generated-project regression coverage if required; docs/FIRST_NATIVE_BUILD.md.
**Interfaces:** hle_hooks maps802FA010 toWPADProbe and803070D0 toKPADRead; generated wrappers retain normal LR return. No automatic allowlist change.

- [x] Ensure generated hook declarations link; run synthetic generated-output tests in both layouts with an originally undeclared custom hook.
- [x] Rebuild/export title and interpreter-OFF executable; bounded real run must either render title screen or provide exact next gate. Keep generated code private.
- [ ] Independent review, fresh full tests and policy checks; commit/push verified redistributable milestone and update handoff.

## Evidence ledger

Base d3c4956. Long100-second native-only run exits0 with empty stderr, GL0 and a visible controller-disconnected screen after loading. SDK addresses/record size come from local guest callers, not a guessed symbol map.
Ruling: use the existing high-level SDK seam for the basic native controller; lower Bluetooth transport is not required by the proven initial probe/read path. Other WPAD controls remain unverified and may be the next gate. Keep this limitation explicit.
Ruling: execute inline in the user's existing checkout; keep ledger here for Windows portability and continue without plan-approval pauses.

Task 1 RED: old KPADRead left signed error byte5D at A5. GREEN: real InputManager/MMU test checks all0xB0 bytes, edges, reconnect, channels, argument/range failures and concurrent coherent snapshots. Review identified the Probe-gated reconnect path; its new test failed first, then passed after resetting only that disconnected channel's history in Probe. Reviewer rechecked and closed the finding.

User-requested controls: left mouse2, right mouse1, WASD sideways, Escape+, Tab-, E=A. Real MouseKeyboardSource regression failed old left-mouse bit0800 versus0100, then passed every mapping, combinations/releases, channel isolation and unchanged GameCube controls. Test SDL shim substitutes only the host input boundary.

Task 2 RED: synthetic split/single generated output failed MSVC C3861 for undeclared NativeControllerProbeTest. GREEN: namespace C declaration before each explicit hook wrapper links and executes exactly once, returns to LR and never runs guest body/interpreter. Shared functions.h emission is unchanged. Windows CI now explicitly builds/runs these tests.

Fresh verification: 28/28 Release and10/10 Debug tests pass, including actual GPU execution; synthetic standalone export and repository policy pass. Independent review has no outstanding findings for this limited milestone. Interpreter-OFF title export/build succeeds; first relink was delayed by the user's running game, which was preserved until the user closed it.

Real integration: after the user closed the game, relink succeeded with the reconnect fix. A100-second interpreter-OFF run exits0 with empty stderr and GL0 throughout. Screenshot native-controls-20261010_6000.bmp shows the actual title menu (New Game / Options). User independently confirms controls worked and reached the save screen, then reported black output after attempting a save. Save/gameplay are the next investigation, not part of this completed basic-input validation.
