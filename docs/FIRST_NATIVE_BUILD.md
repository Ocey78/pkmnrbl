# First native build: status and remaining work

Updated 2026-10-10. Nintendo files, generated title code, private traces and
the locally linked game executable must never be uploaded.

## Fixed architecture and identity

NWiiRecomp ahead-of-time PPC translation with a native Windows x86-64 runtime.
No Dolphin runtime, JIT/emulator wrapper, Unity or gameplay rewrite.

- Title: WPSE01_01; game ID: WPSE; title ID: 0001000157505345.
- main.dol SHA-1: fd9a2c00c97e420a42355e2c27f3dc0ebbd3d8f9.
- Entry: 0x80004050; latest AOT discovery contains 14,375 functions.
- NWiiRecomp base: 595f176f1d24cc54ff2e8389feed12d7fb553cc2.
- Legal inputs stay in ignored local/WPSE01_01/.
- AOT output stays in ignored generated/WPSE01_01/.
- Executables, screenshots and logs stay in ignored build/.

## Verified milestones and limits

A native Windows executable links with Visual Studio 2022 x64. The local
interpreter-OFF build has progressed through resource loading. The Release
suite now contains 31 passing tests, including native input, ES content and real
GPU tests. The current interpreter-OFF build renders the English instruction
screen and the actual Pokemon Rumble title menu (New Game / Options). The user
confirms the requested controls work and reach the save screen, but reports a
black screen after starting a save. Save persistence and gameplay are NOT yet
verified; this is not a finished release. See the latest section below;
the older sections are chronological evidence, not the current blocker.
The last completed title executable is the controls milestone e6aa265. Further
source fixes below pass regressions but still need the combined game rebuild.

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
