# First Native Build Status and Remaining Work

Last updated: 2026-09-01

This document is the handoff for the first native Windows x86-64 build of
Pokemon Rumble USA WiiWare (`WPSE01_01`). It intentionally contains no WAD,
DOL, ticket, TMD, extracted filesystem, NAND data, generated translated game
code, Nintendo-owned media, or locally built executable.

## What "first build" means

There are three separate milestones:

1. **Native binary:** achieved locally. NWiiRecomp translated all 14,373
   discovered PowerPC functions and the generated project linked a Windows
   x86-64 `PokemonRumble.exe` using the local Zig toolchain.
2. **Reproducible MSVC binary:** not yet verified with the real title on this
   machine because Visual Studio 2022 lacks the C++ workload. GitHub CI proves
   the redistributable runtime and a synthetic generated project under MSVC,
   but cannot legally contain or build the real translated title.
3. **First usable native build:** not yet achieved. The executable boots
   Revolution OS and IOS initialization, then waits during Bluetooth/HCI
   startup before reaching a title screen.

The acceptance target for the first usable build is: a local Windows x86-64
executable built from a legally obtained WPSE01 title, reaching the first
visible title screen, accepting one controller, and doing so without Dolphin
or a default general-purpose PowerPC interpreter.

## Fixed inputs and legal boundary

- Target: Pokemon Rumble USA WiiWare `WPSE01_01`
- Expected `main.dol` SHA-1:
  `fd9a2c00c97e420a42355e2c27f3dc0ebbd3d8f9`
- Expected entry point: `0x80004050`
- Expected title ID: `0001000157505345`
- Static recompilation base: NWiiRecomp revision
  `595f176f1d24cc54ff2e8389feed12d7fb553cc2`

The user supplies legal title data only below ignored `local/WPSE01_01/`.
Translation output stays below ignored `generated/WPSE01_01/`, and all
binaries/logs stay below ignored `build/`. None of those paths may be pushed.
The repository policy checker must pass before every push.

## Current verified boot position

The low-memory and arena blocker is resolved. The runtime installs and reports:

```text
MEM1 0x804CBD40-0x81700000
MEM2 0x90000800-0x93E00000
IPC  0x93E00000-0x94000000
```

Boot now reaches Revolution OS, IOS, ES, FS, DI, STM, GX/CP initialization,
and opens `/dev/usb/oh1/57e/305`. The previous failures no longer appear:

- `APP ERROR: Not enough IPC arena`
- `(ddrAllocAligned32) Not enough space`
- `Allocation of diCommand blocks failed`
- `(newContext) Something overwrote the context magic`

The first unresolved transaction is an asynchronous USB interrupt read:

```text
guest PC       0x80313DE0
guest LR       0x80313DDC
request        0x93E00020
USB command    2 (interrupt message)
endpoint       0x81 (HCI event endpoint)
length         0x025C
output buffer  physical MEM2 0x10000880
result         IPC_NO_REPLY
```

The guest then polls byte `0x804AB3C6` for state `5`. No USB control message
containing the first HCI command is submitted before that poll. Physical MEM2
addressing is not the fault: the MMU deliberately accepts the physical and
virtual MEM2 aliases.

## Exact remaining work, in order

