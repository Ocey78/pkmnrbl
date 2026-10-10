# Save-start trace and automatic-hook correction

Continue the user's existing native AOT project in place. The title menu and
requested controls are verified; the user reports black output after starting
a save. Do not equate loading, missing storage, and that reported black screen.

## Evidence and scope

- Preserve private bluetooth-trace.txt and the timed save-start trace in build/.
- Local generated NAND create-file/dir wrappers802E4830,802E4950,802E5180,
  802E52A0 were replaced by IOS_Ioctlv despite different argument contracts.
  The short generic prologue matcher can match unrelated functions; it writes
  hle_hook_name directly, bypassing the normal conservative symbol allowlist.
- The local DOL still calls ISFSCreateFileAsync/ISFSCreateDirAsync from these
  wrappers. A path pointer becomes a negative descriptor in the old trace.
- FSDevice also lacks creation/metadata ioctls and host persistence. This is a
  separate limitation; implement only behavior proven necessary by the trace.

## Work

- [ ] Capture New Game/save attempt with timed rendering/CPU/IPC evidence.
- [x] Add an original synthetic function with the ambiguous generic prologue
  but unrelated behavior; prove the analyzer wrongly selects IOS_Ioctlv.
- [x] Remove only the proven unsupported automatic prologue matches, preserving normal
  AOT execution and explicit verified hooks. Compile/execute synthetic output
  in both layouts, proving the function body remains native and runs correctly.
- [ ] Full tests, independent review, regenerate/rebuild interpreter-OFF title,
  bounded real boot/save test and evidence ledger. Never upload generated code.
- [ ] Update handoff, commit and push verified redistributable changes. Separate
  any next storage implementation behind its own failing regression first.

Execute inline/autonomously as requested. Parallel agent may own scanner and
synthetic regression files; root owns real runs and documentation. Preserve the
user's active game until it closes; no force-stop of their launcher process.

## Evidence ledger

Scanner RED: all three analyzer/split/single synthetic tests failed before the
change. GREEN after removing only the synchronous IOS_Ioctlv matcher: native
body sets r6=37, helper runs once, argument captures and SP/LR are preserved,
and neither IOS nor interpreter stub is called. All31 Release CTests pass in a
fresh root build/run. Independent read-only review has no actionable findings.
Explicit-hook tests and standalone export/policy checks pass.

The longer save-start diagnostic did not capture a confirmed save action. Its
last screenshot remains the title menu. At approximately140 seconds it exits1
with a native-only coverage error: PC8028F440, LR80276D3C, CTR8028F440. This is a
separate proven indirect-target discovery failure, not proof of a save hang.
Map the original target and caller, add an original failing regression, then
restore native AOT coverage; do not enable the interpreter. Private evidence:
build/logs/save-start-20261010.log/.err and _12000.bmp.

Target mapping:8028F440 is a separate DOL function ending with blr8028F588;
the previous generated range ends exclusively at8028F440 and the next begins
8028F590. Caller80276CD0 loads the callback from object+38 and executes bctrl
at80276D38. Setup8028E3B0 constructs the address with lis8029/addi-0BC0 across
branches/calls, then passes it to802768A0, which stores it at object+38. There
is no literal pointer word for the discovery pass to find. Add the verified
address to the existing title-root CSV, not a general speculative scanner rule.

- [x] Original synthetic callback regression must fail without the new CSV root,
  pass with it in both layouts, and verify bctrl continuation and native body.
- [ ] Re-export and rebuild actual title with interpreter disabled; repeat a
  longer menu run and attempt save reproduction when user input is available.

Follow-up audit proved both remaining IOS_IoctlvAsync matches802E4630/4740 call
synchronous ISFSCreateFile802F4470 (command9,input76), with path/permissions/
attributes arguments. Removed that second unsupported matcher after extending
the original fixture: all3 tests fail first on the async variant, then pass
native behavior73, helper once, exact return and preserved r30/r31. Independent
review passed both variants and the callback-root change. Final root CTest run
passes31/31 Release; Debug10/10 remains passing.

Integration is blocked by local disk space, not by a claimed source success.
The full generated-title compile was stopped; latest async removal still needs
nwiirecomp.exe relinking and export. The old legacy configure smoke test copied
the workspace into a temporary duplicate; stopping it left under1GB free and
cleanup calls were rejected. User was asked to delete only that test-created
copy. No original files were deleted. Resume instructions are in FIRST_NATIVE_BUILD.
