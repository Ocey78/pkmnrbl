# Local ES content service implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Replace proven empty ES content responses with read-only local content access, then resume title tracing.

**Architecture:** Retain the existing AOT runtime and IOS device routing. ES resolves content indices through a local signed TMD, opens content-ID-named files, and maintains independent seek positions. No content or translated game code enters the repository.

**Tech Stack:** C++20, real NWii MMU, synthetic CTest fixtures, MSVC x64.

**Spec:** User continuation request and docs/FIRST_NATIVE_BUILD.md, Work still required item 2.

## Global constraints

- WPSE01_01 / WPSE / 0001000157505345; NWiiRecomp AOT, no emulator runtime.
- Preserve current checkout and user helpers; keep interpreter OFF during title verification.
- Only original runtime/tooling/tests/docs may be committed. Local inputs, generated code, executables and traces stay ignored.
- Execute autonomously as requested; no plan-approval pause. This is one tightly coupled debugging change, implemented inline with independent review.

## Review focus

- Index is not content ID: synthetic index 5 maps to ID 42.
- Invalid vectors and RAM spans must fail without touching memory or advancing position.
- EOF/partial reads leave the unused destination tail intact.
- Bad metadata, wrong title or truncated content must fail instead of fabricating success.
- Distinct handles and ES sessions must not share position or invalidate each other.

## Task 1: ES open/read/seek/close

**Files:** tests/es_content_tests.cpp; CMakeLists.txt; runtime ES device and IOS request/kernel files; new runtime devices/es_content.h and devices/es_content.cpp if needed; docs/FIRST_NATIVE_BUILD.md.

**Interfaces:** Existing create_es_device and IOSKernel open/ioctlv/close. Commands 09(1 input,0 output), 0A(1,1), 23(3,0), 0B(1,0); scalar vectors are four-byte big-endian values. Return handle, byte count, new offset or negative error respectively. Use ES invalid argument -1017. Map signed TMD records to local content IDs; accept the configured game's title.tmd or title-ID-named TMD in game_dir or its parent. Reject missing/invalid metadata; no guessed index filenames. Bound reads to TMD logical length (allow extraction alignment padding). Size/hash of real local content is checked separately before use; cryptographic signature authentication is not claimed.

- [x] Write a synthetic signed RSA2048 TMD and original 37-byte content fixture. Open index5 mapped to ID42, read32, seek0, read37, then EOF. Assert real bytes, return counts, untouched tail and independent handles. The current zero-success stub must fail the first 32-byte read.
- [x] Build/run es_content_tests and record the expected failure before runtime changes.
- [x] Implement the bounded read-only service and session ownership; cover close, invalid handle/index/vector/span, SET/CUR/END signed seeks, malformed/wrong-title TMD, missing/truncated content and concurrent sessions with tests.
- [x] Run all Release CTests; ensure asset-free Debug CI builds/runs the new test.
- [x] Sync runtime changes to ignored generated runtime, rebuild interpreter-OFF PokemonRumble.exe, run bounded real title and inspect output. Record the next exact blocker without claiming a title screen prematurely.
- [x] Independent code review, repository policy/staged diff checks, commit and push redistributable milestone; continue trace-driven work.

## Evidence ledger

- Base d38529a, tracked worktree clean; eight preexisting untracked helpers preserved.
- Fresh trace es-baseline-20261010.log: five index5 opens, 32-byte reads return0; header data-offset becomes0, so next requested read length is0. Local index5 TMD size/hash match its local file. No new diagnostic code needed.
- Pre-flight: one task; tests exercise the existing IOS/MMU boundary, runtime changes and real-title verification consume that same boundary. No competing interfaces.
- RED: first header read returned0 instead of32. GREEN: 23/23 Release, 7/7 Debug; standalone synthetic export and repository-policy tests pass. Existing upstream compiler warnings remain.
- Real title: header32, metadata2944 and resource716800 reads succeed. New OSPanic from TPL.c line25; first texture pointer valid, second null. No title-screen claim; bounded launcher killed the halted process.
- Independent review: no remaining actionable findings after production source-list registration; zero-length reads intentionally return -1017, with explicit test. Cryptographic authentication/NAND installation and earlier raw-IPC parsing remain outside this bounded service and are not claimed fixed.
- Committed/pushed f23f080; Windows CI 38025262567 succeeded.