| Order | Required work | Proof required before advancing |
|---:|---|---|
| 1 | Validate generated Windows linking on a clean MSVC runner. The generated project must use portable `setjmp`, define `SDL_MAIN_HANDLED`, link `ws2_32`, apply `/bigobj`, and include the temporary interpreter only when `PKMNRBL_ENABLE_BRINGUP_INTERPRETER=ON`. | GitHub Windows workflow passes policy, CTest, runtime build, synthetic generated-project regression, and native build-script tests. |
| 2 | Trace the pending IOS request without changing behavior. Record PPC/ARM IPC control registers, PI interrupt state, request result field, callback address, and guest state before submission, immediately after `IPC_NO_REPLY`, and only after `ipc_post_reply`. | One trace identifies whether the guest receives an early reply/interrupt or whether its own Bluetooth state machine fails before the first control transfer. |
| 3 | Add a real regression test for the transition identified in step 2. If `IPC_NO_REPLY` is the fault, the test must submit through the real IPC MMIO path and prove that no reply bit, result completion, callback, or interrupt is produced until `ipc_post_reply`. If the guest state machine is the fault, test that exact USB/HCI boundary instead. | The new test fails against the current runtime for the observed reason, not because of source-text matching or a mock. |
| 4 | Implement only the confirmed asynchronous IPC fix. Do not synthesize an unsolicited HCI event: endpoint `0x81` is expected to remain pending until a command or controller event exists. | New regression passes; all boot/MMU tests pass; the local trace shows the first HCI control command or a later, newly identified boundary. |
| 5 | Complete the minimum Bluetooth HCI state machine needed by the title. Parse real control-transfer vectors and return command-complete/status payloads with correct opcode-specific fields; queue events; complete the pending interrupt request through `ipc_post_reply`; preserve request ordering and lengths. | Guest byte `0x804AB3C6` reaches state `5`, the poll at `0x80313DE0` exits, and the boot reaches a later subsystem. Each newly required opcode has a literal-payload test. |
| 6 | Correct WPSE01 identity before title-specific ES/DI behavior matters. The current default is `RSZK`; the runtime must derive or configure `WPSE` and return title ID `0001000157505345` consistently from ES ticket/title queries. | Focused ES/DI tests read the literal WPSE title ID, and the local boot no longer reports `00010000-52535A4B`. |
| 7 | Continue trace-driven bring-up through the first rendered frame. For each new stop, capture the first incorrect boundary, write a failing test, make the smallest runtime fix, and record the new last-good PC/subsystem. Likely surfaces are VI/GX, virtual filesystem/save data, timing, and remaining IOS commands; none should be changed until a trace proves it is the next blocker. | Non-headless run creates a stable window and displays the first title-owned frame without fatal runtime errors. |
| 8 | Connect one native input source to the title's Wii input path. The runtime currently maps keyboard/gamepad states, but Bluetooth Wiimote mode is explicitly unfinished. Use the existing `WiimoteState`/KPAD/WPAD contract or complete the virtual Bluetooth controller path, whichever the traced game calls require. | A deterministic input test verifies button layout, and a keyboard or XInput controller can navigate the first menu. |
| 9 | Remove the temporary PowerPC fallback from the deliverable. Although all discovered functions are statically translated, current bring-up still visits interpreter address `0x802B8CEC`. Identify the unsupported dispatch/instruction path, generate or implement its native equivalent, then build with `PKMNRBL_ENABLE_BRINGUP_INTERPRETER=OFF`. | Full title boot test contains no `[Interp]` execution, the interpreter source is absent from the game target, and the first menu still works. |
| 10 | Produce the clean MSVC title build locally and re-run every gate. Install the Visual Studio 2022 Desktop C++ workload, place the legally extracted title under `local/WPSE01_01/extracted/`, and run `tools/Invoke-NativeBuild.ps1`. | `build/windows-release/PokemonRumble.exe` exists as x86-64 PE; CTest, all PowerShell suites, repository policy, Windows CI, and the local boot acceptance test pass. |

## Immediate next experiment

The next change must be diagnostic, not a guessed USB response. Instrument
`third_party/NWiiRecomp/nWiiRuntime/src/hw/hw_ipc.cpp` around the
`IPC_NO_REPLY` branch and `ipc_post_reply`, then repeat the headless boot. The
current code schedules an IPC delay and changes control bits even for a
pending request; this is a hypothesis, not yet a confirmed defect. The trace
must establish whether that behavior completes or interrupts the request too
early before any production fix is written.

## Build commands

Asset-free safety/configuration gate:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Check-RepositoryPolicy.ps1 -RepositoryRoot .
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-NativeBuild.ps1 -ConfigureOnly -SkipGame
```

Real local build after the legal title is placed in the ignored input path:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-NativeBuild.ps1
```

The resulting executable embeds translated title code and must remain local;
GitHub stores only the redistributable port source and tooling.

