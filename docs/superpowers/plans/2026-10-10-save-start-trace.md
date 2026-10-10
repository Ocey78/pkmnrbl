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

### Confirmed New Game black screen and video catch-up starvation

Milestone34fc73f was committed/pushed; Windows CI run38030802937 passed.
The user confirmed starting New Game and seeing black in e-reply-fixed, then
confirmed selecting Save Data A and leaving the second e-frame-gate run open.
The first run exits normally after window closure, not a native crash. The
second stayed black for over two minutes before its owned diagnostic process
was stopped. Frames/draws stabilized at4652/143629, shaders58; IRQ/audio activity
continued. It is not valid to infer a filesystem failure from the black output.

Local AOT/DOL mapping identifies the frame loop8019DC50 and its vtable:
update8012EF50, predicate8012EEB0, render8012EBE0. The predicate is not itself
the render gate. At8019DD64/DD74 the catch-up loop repeats updates whenever
signed r25 > r24 and byte804B9110 was zero. Per update r24 +=50; r25 increases
by50 * elapsed VI retraces, obtained from804B9674 through802C6510.

Third private trace e-catchup watches804B9110 with registers and peeks the
VI count. The watch's printed PC8019DD50 is a stale block PC; the actual store
is8019DD70, verified in local source/instructions. At150-155s,31 updates and
zero renders occur while debt grows96050 ->114750. By185-190s the debt grows
348850 ->388600 (7772 updates owed), with56 updates and zero renders. VI advances
about850 times per five host seconds. Frames/draws remain5545/236018. The third
run was still at the menu in its final screenshot, so the timing defect can
also freeze menu rendering without a save action. Root stopped this diagnostic
after preserving the evidence; no save interaction is claimed for this run.

Runtime cause: main.cpp sets vblank_pending after every host presentation,
and ios.cpp converts those pulses into VI, independently of a second recurring
EventScheduler VI at tb_freq/60. Host refresh therefore adds extra guest ticks.
Removing the host source alone is insufficient: PC0 idle sleeps without
advancing the virtual instruction-count clock. The next bounded change must
retain one guest-time VI source and advance that same clock to the earliest
event/decrementer only while genuinely idle, preserving wall-clock mode and
interrupt delivery/coalescing. Do not bypass the title's catch-up branch.

- [ ] Real production-timing regression RED, then minimum single-clock fix.
- [ ] Test cadence, host-hint independence, pending/masked interrupt behavior,
  same-tick/late polling, idle event/DEC progress and wall-clock preservation.
- [ ] Independent review, complete test suites, native runtime rebuild and
  actual New Game/cutscene/gameplay verification. This is not yet a fix claim.

First timing iteration: RED reproduced120/240 host-dependent VIs instead of60
and lack of PC0 idle/earlier-DEC progress. Single guest-clock VI plus PC0 idle
advance passes13 new timing cases (both40.5MHz default and60.75MHz Wii clocks),
full Release44/44 and Debug23/23. Runtime-only title rebuild succeeds with
interpreter OFF and zero compiler warnings/errors. Separate implementation
review found only a wall-clock-test preemption sensitivity, to be corrected.

Actual e-single-vi diagnostic proves catch-up debt now clears and rendering
continues, but startup crawls at roughly3.5 guest frames/sec. Local mapping
shows repeated PC802BC278/LR802BC26C is a genuine SDK scheduler idle loop,
not expensive scene work: currentthread800000E4 is zero, all seven active
threads are state4 (waiting), main80452938 sleeps on VIqueue804B9650, and the
loop polls ready bitmap804B9508. DOL instructions230/25C/270/274/278 agree.
PC0-only detection misses this valid idle state. The next test-first refinement
must validate a complete nonempty all-waiting SDK thread list/current context
and enabled interrupts before advancing to the next event, without any
title-PC special case or render-branch patch. Invalid/uninitialized/ready
states must not qualify. Prior suite totals do not yet cover this refinement.

