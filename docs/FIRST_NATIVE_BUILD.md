# First native build: status and remaining work

Updated 2026-10-10. Nintendo files, generated title code, private traces and
the locally linked game executable must never be uploaded.

## Fixed architecture and identity

NWiiRecomp ahead-of-time PPC translation with a native Windows x86-64 runtime.
No Dolphin runtime, JIT/emulator wrapper, Unity or gameplay rewrite.

- Title: WPSE01_01; game ID: WPSE; title ID: 0001000157505345.
- main.dol SHA-1: fd9a2c00c97e420a42355e2c27f3dc0ebbd3d8f9.
- Entry: 0x80004050; latest AOT discovery contains 14,377 functions.
- NWiiRecomp base: 595f176f1d24cc54ff2e8389feed12d7fb553cc2.
- Legal inputs stay in ignored local/WPSE01_01/.
- AOT output stays in ignored generated/WPSE01_01/.
- Executables, screenshots and logs stay in ignored build/.

## Verified milestones and limits

A native Windows executable links with Visual Studio 2022 x64 and runs with
the bring-up interpreter OFF. Current tests:68/68 Release and39/39 standalone
Debug passed. The title menu, requested controls, Save A selection and advancing
opening captions are verified. The scene behind those captions remains black
in the latest completed diagnostic; full cutscenes, gameplay, sound and save
persistence are NOT yet verified. This is not a finished release.

The latest trace proves hundreds of MEM2 display-list calls were rejected by
a MEM1-only parser check. The memory-contract fix passes its new original
regression and the native title rebuilt; e-mem2-dl-20261010 is running to measure
the actual visual result. See the latest sections below for evidence and limits;
older sections are chronological history, not the current blocker. Development
runs from E:/Projects/pkmnrbl-workspace/work/pkmnrbl.

Preserve the solved boot milestones:

- MEM1 804CBD40-81700000; MEM2 90000800-93E00000; IPC 93E00000-94000000.
- Pending IPC ACK Y2 is separate from reply Y1; replies stay ordered and
  accepting X1 does not discard unread Y1.
- Bluetooth state 5 is reached and later HCI initialization continues.
  Opcode 0C0A is Write PIN Type, not Write Scan Enable (0C1A).
- AI PSTAT is bit 0 and AISCNT progresses on CC/CD/physical 0D MMIO views.
  Do not reopen the old 802DA334 loop unless a new trace proves regression.
- Config/ES use WPSE and the WiiWare title high word 00010001.

## Cache-loop diagnosis and correction

The supplied baseline had PC 802B4094, LR 802F27C0, frames 1, draws 0,
shaders 0, black EFB, thread 8090F9D0 (state 2, priority 16, wait queue 0).
Interrupts continued. The complete chain was mapped in local AOT sources;
all seven real addresses were verified against the expected local DOL.

| Address | Meaning |
| --- | --- |
| 802B4094 | Decrement CTR and branch while nonzero, after advancing r3 by 32 bytes; cache flush routine starts at 802B4070. No MMIO polling here. |
| 802F27C0 | Return from flushing an IPC input vector; pointer/length selected by r28 from request r29+24. |
| FFFFFFFC | Runtime interrupt/callback return sentinel, not a title instruction. |
| 802F29D4 | Return from vector preparation at 802F2700, then test result. |
| 802F0BB8 | Return from synchronous submission at 802F2930. Wrapper 802F0B40 submits ES command 23 with three four-byte inputs. |
| 801779C8 | Check wrapper result in title resource-loading path. |
| 801AF850 | Return through resource wrapper 80177FF0, record success/failure. |
| 801A381C | Return from 801AF830 to title caller. |

The cache count is (length + (address & 31) + 31) >> 5. A four-byte aligned
input needs one iteration. The old emitter called handle_syscall at sc
802B4098 while ctx.pc still held the finished loop branch 802B4094 and CTR
was zero. Interrupt dispatch could save that stale state. Restoring it
restarted the counted branch, wrapping CTR to FFFFFFFF.

The emitter now publishes the instruction after sc (802B409C here) before
calling the runtime and returns if the helper reports redirected execution.
No cache bypass, forced wakeup or fabricated IPC length was added.

An original synthetic fixture is emitted by the real recompiler, compiled,
and executed for normal syscall, injected interruption/resume, and redirected
execution. All three tests failed before the fix with a stale continuation;
all pass afterward. The test controls the interrupt source, not the emitter.

Unfixed non-headless runs already sometimes crossed the loop, proving timing
dependence: one reached 980 submissions/207548 draws/5 shaders at 35 seconds;
another 680/143948/3 at 30 seconds. Both had zero nonblack EFB pixels.
Local evidence: build/logs/cache-window-20260910-184455.log and
build/logs/cache-counter-20260910-184608.log. Do not call those a screen.

## Renderer isolation evidence

The 2026-09-16 user bundle was inspected in ignored build/import-renderer-20260916.
Only its relevant diagnostics were merged; existing local fixes were preserved.
Its Linux verification was not accepted as Windows rendering verification.

The Windows executable was rebuilt and its five isolation modes executed.
Seven seconds was too short: the five-second samples preceded GX startup.
Twenty-second runs completed normally. Baseline: 70 submissions, 14628 draws,
zero nonblack EFB pixels. Flat-red: 74/15476 and all 307200 pixels nonblack;
the inspected screenshot is solid red. Reject-bypass and flat-Z also fill red.
Thus geometry reaches the framebuffer; normal TEV/texture color generation is
the next boundary. All modes report GL error 0502, which still needs localization.
The XFB mode reports 307173 nonblack decoded pixels but its base is suspicious
(0x5A in baseline); do not call that valid title output. Local evidence:
build/renderer-isolation/20260916-002834/. The runner's heuristic label alone
is not a diagnosis; missing samples and failed runs require manual review.

Forced red, alpha/cull bypass and flat-Z are diagnostic experiments only.
They cannot be used as release fixes or counted as a correct title screen.
The imported runner's source-text-only test was not adopted; the diagnostic
latch has a behavior test and the real Windows runner has been exercised.

### Multi-stage shader compilation fixed

