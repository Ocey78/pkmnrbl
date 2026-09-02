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

The asynchronous IPC defect at the first USB interrupt read is resolved. The
runtime now acknowledges a pending IOS request with `Y2` without fabricating a
`Y1` reply, then preserves asynchronous and synchronous replies in submission
order. A real-MMIO regression covers the nested completion case that previously
overwrote the older reply address.

The corrected boot completes the pending endpoint `0x81` request, submits an
endpoint `0x82` bulk read, and processes these HCI command-complete events:

```text
0x0C03  Reset
0x1005  Read Buffer Size
0x0C24  Write Class of Device
0x0C13  Write Local Name
0x0C0A  Write Scan Enable
```

After the fifth command the guest submits another asynchronous endpoint `0x81`
read and waits at:

```text
guest PC       0x80313DE0
guest LR       0x80313DDC
request        0x93E002C0
USB command    2 (interrupt message)
endpoint       0x81 (HCI event endpoint)
length         0x025C
output buffer  physical MEM2 0x100022C0
result         IPC_NO_REPLY
```

The guest polls byte `0x804AB3C6` for state `5`. The next required controller
event or guest transition has not yet been identified. Physical MEM2 addressing
is not the fault: the MMU deliberately accepts the physical and virtual MEM2
aliases. No unsolicited controller event will be synthesized without first
proving that the guest state machine expects it.

## Exact remaining work, in order

| Order | Required work | Proof required before advancing |
|---:|---|---|
| 1 | Trace guest byte `0x804AB3C6` across the five verified HCI completions and identify the exact condition that should move it to state `5`. Correlate each write with translated guest code and its event parser. | A trace records the state sequence and names the exact missing or malformed event/field; no response is guessed from the stationary poll alone. |
| 2 | Complete only the minimum Bluetooth HCI behavior proven by step 1. Parse the real transfer vectors, preserve event ordering and lengths, and add a literal-payload regression for every new opcode or event. | Guest byte `0x804AB3C6` reaches state `5`, the poll at `0x80313DE0` exits, and boot reaches a later subsystem. |
| 3 | Correct WPSE01 identity before title-specific ES/DI behavior matters. The current default is `RSZK`; the runtime must derive or configure `WPSE` and return title ID `0001000157505345` consistently from ES ticket/title queries. | Focused ES/DI tests read the literal WPSE title ID, and the local boot no longer reports `00010000-52535A4B`. |
| 4 | Continue trace-driven bring-up through the first rendered frame. For each new stop, capture the first incorrect boundary, write a failing test, make the smallest runtime fix, and record the new last-good PC/subsystem. Likely surfaces are VI/GX, virtual filesystem/save data, timing, and remaining IOS commands; none should be changed until a trace proves it is the next blocker. | Non-headless run creates a stable window and displays the first title-owned frame without fatal runtime errors. |
| 5 | Connect one native input source to the title's Wii input path. The runtime currently maps keyboard/gamepad states, but Bluetooth Wiimote mode is explicitly unfinished. Use the existing `WiimoteState`/KPAD/WPAD contract or complete the virtual Bluetooth controller path, whichever the traced game calls require. | A deterministic input test verifies button layout, and a keyboard or XInput controller can navigate the first menu. |
| 6 | Remove the temporary PowerPC fallback from the deliverable. Although all discovered functions are statically translated, current bring-up still visits interpreter address `0x802B8CEC`. Identify the unsupported dispatch/instruction path, generate or implement its native equivalent, then build with `PKMNRBL_ENABLE_BRINGUP_INTERPRETER=OFF`. | Full title boot test contains no `[Interp]` execution, the interpreter source is absent from the game target, and the first menu still works. |
| 7 | Produce the clean MSVC title build locally and re-run every gate. Install the Visual Studio 2022 Desktop C++ workload, place the legally extracted title under `local/WPSE01_01/extracted/`, and run `tools/Invoke-NativeBuild.ps1`. | `build/windows-release/PokemonRumble.exe` exists as x86-64 PE; CTest, all PowerShell suites, repository policy, Windows CI, and the local boot acceptance test pass. |

## Immediate next experiment

The next change is diagnostic. Run the local executable with
`NWII_WATCH=0x804AB3C6` and USB tracing enabled, correlate every state write
with the five verified HCI completions, and inspect the translated handler that
owns the last transition. Only after that evidence identifies a missing or
malformed controller event should the USB/HCI implementation change.

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