Refinement RED: nonzero-PC SDK idle does not reach Wii VI1012500 or earlier
DEC100 with the PC0-only helper. GREEN after validating SDK active headDC,
tailE0, next+2FC, previous+300, state4, EE/current-thread/context and full RAM
bounds. Local DOL independently confirms this layout. Synthetic tests cover
two waiting threads across MEM1/MEM2,28 invalid/startup/list/state cases and6
in-flight/pending-work guards. No title PC/constants or generated game bytes
enter the helper or tests. The wall-time test's preemption-sensitive repeated
poll assertion was removed after review; virtual tests retain deterministic
same-tick/no-burst assertions.

Final source verification: Release48/48, Debug27/27 pass; separate review and
scoped fix review have no remaining findings. Host build succeeds6 warnings,
zero errors (five unchanged CPUContext warnings and one getenv C4996). Actual
native build succeeds in3.14s with no compiler warnings/errors, interpreter OFF.
Helper source/generated SHA256:
7D6FFD00963CE55BC467763A0D858C8D05763997FCFE6F0FB6CA824185EA7768.

Actual e-sdk-idle-20261010 run reaches New Game / Options (_10800.bmp). At75s:
2364 frames,41514 draws,24 shaders,305823 nonblack pixels, GL error0. This proves
native startup/wakeup/rendering integration past the previously slow SDK idle,
not cutscene/save/gameplay completion. User input was requested again for
New Game -> Save Data A; no blind UI inputs were sent. Preserve this running
diagnostic/evidence and continue from the confirmed next transition.

Longer-run evidence prevents claiming complete render recovery: e-sdk-idle
frames/draws hold3592/174138 at175-180s. Unlike the old host-fed170Hz retraces,
the corrected run advances about145 retraces per five host seconds. Catch-up
debt nonetheless recurs during heavy CPU/audio work:165-170s23650 ->26200,
then170-180s261 updates per five seconds reduces debt27200 ->14600, and
220-225s eight renders resume (frames3600). The215s sample802DF7F4/LR802DF574
has main thread80452938 running, D4=804CA3E8 and call chain802881AC/80287FA4/
80286120 ->8012EF74 ->8019DD08. This is neither SDK idle nor proof of a save
failure. Continue targeted analysis of timer/CPU budget and audio/DSP paths.
The reviewed single-source/idle milestone is valid, but intermittent rendering,
confirmed final-build Save Data A, cutscene and gameplay remain outstanding.

### Active audio workload measurement

Milestone8c3098f is pushed and its Windows CI run38032976538 passed. The
e-sdk-idle run ended normally at its configured600-second limit, not a crash.
No new Save Data A confirmation was received for that executable.

Read-only mapping identifies worker job802892C0 and its completion mutex
unlock802BA020 (LR802893A8). The worker then calls receive802B95B0 with
LR80289170 and queue8044E3A0;8044E3A8 is the receive-wait subqueue, not the
queue base. The job's own two timer reads use802BD080 with LR80289314/8028936C.
DMA setup802D9F30 receives buffer address inr3 and byte length inr4.

Next measurement: opt-in call/watch raw-counter and TB values, current thread/
context and interrupt-dispatch state; bounded DMA/AX mailbox scalar metadata
and raw AI control writes. Do not read or publish command-list/asset contents.
Compare completed-job cost and cadence before changing the clock or DSP model.

Primary independent references: devkitPro/libogc gc/tuxedo/ppc/clock.h defines
CPU-to-TB ratio12 for both platforms; libogc/audio.c distinguishes stream-rate
bit1 from DSP-DMA-rate bit6 and programs32-byte DMA blocks. The runtime raw
counter is only approximate execution work, not an exact cycle counter, so
the hardware ratio alone is not evidence of the current stall's cause.
Likewise fixed200Hz DMA and missing AX command processing remain hypotheses
until the actual programmed length/rate and completed workload are measured.
