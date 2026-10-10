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

## E-drive continuation

The user moved development to the copied E-drive workspace and requested boot
through the cutscenes into gameplay. The active repository is now
E:/Projects/pkmnrbl-workspace/work/pkmnrbl. All 46,012 copied workspace files
were checksum-verified before this continuation. C-drive cleanup was blocked;
leave the C copy untouched and do not reuse its cached build paths.

Ruling: preserve the copied Release/Debug build directories as
build/windows-release-c-copy and build/windows-debug-c-copy, then configure
fresh default build directories on E. Reuse only the pinned dependency source
directories on E, not their old build caches. This costs a full recompile but
avoids building or modifying the stale C workspace. Compiler temporary files
also go to build/tmp on E.

- [x] Inspect E Git status/diffs; preserve the eight untracked user helpers.
- [x] Configure a fresh E Release host build with explicit bundled Python.
- [x] Build and run the complete synthetic regression suite on E: Release31/31
  and fresh asset-free Debug10/10 pass. GPU test executes successfully.
- [x] Re-export using the latest recompiler and rebuild the title with
  PKMNRBL_ENABLE_BRINGUP_INTERPRETER=OFF explicitly retained.
- [ ] Capture a real save/New Game transition, then trace each blocking
  operation before adding its original failing regression and minimal fix.
- [ ] Verify cutscene progression and gameplay in the actual title; menu
  rendering or a green synthetic suite alone does not satisfy this milestone.
- [ ] Update the handoff and push only verified redistributable source/docs.

Fresh export e-save-export-20261010.log succeeds with14,375 functions and both
signature removals included. Fresh generated cache has interpreter OFF and all
source/build paths on E. Full optimized build completed in 9m24s with zero
errors (499 existing compiler warnings); /MP4 limited concurrent compilation.
The fresh executable is 51,639,808 bytes. Do not use the preserved old executable
as proof. The generated project compiles native_only.cpp, not interpreter.cpp.

Save diagnostic contract (read-only local-code audit): create-file game801B0650
calls NAND802E4830, ISFS802F4570, IOS802F2490 with command9,input76. Completion
passes802F3560 ->802E6DD0 ->801B0640. The game callback userdata is stack-frame+28;
the result is userdata+C8 and semaphore userdata+BC, with waitPC801B06D0.
Directory counterpart801B0A30/802E5180/802F37E0 uses command3 and wait801B0AB0.
Current old traces do not capture create9, so this remains a diagnostic contract,
not the proven cause of the reported black screen. File-stat command11 also needs
internal-descriptor routing if reached; IOSKernel::ioctl currently omits device_fd.

### First fresh E title run: reply-header failure

Private evidence: build/logs/e-save-baseline-20261010.log/.err. The diagnostic
remained at PC802B4064, LR802F14BC, frames60/draws0 before any menu input. It was
stopped deliberately after recording the persistent state, not marked passing.
The completed request immediately before the stall was WRITE command4 on fd7
at93E00140, writing4000 bytes to the synthetic SYSCONF. Earlier command9 is now
visible in the real create-file path; do not confuse it with a save-game action.

Local mapping: reply handler802F1350 loads request+8 at802F13B4, dispatching
READ3/IOCTL6/IOCTLV7. At802F14B8 it calls DCInvalidateRange802B4040 for an ioctlv
element. The existing runtime writes only result+4, leaving fd7 in the union
field that the reply consumer expects to contain the original command. A WRITE
therefore follows the IOCTLV buffer walk. This is a reply-format bug, not an
input, Bluetooth, video, or scheduler-idle diagnosis.

Primary independent ABI reference: devkitPro/libogc libogc/ipc.c, struct_ipcreq
fd/req_cmd union at byte8 and __ipc_replyhandler dispatch on req_cmd:
https://github.com/devkitPro/libogc/blob/master/libogc/ipc.c

- [x] Original synthetic regression: complete WRITE with fd7; reply must carry
  command8 at+0 and original command4 at+8, preserving result/payload/callback.
- [x] Cover deferred completion and pending-request nonmutation, preserving
  existing acknowledgement ordering and queued replies.
- [x] Minimal shared completion fix, complete suite, independent review, native
  rebuild and another real run before claiming that this stall is resolved.

RED confirmed in Debug ipc_async_reply: command1..7 fail the new envelope
assertions; WRITE/fd7 specifically expects command4 at+8 but reads7. The old
result, argument-tail, pending-request and queue checks still pass. The fixture
uses original synthetic64-byte packets and the real transport/MMU; only the
device-operation boundary is substituted. It covers MEM1/MEM2 physical,
cached and uncached packet addresses, success values and signed errors.

GREEN: ipc_post_reply converts the envelope once; the immediate path now joins
that same deferred-completion path. Focused Debug passes, then full Release31/31
and Debug10/10 pass. Independent two-file review has no actionable findings.
The unused legacy hle_set_ipc_arm_msg remains outside this change.

Actual native integration: copied the changed redistributable hw_ipc.cpp into
the generated runtime template and verified matching SHA-256, leaving all AOT
translation units unchanged. Incremental title build succeeds with no warnings
or errors; interpreter remains OFF. Private e-reply-fixed-20261010.log/.err and
_10800.bmp verify the actual New Game / Options title menu. At80s:3771 frames,
66,528 draws,40 shaders,305,823/307,200 nonblack pixels, GL error0. This resolves
the diagnosed reply/cache-loop stall, not the unverified save/gameplay milestone.

Live Computer Use window capture failed twice (FrameArrived/window capture
timed out), so no blind UI input was sent. The title's own screenshot confirms
the menu; the user was asked to start New Game while the600-second diagnostic
records the next transition. No persistent save or cutscene claim is made.
