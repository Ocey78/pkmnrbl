# Single-precision update AOT continuation

User's existing native-game continuation, inline in E checkout. Preserve local
files, title identity, interpreter OFF and asset exclusion.

New evidence after bdfad84: Save Data A reaches repeated indexed paired loads,
then native failfast at80252450/C4040008. Local DOL confirms lfsu f0,8(r4).
Emitter lacks immediate single-precision updates49/53; double forms51/55 exist.

1. Original synthetic native regressions for lfsu/stfsu: signed displacement
   boundaries, zero/positive/negative offsets, uint32 wrap, FPR/GPR same numeric
   index, both single-load lanes, one-word store, preserved registers/sentinels.
   Invalid RA0 must remain unsupported. Prove RED, with interpreter forbidden.
2. Emit scoped uint32 EA, transfer via existing MMU, write RA after transfer.
   Do not change double forms, helper rounding or interpreter fallback.
3. Test Release and Debug, independently review, isolate local re-export,
   inspect only attributable changed AOT sites and preserve runtime.
4. Rebuild actual title OFF, request the user's Save Data A input where native
   window capture remains unavailable, trace next state and checkpoint safely.