The real driver rejected the generated two-stage fragment shader: cIn and
other per-stage variables were redeclared in one scope. A GPU regression
compiled and linked original synthetic 1/2/16-stage programs: 2 and 16 failed
before the fix. Each generated stage now has its own lexical scope, while
the shared TEV result registers remain outside. All three cases pass, and
the full Windows Release suite passes 10/10. CI runs the shader test when
an OpenGL 3.3 context is available; return 77 is an explicit skip, not a pass.

The local Windows title was rebuilt and run for 20 seconds without rendering
overrides. At 15 seconds: 382 submissions, 80772 draws, 3 shaders, GL error
zero, complete EFB, but still zero nonblack pixels. This fixes the compiler
failure, not the title screen. Private evidence: build/logs/post-shader-scope.log.
Next inspect preservation of normal versus constant TEV color register banks.

### First visible title-owned output: TEV register banks

BP E0-E7 writes select one of two color banks via bit 23. The renderer used
only the last write to each address, zeroing the other bank during uniform
upload. Separate persistent banks now retain both RA and BG halves at ordered
render-time BP application. The encoding was cross-checked against
[libogc GX_SetTevColor/GX_SetTevKColor](https://github.com/devkitPro/libogc/blob/master/libogc/gx.c).

An original synthetic triangle goes through the real renderer and GPU. Its
control pixel was (64,128,192,255); a subsequent constant-bank write incorrectly
turned it (0,0,0,0) before the fix. Now it stays correct. Additional checks
cover reverse bank preservation, a partial RA write retaining BG, and signed
11-bit upload. All 10 Release tests pass locally, including the GPU test.

Rebuilt PokemonRumble.exe and ran WPSE without rendering overrides for 20 and
55 seconds. The inspected EFB screenshots show a white field with a small
pale-blue central graphic: first visible title-owned output, NOT a validated
title screen or playable menu. At 50 seconds: 1180 submissions, 249948 draws,
5 shaders, 307200/307200 nonblack pixels, complete framebuffer and GL error 0.
An earlier 20-second run latched 0502 at its first sample, then zero; the
remaining intermittent GL error still needs localization, not dismissal.
The picture remains the same in the later screenshot. Private evidence:
build/logs/post-tev-banks.log, post-tev-banks-long.log and their BMP captures.
Do not upload those captures or private traces.

Frequent sampling now shows PC 802BC278, LR 802BC26C. Local AOT decoding shows
a branch while the word at r13-17688 is zero, after calling 802B8CB0. Prove
which queue/thread owns that wait before treating it as the next blocker;
other sampled PCs and continuously increasing draw counts show activity.

## Remaining work for the first usable build

### Additional verified continuation and CI work

The recurring interpreter entry 802B8CEC has native instructions already.
An mtmsr enabling interrupts publishes its successor, but the dispatcher
did not explicitly register that successor. Range fallback chose the earlier
sparse function 80250B00, whose broad bounds contain 802B8CEC but whose actual
instruction set does not. Its local dispatch consequently invoked the interpreter.
Both emitter layouts now register mtmsr successors as exact AOT continuations.

A synthetic executable processed by the real analyzer creates the same sparse
overlap. The compiled split dispatcher failed by invoking the interpreter
before the change; split and single layouts now resume and execute the successor
once. Compiling the single layout also uncovered an existing unclosed try block;
its dispatcher now uses the same balanced setjmp guard as the split layout.
The regression uses callback yield, not an independent longjmp stress test.

All 14373 local functions were regenerated in an ignored staging directory.
Hash comparison found only main_output.cpp changed; it was installed and the
real Windows title relinked. A 25-second run preserved the central graphic:
at 20 seconds, 535 submissions, 113208 draws, 3 shaders, complete EFB, GL error 0.
Captured fallback samples now show 8012B740 rather than 802B8CEC/802B8CBC.
8012B740 is absent from the discovered function list; its caller at 801E1514
uses an indirect call through a vtable slot at +20. Local DOL decoding identifies
8012B740 as a static branch stub targeting the already translated 801E78A0,
not dynamically generated code. Investigate that discovery gap next with a
failing regression. Do not disable the interpreter or claim interpreter-free execution.
Private evidence: build/logs/post-aot-resume.log and associated captures.

GitHub runs for 0d779f4 and 9ce14c4 built successfully but crashed in the GPU
test; their AOT tests passed. The supplied CI log showed a GPU-test segfault.
Bundled SDL can return its legacy WGL context when 3.x creation is unavailable;
GLAD accepts a valid 1.1 version string, leaving shader function pointers null.
The test now requires the loaded GL 3.3 capability before any shader call.
A real-GLAD/synthetic-legacy-driver regression failed before the guard and passes
after it. Unsupported GPU contexts return explicit skip 77, never a GPU pass.
At that milestone the local Release suite passed 13/13, including real GPU execution.
GitHub Windows CI run 35180854096 for f89802b completed successfully on
2026-09-17, including the synthetic AOT/GPU test step, generated-project
regression and repository policy tests:
https://github.com/Ocey78/pkmnrbl/actions/runs/35180854096
The public job status does not distinguish a GPU test pass from an unsupported
context skip; real GPU execution is verified locally, not asserted for CI.

### AOT root and projection verification

Latest AOT discovery milestone (2026-09-17): the existing `symbols_csv` mechanism
now loads `config/WPSE01_01/aot_roots.csv`, containing the manually identified
8012B740 entry point. This is address metadata, not copied game code. Regeneration
discovers 14374 functions and emits a native branch to the existing 801E78A0
function. No general heuristic or interpreter behavior was changed.
Synthetic tests load the real title configuration and execute an indirectly
entered branch stub through both emitted dispatcher layouts. Without the root,
split output reached the interpreter and single output rejected unknown dispatch.
With the root, both execute the target exactly once and preserve LR. All 15 local
Release tests pass; generated-project, native-build-script and policy tests pass.
GitHub CI for 94c80b1 passed: https://github.com/Ocey78/pkmnrbl/actions/runs/35186161112
The real title rebuilt successfully and completed a 25-second run with no
`[Interp]` entry messages (private evidence: build/logs/post-title-root.log).
At 20 seconds it had 497 submissions, 105152 draws and 3 shaders. This log does
not prove that unlogged single-instruction `micro_interpret` calls were avoided.

The remaining early GL 0502 was localized to XF projection state arriving before
the first shader. The renderer eagerly uploaded to zero-initialized uniform
location 0 without a bound program. A real-GPU regression reproduced the error.
Projection uploads now occur only in SetupDrawState, after the program is bound;
XF commands still flush the preceding batch and retain the next projection.
A 25-second rebuilt-title run reports GL error 0 in all four samples, with
491 submissions, 103880 draws and 6 shaders at 20 seconds. The captured image is
still the white field and small central graphic, not the title screen.
Private evidence: build/logs/post-projection.log and post-projection_2400.bmp.

### Verified interpreter-OFF boot

The generated project's OFF setting previously failed to link: the loader still
needed add_recompiled_range, and generated code still referenced interpret_step
and micro_interpret. A native-only source now supplies diagnostic failure guards
instead of a PPC interpreter. Uncovered code or unsupported instructions print
PC/LR/CTR (and the supplied opcode for a micro fallback), then exit with failure.
No guest instruction is interpreted. Text-range registration is a no-op in this
mode; interpreter-ON builds retain their original implementation.

Real generated synthetic code reproduced the missing symbols before the fix.
Both output layouts now link and execute a covered continuation; subprocess
regressions verify failure on an uncovered PC and on a reserved instruction,
including its explicit instruction address when ctx.pc is stale. All 22 local
Release tests pass, including actual GPU rendering and the scaled-projection
pixel check. Export and repository-policy checks also pass.

The local generated project was configured with
`-DPKMNRBL_ENABLE_BRINGUP_INTERPRETER=OFF` and rebuilt successfully. Its MSVC
project selects native_only.cpp and does not compile interpreter.cpp. A real
25-second WPSE01_01 run exited 0 on its diagnostic deadline with no AOT guard
failure: 484 submissions, 102396 draws and 3 shaders at 20 seconds; GL error 0
in every sample. The image remains the initial central graphic. This proves
interpreter-free execution of this bounded boot path, not full gameplay coverage
or release readiness. Private evidence: build/logs/native-only-title.log,
native-only-title.err and native-only-title_2400.bmp.
A second 55-second interpreter-OFF run also exited 0 with empty stderr, no
coverage trap and GL error 0 in all samples. At 50 seconds: 1341 submissions,
284080 draws and 3 shaders. The graphic remained unchanged. Private evidence:
build/logs/native-only-long.log, native-only-long.err and native-only-long_4800.bmp.

### Work still required

1. Advance beyond the English Wii Remote instruction/loading screens to a recognizable title
   screen. Trace guest thread/queue and resource state; inspect rendering
   only where evidence shows a discrepancy. Monitor for new GL errors,
   validate TEV/texture state and EFB copy/clear ordering, and keep
   using failing regressions followed by real-title runs.
2. Capture a longer boot with valid content and WPSE low-memory identity.
   Identify the next exact wait or failure from code before changing behavior.
   The former TPL assertion is resolved: a zero boot game code selected the
   wrong region and omitted the localized texture archive. Do not reopen it
   unless a fresh trace proves regression.
3. Re-run bounded real title boots to verify the cache-loop runaway does not
   recur. Keep PC/LR/CTR/context ownership and rendering evidence.
4. Connect and verify one native input source through the title's actual Wii
   input path. Require navigation of the first menu; HCI startup is not input.
5. Monitor the corrected 8012B740 entry and 802B8CEC/802B8CBC continuations
   for recurrence and investigate any new fallback sites.
   Supply missing AOT/native coverage. Audit fallback syscall continuation
   separately; the current fix is in the AOT emitter. Extend the verified
   interpreter-OFF boot to menu/input/gameplay paths before calling it release-ready.
6. Run all MSVC tests, synthetic export, policy tests and Windows CI; prove a
   clean checkout rebuilds with separately supplied legal title data.
   Never publish the resulting game executable or translated source.

### Local ES content service (2026-10-10)

Fresh baseline call tracing proved all five opens requested index 5. Each
32-byte header read previously returned zero. The guest takes header +0xC
as the archive data offset, rounds it to 32 bytes, seeks to zero and reads
that metadata region. Zeroing the header caused a zero-length second read,
which the guest wrapper rejects before issuing IPC. This was a real content
failure, not a scheduler wait. The local index-5 file's size and SHA1 were
independently checked against its TMD before use; both matched.

ES commands 09, 0A, 23 and 0B now implement read-only open/read/seek/close.
The service finds title.tmd or the configured title-ID-named TMD in game_dir
or its parent, maps indices to content IDs, and reads ID-named .app files.
Only already-decrypted local files are supported. It validates TMD structure,
title identity, file length, complete guest RAM spans, and session ownership;
it does not authenticate signatures or content hashes at runtime. Padding
beyond logical EOF is not returned. This is not a NAND installer/decryptor
or shared-content resolver. Existing non-content ES stubs remain unchanged.
The command/vector contract was cross-checked against the local guest wrappers
and [libogc ES functions](https://github.com/devkitPro/libogc/blob/master/libogc/es.c).

The synthetic regression failed before implementation at the first header
read (expected 32, got 0). It now passes through the real IOS kernel/device/MMU,
with index != ID, signed seeks, independent handles, EOF/tail preservation,
session-close cleanup, malformed vectors/TMDs and invalid memory bounds.
All 23 Release tests passed, including actual GPU execution. The new ES test
is also part of the asset-free Debug target used by Windows CI; all seven
Debug tests passed locally after a fresh configuration/build.

The rebuilt interpreter-OFF title now returns 32, 2944 and 716800 bytes for
the traced reads. It then reaches OSPanic from TPL.c line 25 and halts at
802B1890 / LR 802B513C, with interrupts disabled (MSR 1032), 212 submissions,
zero draws/shaders and black EFB. The bounded launcher terminated the hung
process at 45 seconds; this was not a clean diagnostic exit or title-screen
success. Chain: 8033E1EC, 802AEF6C, 802AC380, 802A722C, 802A6FE8, 802A668C,
801CF1F0, 801CE6CC, 801CCC00, 801182C0, 8012C6B4, 8012FA9C, 8019EACC.
Private evidence: build/logs/es-baseline-20261010.log and
build/logs/post-es-20261010.log, .err, and BMP. Keep these local.
Follow-up trace tpl-trace-20261010.log proves TPL unpack is called twice from
802AEF6C: first with r3=901A50C0 (accepted), then with r3=0 (assertion).
The next investigation is the resource lookup returning null, not corrupting
the first valid TPL. The required header magic is 0020AF30.

Previous milestone d38529a Windows CI succeeded:
https://github.com/Ocey78/pkmnrbl/actions/runs/35294073101

### Title-owned instruction screen and boot identity (2026-10-10)

The ES milestone f23f080 passed Windows CI:
https://github.com/Ocey78/pkmnrbl/actions/runs/38025262567

The null texture was wii_remote_title.tpl. Tracing the resource path backwards
proved that 8012C660 chooses ja_JP for region 0, en_US for region 1, and only
consults the system-language locale getter for region 2. Guest 802B2EA0 checks
the optional disc game code at 80003194, then falls back to the WiiWare boot
game code at 80003180. Both were zero. The copied ID at 804B9468 remained four
zero bytes; the region getter's unknown-character branch selected Japanese.
This was not a regression of Config/ES identity, which already used WPSE.

The WPSE boot adapter now writes 57505345 at 80003180 only after arena validation
succeeds. It leaves 80003194 and neighboring words intact. A real-MMU regression
failed on the missing identity first, then passed with physical/cached byte
order, neighbors and invalid-layout preservation checks.

Separately, the original synthetic SYSCONF had invalid magic, count, offsets,
record encoding and no footer. It now has a valid 16-KiB SCv0 document, two BYTE
items IPL.AR=0 (4:3), IPL.LNG=1 (English), and SCed footer. The new test failed at
the old header and now independently parses records through the real FS/MMU;
it also checks chunked reads, EOF/tail preservation and rewind. These are
original runtime-generated defaults, not imported NAND data. The format was
cross-checked with local guest parsing and
[libogc configuration support](https://github.com/devkitPro/libogc/blob/master/libogc/conf.c).
The separate post-sysconf control run still selected Japanese and panicked,
which isolates the boot-ID correction as the resource-selection fix.

With boot identity installed, the failing texture now resolves to 9003FBC0,
other English instruction textures load, and the screenshot visibly reads
"How to Hold the Wii Remote" with the controller illustration. The 35-second
interpreter-OFF run exited 0, stderr was empty, no native-only coverage guard
fired, and every rendering sample reported GL error 0. This is the first
verified substantial title-owned screen in this content-backed boot, not the
title menu. Loading continues afterward; no input/menu success is claimed.
Private evidence: region-trace-20261010.log, post-sysconf-20261010.log,
post-boot-id-20261010.log/.err and post-boot-id-20261010_1200.bmp, all in build/logs.
Temporary generated-source lookup logging was removed after collecting evidence.

Verification: all 24 Release CTests and 8 asset-free Debug CTests pass; GPU tests
actually execute. Synthetic generated-project export and repository policy
tests pass. Independent review found no actionable issue in these two changes.
Existing FS seek/read hardening and setting.txt serialization remain unchanged;
this milestone does not claim complete NAND compatibility. Existing upstream
compiler warnings remain.

### Native controls and verified title menu (2026-10-10)

Boot-identity milestone d3c4956 passed Windows CI:
https://github.com/Ocey78/pkmnrbl/actions/runs/38026030441

The earlier 100-second run reached a visible controller-disconnected message.
Native SDL input existed but did not reach the title: Bluetooth bulk-IN remains
unimplemented, and the existing KPADRead stub was both unbound and only wrote
0x50 bytes. Local guest consumers establish a 0xB0 KPAD sample with type at5C,
signed error at5D, pointer-valid at5E and format at5F. Main polling801B1810 calls
WPADProbe802FA010 and KPADRead803070D0; initialization8017A3E0 checks Probe
directly. Those two SDK functions are now explicit per-title AOT hooks. The
guest's connection checks, loading, rendering and menu logic remain unchanged.

The native basic-remote bridge validates complete RAM outputs and publishes
coherent button snapshots. Samples contain hold/trigger/release, valid basic
type/status and deterministic unused bytes. Disconnected probes and reads reset
only that channel's edge history. No physical Bluetooth, extension, pointer,
motion, speaker or full WPAD callback compatibility is claimed. Other WPAD
functions remain guest code; trace any future gate before expanding the seam.

User defaults are left mouse=2, right mouse=1, WASD=sideways movement,
Escape=+, Tab=-, E=A; see [controls](CONTROLS.md). Escape does not quit.
GameCube bindings remain unchanged. The tests exercise the real input source,
InputManager and guest MMU, not a copied input mapping.

Regression-first evidence: old KPADRead failed the complete-record error byte;
old left-click mapping emitted0800 instead of0100; the review-discovered
Probe-gated reconnect path lost its press edge. All now pass. Explicit hook
declarations also failed to compile initially; both synthetic generated layouts
now compile and execute the custom hook once, return to LR, and never execute
the guest body or interpreter. Shared generated-header emission is unchanged.

The actual title was regenerated and rebuilt with
PKMNRBL_ENABLE_BRINGUP_INTERPRETER=OFF. Its 100-second run exits0 with empty
stderr, no native-only coverage failure, and GL error0 in every rendering
sample. Screenshot native-controls-20261010_6000.bmp visibly shows the Pokemon
Rumble logo, New Game and Options. At95s:3031 submissions,126761 draws,35 shaders,
305823/307200 nonblack pixels. Local evidence is build/logs/native-controls-
20261010.log/.err and BMPs; none may be uploaded. Bounded table scans802349D8
and80234A88 during loading completed; they are not the current blocker.

The user separately verified controls and navigation to the save screen, then
reported black output after starting a save. Their local bluetooth-trace.txt
is retained as evidence, not published. The automated title-menu run did not
exercise save creation. Next: reproduce the save-start path with timed samples
and IPC/FS evidence, distinguish slow loading from a persistent wait/failure,
then add a failing original regression for the proven missing runtime behavior.
Do not claim persistent saves or playable gameplay yet.

Fresh verification:28/28 Release tests and10/10 Debug tests pass, including real
GPU execution; standalone export and repository-policy checks pass. Windows CI
explicitly includes both new generated-hook layouts and input tests. Independent
review's reconnect finding was fixed and rechecked; no findings remain for this
limited milestone. Unrelated local helper scripts are preserved.

### Post-menu AOT corrections; full rebuild pending (2026-10-10)

Controls/title-menu milestone e6aa265 passed Windows CI:
https://github.com/Ocey78/pkmnrbl/actions/runs/38027033907

A longer run stayed on the title menu (no confirmed save action), then exited1
after approximately140 seconds with native-only guard PC8028F440,
LR80276D3C, CTR8028F440. This is a separate callback discovery gap, not proof of
the user's save-start black screen. The original callback8028F440..8028F588 is
absent from generated function bounds; setup8028E3B0 constructs its address
across branches/calls and passes it to802768A0 for storage at object+38.
Caller80276CD0 loads that field and bctrl at80276D38. A verified CSV root now
includes the callback without changing general discovery or enabling fallback.
An original synthetic object callback fails in both generated layouts without
the root, then passes with it: one invocation, exact callback return address,
resumed caller and preserved outer LR. The previous8012B740 case still passes.

Separately, two generic prologue signatures incorrectly replaced NAND wrappers
with IOS_Ioctlv and IOS_IoctlvAsync. Original802E4830/4950 call create-file async;
802E5180/52A0 call create-directory async. Original802E4630/4740 call synchronous
ISFSCreateFile802F4470 (command9,input76), not IOS_IoctlvAsync. A path pointer was
treated as a negative fd in an earlier trace. The two unsupported automatic
matches are removed; other matches and verified explicit hooks are unchanged.
True IOS_Ioctlv802F2930 and async802F2840 already have translated MMIO paths.
Original synthetic functions reproducing each generic prologue failed before
the removals and now execute their bodies, helpers and returns natively in both
layouts, with no IOS replacement or interpreter call. No guest code is shipped
in those fixtures. Independent reviews found no remaining scoped issues.

Fresh final verification:31/31 Release tests and10/10 Debug tests pass, with
real GPU execution. Export/policy checks pass. This does NOT verify the combined
title executable yet: export after the synchronous removal and new callback
root discovered14,375 functions, but its full compile was stopped for low disk
space. The additional async-signature removal came afterward and is not yet
in that export. The last completed executable remains e6aa265.

Resume in this order:

1. Free disk space. A stopped legacy configure smoke test copied the complete
   workspace into a newly created local temporary directory named
   pkmnrbl-configure-9bb134c26e274d4cacd2862d428f8632. Agent cleanup was blocked;
   the user was asked to remove only that duplicate. Original repo/game files
   must stay untouched. Do not rerun tests/cmake_configure.Tests.ps1 from this
   full game workspace until its copy policy is corrected; clean CI is safe.
2. Rebuild nwiirecomp.exe to include BOTH signature removals. Regenerate using
   config/WPSE01_01/recomp.toml, then build the generated Release title with
   PKMNRBL_ENABLE_BRINGUP_INTERPRETER=OFF. Do not infer completion from objects
   left by the interrupted compile. Existing warnings are not new failures.
3. Repeat a longer menu run past the previous140-second guard. Confirm the
   callback is reached natively and inspect private screenshots/exit status.
4. Capture an actual New Game/save attempt. The previous user trace had only
   about20 seconds of black output while loading work continued; the longer
   automated trace never left the menu. Save storage remains incomplete:
   FSDevice lacks creation/metadata ioctls and disk persistence. Add failing
   original regressions for behavior proven by the next trace before fixing it.

Private evidence: build/logs/save-start-20261010.log/.err, _12000.bmp and
post-callback-export-20261010.log. Do not upload these or the temporary copy.

### E-drive continuation and fresh reply-format blocker (2026-10-10)

The copied workspace was verified file-for-file before use: 46,012 files,
4,533,221,993 bytes, identical SHA-256 content. C-drive cleanup was blocked;
the original remains untouched. Use the E repository explicitly, not the old
task working directory. Untracked local helper scripts are preserved.

Fresh E Release and asset-free Debug configurations were built, with 31/31 and
10/10 tests passing, including actual GPU execution. Python is selected
explicitly and compiler TEMP/TMP point to build/tmp on E. The old copied build
trees are named windows-release-c-copy and windows-debug-c-copy. The new builds
reuse only pinned dependency source directories in the former; do not remove
that tree while those source overrides remain in CMakeCache. Active caches and
projects no longer reference the old C workspace.

Latest nwiirecomp export discovers14,375 functions, including callback8028F440.
The full native title built successfully (zero errors,499 existing warnings),
with PKMNRBL_ENABLE_BRINGUP_INTERPRETER=OFF. No interpreter source is compiled.
This supersedes the earlier disk-space-blocked rebuild, but not its outstanding
save/cutscene/gameplay verification.

First fresh title run: frames60/draws0, PC802B4064/LR802F14BC, before any input.
It was stopped after capturing a persistent state. A WRITE command4 completed
on descriptor7 while saving synthetic SYSCONF. The runtime left descriptor7
at request+8; the guest expects original command4 there in a reply, so it takes
the IOCTLV7 branch and walks unrelated data as buffer vectors. The exact AOT
branch/call mapping and regression checklist are in
docs/superpowers/plans/2026-10-10-save-start-trace.md. Private evidence remains
under build/logs/e-save-baseline-20261010.* and must not be uploaded.

The reply-format correction is now verified: original synthetic packet tests
failed first, then pass for all seven commands, RAM aliases, pending/deferred
completion and queue ordering. Shared ipc_post_reply converts+0/+8 after device
completion, preserving result/payload/callback metadata. Full Release31/31 and
Debug10/10 tests pass; independent review has no actionable findings.

The actual interpreter-OFF executable was incrementally rebuilt with the changed
runtime file (its generated copy was checksum-checked; translated game code is
unchanged). Private e-reply-fixed-20261010_10800.bmp visibly shows the New Game /
Options title menu. At80s:3771 frames,66,528 draws,40 shaders,305,823 nonblack
pixels and GL error0. The malformed-reply stall is resolved. Live Windows
capture timed out twice, so the user was asked to start New Game in the running
600-second diagnostic instead of sending blind inputs. Next: capture that
transition and trace the next proven blocker. No cutscene, persistent-save,
or gameplay success is claimed.

### Next proven blocker: duplicate video timing (2026-10-10)

Commit34fc73f and Windows CI run38030802937 pass. The user then reproduced
black output after New Game / Save Data A. A second trace stayed black for
over two minutes while interrupts and game updates continued. Frames/draws
stopped at4652/143629. No native crash or filesystem failure was established.

The local frame loop8019DC50 catches up simulation updates before rendering.
At8019DD64/DD74 it loops when accumulated VI time exceeds simulated time.
Targeted register watches prove a growing debt of thousands of updates;
about170 VI retraces per host second were observed. A third trace reproduced
the same starvation while still at the menu, independently of save creation.
The old host-presentation vblank signal and EventScheduler recurring VI both
raise guest VI interrupts. This is a runtime timing defect, not permission to
patch the game's skip-render branch.

The corrected runtime now uses one guest-time VI source and advances the same
virtual clock to the earliest event/decrementer during genuine CPU idle.
PC0 alone proved insufficient in the actual title: its SDK scheduler idles at
nonzero PC802BC278. A second failing regression preceded recognition of a
validated nonempty, complete, all-waiting SDK active-thread list, with EE set,
no current thread and a distinct valid context. Invalid/ready/running states,
callbacks, handlers and deliverable interrupts prevent this fast-forward.
There is no hard-coded title-PC exception or guest branch rewrite.

Final verification: Release48/48 and Debug27/27 pass, including the actual GPU
test and17 timing cases. Separate implementation review and scoped re-review
have no outstanding findings. Full host build succeeds with6 compiler warnings
(5 from unchanged CPUContext,1 getenv deprecation retaining its existing clock
policy), zero errors. The actual native title rebuild has zero compiler
warnings/errors and remains interpreter OFF. Changed runtime files were
checksum-verified against their generated copies; AOT game code is unchanged.

Private e-sdk-idle-20261010_10800.bmp shows New Game / Options. At75s the title
has2364 frames,41514 draws,24 shaders,305823 nonblack pixels and GL error0.
The first single-clock iteration had crawled at3.5 frames/sec in SDK idle;
after recognizing real idle it progresses through the startup screens to the
menu. New Game/Save Data A interaction has been requested in this corrected
run. Cutscenes, actual gameplay and persistent saves are NOT yet verified.

Resume from e-sdk-idle-20261010.log/.err and the newest screenshots in build/logs.
Keep the actual AOT native build and both timing corrections. Capture confirmed
Save Data A input, then map the next persistent guest state before adding its
own failing regression. Filesystem creation/metadata/persistence and DSP AX
parameter-block processing remain known incomplete areas, but neither should
be declared the next blocker without trace evidence. Never upload private
traces, screenshots, generated game code, game content, or the title executable.

Longer-run qualification: the corrected title still develops intermittent
render starvation after the menu. Frames/draws hold3592/174138 around175-180s;
updates later catch up and eight renders resume around220-225s. Retraces now
advance about29 per host second, not the former170, but main/audio CPU work
still consumes more guest-time budget than some simulation steps allow.
Current215s sample802DF7F4/LR802DF574 and main-update chain802881AC/80287FA4/
80286120 ->8012EF74 ->8019DD08 need further mapping. This milestone fixes the
proven duplicate source and real-idle progress, NOT the complete black-screen
problem. Next discriminate timer/instruction budget from missing audio/DSP
behavior before changing either. Save Data A on this final build is unconfirmed.

### Audio workload diagnostic checkpoint

The8c3098f Windows CI run38032976538 passed. Its600-second real-title trace
ended at the configured limit with a normal shutdown, not a crash.

NWII_TIMING_TRACE now adds opt-in raw execution-counter/TB/thread/context/
interrupt-state fields to selected call/watch traces, plus bounded DMA and AX
mail scalar metadata and raw AI control writes. It does not execute AX commands,
change timer rates or read/log command-list contents. First16 and every200 DMA/
mail events are logged; no new output or guest reads occur when disabled.
Original behavioral tests failed first for four enabled contracts; all five
contracts then passed. Full Release53/53 and Debug32/32 pass; independent review
has no blocking findings. Release host build had26 warnings, zero errors;
Debug build and actual runtime-only native rebuild had zero warnings/errors.
The title rebuild remains interpreter OFF and took3.44seconds.

Private current run: build/logs/e-audio-budget-20261010.log/.err, bounded240s.
At90s it was still rendering (2528frames/63169draws). Actual DMA is12blocks,
384bytes, with AIcontrol0x60 selecting32kHz DMA. That is96 stereo frames or
3ms nominal duration, whereas the current runtime schedules5ms. The existing
timer is therefore slower, not faster, than the programmed buffer cadence;
do not label it the source of excess interrupts without further evidence.
Early completed audio jobs cost355 raw counts median; the heavy phase still
needs measurement. Raw deltas include intervening execution/idle advances.

Next: analyze completed audio job802892C0 through802BA020/LR802893A8 and the
core timer pair802BD080/LR80289314/8028936C. Separate idle/interrupt/other-thread
time before comparing cost with DMA deadlines and frame catch-up debt. If a
clock-unit error is confirmed, add failing conversion/DEC/event/idle-inverse
tests before changing it; preserve wall mode and guard multiplication overflow.
Hardware CPU/TB ratio12 is not a claim that every translated instruction costs
exactly one cycle. AX voice processing and proper DMA cadence remain separate
unfinished runtime work. Do not force rendering or skip the title's own loop.

Live Computer Use reported a physical-Escape stop on the new window; no further
automated input was sent. No Save Data A, cutscene or gameplay success is claimed
for this diagnostic. Resume by examining its completed log, then rebuilding and
obtaining a confirmed New Game transition when window input is available.

### CPU/TB separation and next native instruction blocker (2026-10-10)

The raw execution charge counter is now converted to TB ticks at12:1, with
checked inverse conversion when advancing idle to an event/DEC deadline.
This is an approximate CPU-charge policy, not cycle-accurate PPC timing.
Event periods, DSP behavior, wall-time mode and title code were not changed.
Six new behavioral regressions failed before the fix; Debug38/38 and
Release59/59 now pass. Independent review found no outstanding issues.
The full native interpreter-OFF rebuild succeeded in8m56s with499 warnings
and zero errors; source/generated runtime hashes matched. Host Release build
had531 warnings, zero errors. Do not describe these builds as warning-free.

Private e-cpu-timebase-20261010.log/.err records the user's confirmed New Game
and Save Data A selection. Unlike the prior frozen rendering counter, the
black loading phase continues drawing:80s1458frames/29669draws;85s1473/29684;
95s1502/29713. At80-85s, fifteen updates and fifteen render entries correspond
to0.25333 guest seconds. Catch-up debt is not growing in this interval.
The busy PC802E31E8..802E335C belongs to decompression, not scheduler idle.
No DEC force-expired marker appeared. These facts do NOT establish a completed
cutscene, gameplay, persistent save or accurate audio.

The process then exits1 on a missing AOT instruction. The diagnostic reports
stale function-entry PC802A3970; the actual unsupported instruction is at
802A3978, opcode1000000C, indexed paired quantized load (psq_lx). The emitter
supports immediate psq_l/psq_st but lacks the indexed op4 variants. Next:
add an original synthetic AOT regression for indexed loads/stores and their
update forms, prove failure, emit native calls to the existing quantization
helpers with correct effective addresses, then regenerate/rebuild and rerun
the same Save Data A transition with the interpreter OFF.

Live window capture still failed twice (FrameArrived/window capture timeout);
the user supplied the menu input. No blind automated input was sent. All
traces/screenshots/executable and Nintendo-derived generated output stay local.

### Indexed paired quantized instructions: verified native checkpoint

The emitter now handles psq_lx/stx/lux/stux using six-bit XO and indexed W/I,
with uint32 effective-address snapshots, RA0/RB0 distinction and update forms.
Reserved-bit/update-RA0 encodings remain unsupported. Unknown op4 diagnostics
now report the static instruction address instead of stale context PC.
Original synthetic tests failed first for all four native forms and the
diagnostic-address contract. The384 native cases plus decode checks now pass
in both Release and Debug. Full Release64/64 and standalone Debug38/38 pass;
independent code and CI-wiring review found no issues. CI builds/runs the new
five tests without game data. Existing quantization helper signed-scale,
saturation and exception limitations remain separate unfinished work.

Isolated local re-export found exactly39 changed indexed instructions across
three AOT output files; all other AOT files matched byte-for-byte. Only these
three generated files were copied after retaining local backups. Runtime
differences were preserved. Native rebuild succeeds in1m20s,16 warnings,
zero errors, interpreter OFF. The initial Release test build had8 warnings;
incremental full Release had zero; new Debug fixture build had553 warnings,
zero errors. These warnings have not been eliminated.

The user confirmed Save Data A again in e-indexed-psq-20261010.log/.err.
The formerly failing function802A3970 now executes repeatedly and loading
proceeds further. The next failfast is pc80252450, LR802534A0, CTR802538A0,
opcodeC4040008 (lfsu), with interpreter disabled. This is the next specific
translation gap; add a fresh regression before implementing it. No completed
cutscene, gameplay or persistent save is claimed.

### Single-precision update translation: opening caption reached

The missing lfsu/stfsu immediate forms now emit native transfers with a scoped
uint32 effective address and post-transfer base writeback. lfsu updates both
paired lanes, matching the runtime's existing lfs behavior. RA0 remains on
the unsupported path; double forms are unchanged. Original load/store tests
failed on the forbidden fallback first.36 native cases cover offsets, wrapping,
signed zero, register preservation and neighboring bytes. Release67/67 and
the new Debug3/3 pass; independent review reran the three Release regressions
with no findings. One initial test-sentinel address overlapped the preexisting
MMU physical0xC special case; the corrected wrap test uses ordinary RAM0x20.
The production code was not changed to accommodate that harness issue.

Local isolated export changed ten lfsu sites in two AOT files, nothing else.
Backups retained; copied outputs checksum-verified; runtime unchanged. Native
interpreter-OFF rebuild succeeds in41.79s with zero warnings/errors. The first
Release implementation build had14 warnings, final incremental build zero,
new Debug fixture build8 warnings, all zero errors.

Current private run e-float-update-20261010.log/.err (bounded600s) gets past
both previous instruction failures and reaches the opening sequence's visible
caption, "This is a Toy Pokemon." Screenshot e-float-update-20261010_14400.bmp
records it. At100s:1607frames/49434draws/64shaders/2429nonblackpixels; GLerror0.
Both scene-update functions continue executing natively. This is an opening
caption milestone, NOT a verified complete cutscene or playable game: the
scene's 3D visuals, timing, sound, later inputs and persistent saves still
need validation. Continue observing this run and map any next proven blocker.

14fc2f8 Windows CI run38065496404, bdfad84 run38066137684 and 2a08bbc
run38066653505 all passed. Keep all title output/assets/screenshots private.

### Opening sequence: geometry is submitted but clipped

The float-update run completed its600-second bound normally, with empty stderr.
Several opening captions advanced; the user independently confirmed captions
over black, with slow playback. Full 3D cutscenes and gameplay remain unverified.

Default-off NWII_RENDER_DIAG now observes actual GL batches separately from
decoded FIFO draw counts, retaining perspective and orthographic groups through
later caption draws. Scalar triangle diagnostics identify nonfinite coordinates,
nonpositive W, common-plane rejection, degeneracy and winding; rejection-plane
counts can overlap. Bounded GX3D records report applied state and view-space
bounds. These observations do not force rendering or establish fragment visibility.

Original geometry tests failed before implementation; the GL integration test
failed before wiring. Full Release67/67 and standalone Debug38/38 passed.
Independent review found one test blind spot: diagnostics consume GL errors into
a latch, so tests must inspect it as well as glGetError. Fixed and demonstrated
failure with an invalid GL-enum mutation, then removed the mutation and reran
the suite. A subsequent rejection-plane extension failed first and passed after
implementation; final Release67/67 and Debug diagnostic1/1 pass. Runtime test
build has27 warnings, zero errors. Initial native diagnostic build was6.38s,
zero warnings/errors, interpreter OFF. Only four verified runtime files were
copied to the local generated tree; all unrelated differences were preserved.

The user's confirmed Save A run e-perspective-20261010 removes per-call/watch
logging. At95s it submitted800 perspective batches/4800vertices/1600triangles:
all1600 were rejected by a common clip plane, with zero nonfinite coordinates,
nonpositive W or shader skips. A later window shows26820/26820 rejected triangles.
Sampled shaders link, culling is disabled and GL error is zero. This narrows the
next investigation to coordinates/matrix/projection/clipping, not a stopped draw
loop; it does not prove which transform is wrong. Next run adds exact rejection
planes and per-batch view bounds, then traces those values backward through FIFO
matrix snapshots and native scene transforms. Do not add depth/cull bypasses.

Known remaining native AOT inventory: three ordinary rlwnm sites, one deliberate
trap and data/padding analysis artifacts. None is proven reached in this run.
Retain failfast OFF behavior and only implement gaps after relevant evidence.

### New boot exception target: native coverage checkpoint

The next clipping run e-clip-planes-20261010 exited1 after the Wii Remote screen,
before Save A: uncovered AOT PC8019EAD0, LR8019EAD0, CTR803419B8. No graphics
exception was logged. Local code and exception metadata confirm an intentional
landing pad: the owning function8019E9C0 plus relative offset110 selects8019EAD0.
The title's own restoration routine loads that computed target into LR and
returns to it. The normal branch at8019EACC skips this block, so ordinary CFG
discovery correctly does not find it; changing the preceding call's LR would
be wrong. Why this run entered the exception path is still unproven.

Added precisely this observed entry to the existing redistributable AOT-root
CSV, without changing exception behavior or enabling interpretation. Original
split/single synthetic tests both failed before the root and now pass in
Release and Debug. Full Release67/67 pass; fresh review found no issues.
The fixture verifies discovery/native dispatch and LR restoration, not the
whole title exception mechanism. Existing two roots remain intact.

Isolated export adds exactly two native functions: the landing pad and its
direct cleanup callee803422A8. Five output files differ due to chunk placement;
1429 preexisting function bodies in those files were compared unchanged by
hash. Other output files are identical. Only those five files plus declarations
and dispatcher were copied after backups, with hashes verified. Runtime and
all unrelated local changes remain intact. The full interpreter-OFF title rebuild
succeeded in8m29s (499 warnings, zero errors); the declaration-header change
recompiled all chunks. Windows CI38068062363 also passed. Private diagnostic
e-landing-root-20261010 reached Save A and advancing captions, as confirmed by
the user. It did not reproduce the missing-entry exit, but the original exception
trigger remains unproven. The diagnostic was deliberately stopped after gathering
the following rendering evidence, not because of a new title crash.

### MEM2 display-list rejection: verified runtime fix, visual result pending

The same run repeatedly issued hundreds of CALL_DL requests in MEM2, including
91560560/96bytes and neighboring lists. The parser masked the address to11560560
then rejected it against a MEM1-only01800000 ceiling before decoding anything.
The remaining submitted perspective triangles were all left-clipped, with view
X around-49.7 at Z around-8.6; they do not establish what the discarded geometry
would draw. No projection/depth/culling override was added.

The active producer is translated guest code writing CALL_DL bytes through the
write-gather pipe. The optional GX_DL_Snapshot seam is not bound/called by this
title. Its separately reproduced bank-mask/span bugs are corrected with the
same memory contract, but are not attributed as this title's active blocker.

New asset-free gx_display_list_memory tests link the actual parser, snapshot
and MMU with only unrelated renderer/hardware boundaries stubbed. Before the
fix,14 of57 assertions failed: MEM2 expansion, cross-bank snapshot identity and
bytes, invalid snapshot spans. After the fix all57 pass. Both banks' aliases,
exact-end and crossing spans, MMIO/unmapped/wrapping rejection and parent FIFO
continuation are covered. Snapshot alone validates guest reads; Fetch returns
only already stored bytes, so its rejection assertions cannot mask a bad insertion.

The shared rule retains MEM2's bank bit and validates complete guest spans with
subtraction before reading memory. Parser size/depth limits, caching, command
order and native CPU execution remain unchanged. Fresh review found no issues.
Final Release68/68 (9.42s) and Debug39/39 (1.86s) pass. Release build reports14
warnings (first build34), Debug0, all zero errors. Native OFF rebuild:5.24s,
zero warnings/errors. Two generated runtime preimages were hash-checked and
backed up before replacement; the new helper and copied files match source.

Private run e-mem2-dl-20261010 is now testing the real title. Do not claim visible
3D until its capture/user result confirms it. A separate synthetic probe also
reproduces stale per-vertex matrix-bank snapshots on a DL cache hit; that issue
is deliberately deferred until the MEM2-only run establishes the next symptom.

## Reproduction

Full legal local build: tools/Invoke-NativeBuild.ps1.
After emitter changes, rebuild nwiirecomp, run it with
config/WPSE01_01/recomp.toml, then rebuild build/windows-release/generated
with configuration Release and target PokemonRumble.
The executable is build/windows-release/PokemonRumble.exe; pass
local/WPSE01_01/extracted as its argument.

Build all test executables before full ctest --preset windows-msvc-release.
CI builds standalone tests in Debug, then host, AOT and GPU tests in Release.
The GPU test explicitly skips when no OpenGL 3.3 context is available.
Use tools/Invoke-RendererIsolation.ps1 -Seconds 20 for renderer diagnostics.
NWII_SAMPLE=1 and NWII_LOOPTRACE=802B4094 add PC/CTR/context evidence.
All evidence belongs below build/, never GitHub.

Local toolchain notes: MSBuild needed a child environment with a single Path
entry and /nodeReuse:false to avoid duplicate PATH/Path inherited state.
The Store Python alias became unavailable; selecting the installed bundled
Python via CMake Python_EXECUTABLE resolved GLAD generation.

Before pushing: run tools/Check-RepositoryPolicy.ps1 -RepositoryRoot .,
inspect the staged diff, and stage an explicit redistributable allowlist.
Preserve untracked user helper scripts; do not reset to old handoff commits.
