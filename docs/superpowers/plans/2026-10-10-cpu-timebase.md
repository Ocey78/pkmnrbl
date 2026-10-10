# CPU timebase implementation plan

> **For agentic workers:** Use superpowers:executing-plans for this inline task. Preserve the existing E-drive checkout and the user's local files.

**Goal:** Test a coherent virtual CPU/TB clock against the observed render starvation, then continue native New Game debugging.

**Architecture:** Retain ahead-of-time execution. Treat the existing raw counter as approximate CPU-cycle charges, with 12 charges per TB tick; do not claim cycle-accurate instruction timing. Keep event and DEC deadlines in TB units and convert idle deadlines back with checked integer arithmetic.

**Tech stack:** C++20, MSVC x64, CMake/CTest, existing NWiiRecomp runtime.

**Spec:** User's native continuation request and `2026-10-10-save-start-trace.md`; private measurement is `build/reviews/video-timing/audio-budget-evidence.md`.

## Global constraints

- WPSE01_01 / WPSE / 0001000157505345; interpreter OFF.
- No Dolphin runtime, JIT, game-branch patches, forced rendering or gameplay rewrite.
- No Nintendo files, generated game code, private logs or assets in GitHub.
- Preserve NWII_WALLTB, existing event periods, interrupt gates and SDK-idle validation.
- This is a controlled clock-policy experiment, not proof that missing DSP processing is harmless.

## Review focus

Fractional counters; idle inverse overflow; earliest DEC/event ordering; wall-time invariance; diagnostic raw-counter/TB distinction. Each is tested below.

### Task 1: Separate execution charges from TB ticks

**Files:** `third_party/NWiiRecomp/nWiiRuntime/include/runtime/cpu_context.h`, `src/core/guest_timing.cpp` below that runtime, `tests/guest_timing_tests.cpp`, `tests/timing_trace_tests.cpp`, root `CMakeLists.txt`.

**Interfaces:** `CPUContext::read_timebase()` remains uint64_t. Add `bool advance_virtual_timebase(uint64_t deadline)` for forward-only, checked idle conversion; unchanged calls still increment `inst_count`. EventScheduler remains in TB ticks.

- [ ] Write and run failing tests: raw0/11/12/23/24 -> TB0/0/1/1/2; CPU486M and729M charges -> one respective guest second. DEC2 written at raw13 expires at raw36, not35. Event10 fires at raw120, not119. Idle reaches raw1200 for DEC100; same/past deadlines retain fractions; unrepresentable deadline refuses advancement; largest representable deadline works. Wall mode refuses virtual advancement.
- [ ] Add a busy-work event-budget case: 729000 charged steps are1ms, so a5ms recurring event has not fired; at3645000 it fires once. Expectations are literal, independent of implementation helpers.
- [ ] Implement ratio12 and checked inverse; retain all guard semantics and exact event periods. Adapt existing test inputs from TB ticks to raw charges, preserving their assertions about guest behavior. Keep trace tests checking distinct raw and TB fields.
- [ ] Run focused tests, then all Release and Debug tests. Independent review of the complete task diff.
- [ ] Copy only changed redistributable runtime files into local generated runtime, verify hashes, rebuild the full interpreter-OFF native executable. No re-export or AOT body edits required.
- [ ] Run the real title with the same device model, capture sustained menu rendering and confirmed New Game/Save Data A transition. Compare with baseline; do not infer cutscene/gameplay success from tests or menu alone.
- [ ] Update handoff with actual evidence, commit and push the redistributable milestone. Trace the next proven blocker separately.

Ruling: execute inline and autonomously, with independent read-only audits/review, under the user's explicit keep-building direction. Reuse the feature checkout and ignored `build/reviews/cpu-timebase` ledger; no new worktree or bash workspace script. Cost if the clock hypothesis is wrong is a reversible runtime-policy change and full local rebuild, not changes to game code or saves.
