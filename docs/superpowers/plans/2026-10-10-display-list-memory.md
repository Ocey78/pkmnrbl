# Display-list memory contract plan

> **For agentic workers:** Execute inline with systematic-debugging and test-driven-development. Delegate the independent original fixture; parent owns implementation and integration.

**Goal:** Preserve correct MEM1/MEM2 display-list addressing and reject invalid guest spans without reading host padding or unrelated memory.

**Architecture:** Retain the existing FIFO parser and snapshot/cache design. Use the production MMU guest-bank ranges and canonical physical address for complete-span validation and cache identity. Do not change graphics state or CPU architecture.

**Evidence:** Private real-code probe has43/57 passing memory assertions and14 contract failures: MEM2 lists silently dropped, snapshot bank collision/wrong-bank reads, invalid snapshot spans cached. The title attribution remains pending in the current diagnostic; these are independently proven runtime correctness defects, not yet an explanation of every clipped draw.

## Steps

- [x] Promote only the original asset-free memory fixture into a standalone CTest target linked to the real parser/snapshot implementations; independently observe RED. Keep unrelated boundary stubs explicit.
- [x] Check real-run display-list addresses and clipping results before selecting the next title rendering hypothesis.
- [x] Implement the smallest shared address/span rule for parser and snapshot, preserving size/depth limits, cache policy and FIFO sequencing. No changes to indexed-matrix caching in this patch.
- [x] Run the new regression in Release/Debug and all available tests. Fresh independent review.
- [x] Run repository policy and create source-only checkpoint.
- [x] Copy only verified attributable runtime changes to local generated code after backup, rebuild interpreter-OFF and rerun title. Source-only checkpoint with actual outcomes; no title payloads, logs, screenshots or generated code.

## Contracts

- MEM1 and MEM2 physical/cached/uncached aliases resolve to their own bank.
- Snapshot keys distinguish banks while aliasing addresses within one bank.
- Exact-end spans are valid; crossing, wrapping, unmapped and MMIO spans are rejected before host reads.
- Parent FIFO commands remain consumable after rejected calls.
- A successful generic regression does not establish a rendered cutscene or playable gameplay.

## Verified execution

RED:57 assertions,14 intended failures, independently observed by parent and
fixture agent. GREEN:57/57; complete Release68/68 and Debug39/39. Fresh review
approved, including final Fetch-only-cache lookup that keeps invalid-insertion
tests meaningful. Native OFF build5.24s,0warnings/errors. Local runtime copies
were backed up and hash-verified. Source-only checkpoint6c312ef pushed. The real
e-mem2-dl-20261010 run submits previously rejected model geometry, but its view
coordinates collapse tozero; the user and capture still show captions on black.
The prior user-confirmed caption run proves repeated MEM2 calls reject;
the snapshot seam is dormant, not an attributed active title cause.
