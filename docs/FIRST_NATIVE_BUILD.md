# First native build: status and remaining work

Updated 2026-09-16. Nintendo files, generated title code, private traces and
the locally linked game executable must never be uploaded.

## Fixed architecture and identity

NWiiRecomp ahead-of-time PPC translation with a native Windows x86-64 runtime.
No Dolphin runtime, JIT/emulator wrapper, Unity or gameplay rewrite.

- Title: WPSE01_01; game ID: WPSE; title ID: 0001000157505345.
- main.dol SHA-1: fd9a2c00c97e420a42355e2c27f3dc0ebbd3d8f9.
- Entry: 0x80004050; 14,373 functions regenerated on 2026-09-15.
- NWiiRecomp base: 595f176f1d24cc54ff2e8389feed12d7fb553cc2.
- Legal inputs stay in ignored local/WPSE01_01/.
- AOT output stays in ignored generated/WPSE01_01/.
- Executables, screenshots and logs stay in ignored build/.

## Verified milestones and limits

A native Windows executable has linked with both Zig and the now-installed
Visual Studio 2022 x64 compiler. The latest MSVC executable includes the AOT
syscall correction and imported renderer diagnostics. Nine Release CTests
pass: memory layout, real-MMU boot, async IPC, identity, AI counter, renderer
diagnostic latch, and three compiled synthetic AOT syscall cases. The
standalone synthetic generated-project export regression also passed.

There is still NO verified title screen, visible title-owned frame, input
navigation or interpreter-free release. Draw submissions are not pixels.

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

## Remaining work for the first usable build

1. Finish renderer isolation; inspect GL errors, shader compile/link logs,
   actual vertices/projection, TEV/texture state and EFB copy/clear ordering.
   Write a failing regression for the proven boundary, implement the minimal
   correction, rebuild and inspect a normal-mode title-owned frame.
2. Verify ES content service. Current ES open 09/read 0A/seek 23 return
   success without serving content, and read buffers are zeroed. The launch
   directory contains only main.dol; sibling local/WPSE01_01 contains seven
   local app files and a TMD. Trace requested indices and buffers, validate
   the local content mapping/size/hash, then add synthetic open/read/seek/close,
   EOF, invalid handle and bounds tests before serving local content.
   Do not assume every black frame has this cause.
3. Re-run bounded real title boots to verify the cache-loop runaway does not
   recur. Keep PC/LR/CTR/context ownership and rendering evidence.
4. Connect and verify one native input source through the title's actual Wii
   input path. Require navigation of the first menu; HCI startup is not input.
5. Investigate fallback at 802B8CEC and 802B8CBC, plus any new fallback sites.
   Supply missing AOT/native coverage. Audit fallback syscall continuation
   separately; the current fix is in the AOT emitter. Build and run with
   PKMNRBL_ENABLE_BRINGUP_INTERPRETER=OFF before calling it release-ready.
6. Run all MSVC tests, synthetic export, policy tests and Windows CI; prove a
   clean checkout rebuilds with separately supplied legal title data.
   Never publish the resulting game executable or translated source.

## Reproduction

Full legal local build: tools/Invoke-NativeBuild.ps1.
After emitter changes, rebuild nwiirecomp, run it with
config/WPSE01_01/recomp.toml, then rebuild build/windows-release/generated
with configuration Release and target PokemonRumble.
The executable is build/windows-release/PokemonRumble.exe; pass
local/WPSE01_01/extracted as its argument.

Build all test executables before full ctest --preset windows-msvc-release.
CI builds standalone tests in Debug, then the host and AOT tests in Release.
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
