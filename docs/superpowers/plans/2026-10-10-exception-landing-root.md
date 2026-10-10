# Observed exception landing root implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline with test-driven-development.

**Goal:** Cover the dynamically selected exception landing pad observed in the native-only boot trace.

**Architecture:** Reuse the title's existing additional AOT-root CSV; no runtime fallback, branch bypass or new exception implementation. Extend its original synthetic fixture to prove a metadata-selected interior block reaches native code and preserves caller state.

**Tech Stack:** C++20 synthetic AOT fixture, existing CSV root loader, NWiiRecomp.

**Spec:** User's native-only continuation; e-clip-planes-20261010 fails at8019EAD0, LR same, CTR803419B8 before Save A.

## Constraints and review focus

- Verify the local DOL/unwind metadata selects this block, rather than registering a corrupted return address.
- Preserve existing indirect branch/callback roots and both split/single output tests.
- Fixture instructions must be original, never copied from the DOL.
- Keep interpreter OFF, all generated game code/assets/evidence local and unrelated changes intact.
- This addresses coverage only; do not claim the underlying title exception or scene clipping is fixed.

## Task 1

- [x] Confirm exception target from local metadata and code.
- [x] Extend tests/aot_title_roots_fixture.cpp and tests/aot_title_roots_tests.cpp with an interior handler block skipped by normal control flow; dispatch to it dynamically and verify register effect, exact single execution and restored caller return. Both variants must fail before config change.
- [x] Add only the observed root to config/WPSE01_01/aot_roots.csv; rerun full Release suite and both Debug root tests.
- [x] Isolated local export, inspect attributable output/dispatch changes, back up replaced generated files; rebuild native title OFF.
- [x] Fresh review, source-only checkpoint and real run with clipping plus display-list diagnostics.

Checkpoint a04592c passed Windows CI. Native build:8m29s,499 warnings,0errors.
The real e-landing-root-20261010 run has started; results remain pending. Completion
of this coverage task does not establish full unwind correctness or scene rendering.
