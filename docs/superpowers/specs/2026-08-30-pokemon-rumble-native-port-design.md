# Pokemon Rumble Native Windows Port Design

## Objective

Build a native Windows x86-64 `PokemonRumble.exe` for the USA WiiWare title
`WPSE01_01`, using NWiiRecomp for ahead-of-time PowerPC translation and a
purpose-built Wii compatibility runtime. The shipped program must not execute
the game's PowerPC instruction stream through a general-purpose emulator.

The verified target is:

- Title: Pokemon Rumble USA WiiWare (`WPSE01_01`)
- Title ID: `0001000157505345`
- Required IOS from the local WAD TMD: IOS56
- `main.dol` SHA-1: `fd9a2c00c97e420a42355e2c27f3dc0ebbd3d8f9`
- Entry point: `0x80004050`

## Non-goals

- No Dolphin runtime or embedded Dolphin core.
- No general PowerPC JIT or interpreter in the finished executable.
- No WAD, DOL, TMD, ticket, extracted filesystem, NAND, generated translated
  game code, or Nintendo-owned media in Git.
- No promise of commercial-use rights. NWiiRecomp's license restricts this
  derivative work to personal, educational, and noncommercial use.
- No gameplay enhancements before the baseline title reaches reproducible
  native boot and rendering milestones.

## Repository boundary

The public repository contains only source, build logic, tests, reverse-
engineering metadata that is safe to redistribute, and third-party notices.
The user's legally obtained title data lives below `local/`; generated C++ and
build products live below `generated/` and `build/`. All three trees are
ignored and a repository-policy check rejects forbidden tracked extensions and
paths before CI builds anything.

The minimum repository layout is:

```text
.
|-- .github/workflows/windows.yml
|-- CMakeLists.txt
|-- CMakePresets.json
|-- LICENSE
|-- README.md
|-- THIRD_PARTY_NOTICES.md
|-- cmake/
|-- config/WPSE01_01/
|-- docs/superpowers/{specs,plans}/
|-- runtime/
|   `-- boot/
|-- tests/
|-- tools/
|-- third_party/NWiiRecomp/
|-- local/                 # ignored
|-- generated/             # ignored
`-- build/                 # ignored
```

## NWiiRecomp source policy

Import the analyzer, recompiler, and runtime source from upstream revision
`595f176f1d24cc54ff2e8389feed12d7fb553cc2`. Exclude nWiiStudio, screenshots,
sample game data, and the Dolphin signature database. Keep the upstream
license verbatim and record the revision in `THIRD_PARTY_NOTICES.md`.

The root project owns dependency resolution and Windows build flags. In
particular, generated translation units use MSVC `/bigobj`, and the generated
executable is named `PokemonRumble`. Downloaded dependencies are pinned by tag
or commit and are never title data.

## Native execution boundary

NWiiRecomp translates all statically discovered PowerPC functions into C++.
The compatibility runtime supplies guest memory, scheduling, hardware-register
seams, GX rendering, AX audio, input, and IOS device services. A temporary
interpreter may be enabled only as an instrumented bring-up fallback for an
unresolved indirect target or low-memory trampoline. Every use must log its
guest address so it can be replaced with static translation before release.

## Wii low-memory contract

Low memory is an explicit boot protocol, not a collection of ad-hoc writes in
the loader. `runtime/boot/wii_memory_layout.*` owns a typed
`WiiMemoryLayout`, validates it, and writes it through a four-byte big-endian
guest-memory interface.

For the baseline 24 MiB MEM1 / 64 MiB MEM2 Wii configuration, the following
fields are required before control reaches `0x80004050`:

