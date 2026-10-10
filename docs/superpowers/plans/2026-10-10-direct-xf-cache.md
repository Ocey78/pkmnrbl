# Direct XF display-list cache regression plan

**Goal:** Preserve immediate matrix payloads across a cached display-list replay.

**Proven defect:** Direct XF parsing constructs GXCommand without setting val.
Cache replay reads val's high bit to distinguish an indexed matrix load. In an
original Debug production-code probe, the first direct matrix draw decodes to
(8,-1,-8); all four verified cache replays in both MEM1 and MEM2 replace its
matrix with zeros and decode to(0,0,0). No fields were manually poisoned.
The probe has60 checks and16 expected failures. This matches the kind of zero
collapse observed in the title, but does not yet prove every zero title draw
has this cause.

## Steps

- [x] Promote original fixture to its own standalone CTest target; independently observe tracked RED. Keep genuine indexed LOAD_INDX behavior covered as a control.
- [x] Set the direct XF command's cache marker to the direct-kind value at creation; do not change cache selection, matrix arithmetic, guest code or draw state.
- [x] Run full Release/Debug tests and independent review. Copy only checked runtime differences after backup; rebuild native OFF.
- [ ] Run real title with bounded zero-model diagnostics; record actual visual outcome and remaining blockers. Commit/push source only.

The concurrent diagnostic patch is observation-only and should be checkpointed
separately. Stale indexed matrix-bank snapshots remain outside this behavior fix.

Tracked Debug RED:84checks,16failures; direct misses and genuine indexed controls
pass before fix. GREEN:Release71/71,Debug42/42; fresh review approved. Single-file
generated preimage matched6c312ef, backed up, replacement SHA256verified. Native
OFF build4.01s,0warnings/errors. Real e-direct-xf-20261010 started; visual result
not yet known. Diagnostic-only checkpointb5999d8 already pushed separately.
