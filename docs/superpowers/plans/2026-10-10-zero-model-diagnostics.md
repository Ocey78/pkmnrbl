# Zero-position model draw diagnostic plan

**Goal:** Find the source of the model vertices that reach perspective rendering as exactly (0,0,0) after MEM2 list expansion was restored.

**Evidence:** The user-confirmed e-mem2-dl-20261010 run still displays captions over black. New perspective batches have36/72/360/432 vertices with zero view bounds and nonpositive W. Accepted list calls and vertices increased substantially; the small residual left-clipped draws remain. This is not evidence that projection, shading or depth should be bypassed.

**Architecture:** Add a narrowly bounded, default-off scalar observation at deferred draw decoding. Keep all guest transforms, cache behavior, graphics state and ordering unchanged. Do not compare a draw's snapshot with the parser's final state or label post-transform positions as raw object positions.

## Steps

- [x] Add an original lit indexed-matrix fixture: known nonzero object vertices, selected zero matrix slot while another bank slot is nonzero. Assert actual decoded zeros and require the diagnostic to distinguish the input and selected matrix. Observe missing diagnostic RED first.
- [x] Under NWII_RENDER_DIAG, record bounded lit-draw input/count/format/array details, object bounds, actual selected transform source and zero-matrix count, representative translation/linear maximum, and decoded bounds. No title payload dumps.
- [x] Verify diagnostic integration plus complete Release/Debug suites and fresh review.
- [ ] Copy only attributable runtime changes after backups.
- [ ] Rebuild native OFF and rerun real title. Use the new observation to select one proven minimal behavior correction with its own failing regression.

Separate candidates remain unproven as this zero-position cause: stale full-bank snapshots on cache hits; invalid indexed position arrays; direct XF commands with uninitialized cache-marker fields. Private synthetic investigation may distinguish their independent correctness contracts, but do not bundle fixes into the diagnostic patch or claim full scene recovery from unit tests.

Verified registered suites:Release70/70,Debug41/41. DiagnosticRED/disabledcontrol
thenGREEN, first16 lit draws then every4096 to128records. Independent review
found no issues and noted other scalar branches/cap are code-reviewed only.