| Cached address | Meaning | Baseline value |
|---:|---|---:|
| `0x80000020` | boot magic | `0x0D15EA5E` |
| `0x80000024` | apploader version | `0x00000001` |
| `0x80000028` | physical MEM1 size | `0x01800000` |
| `0x8000002C` | nonzero console type | `0x00000021` |
| `0x80000030` | boot MEM1 arena low | derived, 32-byte aligned |
| `0x80000034` | boot MEM1 arena high | derived, 32-byte aligned |
| `0x800000F0` | simulated MEM1 size | `0x01800000` |
| `0x800000F8` | bus clock | `0x0E7BE2C0` (243 MHz) |
| `0x800000FC` | CPU clock | `0x2B73A840` (729 MHz) |
| `0x80003100` | physical MEM1 size | `0x01800000` |
| `0x80003104` | simulated MEM1 size | `0x01800000` |
| `0x8000310C` | usable MEM1 start | boot arena low |
| `0x80003110` | usable MEM1 end | boot arena high |
| `0x80003118` | physical MEM2 size | `0x04000000` |
| `0x8000311C` | simulated MEM2 size | `0x04000000` |
| `0x80003120` | accessible MEM2 end | `0x94000000` |
| `0x80003124` | usable MEM2 start | `0x90000800` |
| `0x80003128` | usable MEM2 end | `0x93E00000` |
| `0x80003130` | IPC buffer start | `0x93E00000` |
| `0x80003134` | IPC buffer end | `0x94000000` |
| `0x80003138` | Hollywood revision | `0x00000011` |
| `0x80003158` | GDDR vendor code | `0x00000023` |

The FST may lower MEM1 arena high. The loaded DOL and CRT stack may raise MEM1
arena low. Validation rejects unaligned, reversed, overlapping, or unmapped
ranges. The MEM2 arena must end at or before the IPC buffer, and the IPC buffer
must be wholly inside mapped MEM2.

The initial 2 MiB IPC arena is intentionally generous. Pokemon Rumble's SDK
first reserves `0x800` bytes for the IPC client, then `0x1540` bytes for the
FS path and heap, followed by aligned DVD structures. Tests exercise the exact
allocation sequence and prove it cannot overlap the usable MEM2 arena.

## IOS IPC bring-up

The first supported devices are the ones already observed during boot:

- `/dev/stm/immediate`
- `/dev/stm/eventhook`
- `/dev/fs`
- `/dev/di`

IOS requests remain guest ABI structures in guest memory. The runtime converts
physical MEM1/MEM2 pointers to cached addresses in one shared helper, validates
every range before access, and completes async requests through the scheduler.
Device handlers return IOS error codes rather than host exceptions.

Each IPC allocation and request emits structured diagnostics in debug builds:
guest PC, device, command, buffer low/high before and after, and result. This
makes the four known allocator/context errors attributable to the write that
caused them rather than to the later assertion that noticed corruption.

## Windows build and CI

The supported toolchain is Visual Studio 2022, CMake 3.25 or newer, Ninja, and
the x64 MSVC compiler. `CMakePresets.json` provides `windows-msvc-debug` and
`windows-msvc-release` presets. CI runs on `windows-2022` and performs:

1. repository-policy verification;
2. dependency configure with pinned revisions;
3. x64 build with `/bigobj` for generated sources;
4. unit and integration tests through CTest;
5. an asset-free host-tools smoke test.

CI never downloads a game image and cannot build `PokemonRumble.exe` without
the local verified DOL. A local preparation script verifies the WAD and DOL
hashes, extracts into ignored directories, invokes NWiiRecomp, and configures
the final executable. It refuses an unexpected title ID, region, revision, or
hash.

## Verification gates

Milestones are accepted only with evidence:

1. Repository safety: policy tests pass and `git ls-files` contains no banned
   path or extension.
2. Tooling: Windows CI builds the redistributable host/runtime targets with no
   title data.
3. Memory bootstrap: unit tests verify every address/value above, all invalid
   range cases, big-endian byte order, and the real SDK allocation sequence.
4. Game integration: the local executable reports the expected title/hash and
   enters `0x80004050`.
5. IPC: boot passes IPC client, STM, FS, and DVD allocation without any of the
   four known error messages.
6. Subsequent debugging: each new boot failure gets a minimal regression test
   before its runtime fix.

## Commit strategy

Commit independently reviewable milestones: repository safety and design,
pinned NWiiRecomp import, tested low-memory bootstrap, Windows CI/tooling, and
each later IPC/boot fix. Generated game code and local evidence logs are never
staged.
