# Windows Bootstrap Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a redistributable, asset-safe Windows build around a pinned NWiiRecomp source import and replace ad-hoc Wii memory initialization with a tested Pokemon Rumble boot contract.

**Architecture:** The repository owns policy, build orchestration, and a small boot-memory module layered into the imported NWiiRecomp runtime. The boot module produces validated address/value writes through a narrow guest-memory interface, so it can be tested without SDL, OpenGL, or translated game code.

**Tech Stack:** C++20, CMake 3.25+, Visual Studio 2022/MSVC x64, Ninja, PowerShell 7/Windows PowerShell 5.1, GitHub Actions, NWiiRecomp revision `595f176f1d24cc54ff2e8389feed12d7fb553cc2`.

**Spec:** `docs/superpowers/specs/2026-08-30-pokemon-rumble-native-port-design.md`

## Global Constraints

- Target only Pokemon Rumble USA WiiWare `WPSE01_01` with `main.dol` SHA-1 `fd9a2c00c97e420a42355e2c27f3dc0ebbd3d8f9` and entry point `0x80004050`.
- Do not commit WADs, DOLs, tickets, TMDs, extracted filesystems, NAND data, Nintendo-owned media, or generated translated game code.
- Use NWiiRecomp for ahead-of-time PowerPC translation; do not use Dolphin as the runtime.
- Do not ship a general PowerPC JIT or interpreter.
- Preserve NWiiRecomp's modified noncommercial license and visible attribution.
- Write tests before production changes and commit each independently verified milestone.

---

### Task 1: Repository safety boundary

**Files:**
- Create: `.gitignore`
- Create: `LICENSE`
- Create: `THIRD_PARTY_NOTICES.md`
- Create: `README.md`
- Create: `tools/Check-RepositoryPolicy.ps1`
- Create: `tests/repository_policy.Tests.ps1`

**Interfaces:**
- Consumes: a Git worktree path through `-RepositoryRoot`.
- Produces: `tools/Check-RepositoryPolicy.ps1 -RepositoryRoot C:\src\pkmnrbl` returning exit code 0 only when tracked paths are redistributable.

- [ ] **Step 1: Write the failing policy tests**

Create temporary Git repositories and verify a harmless `README.md` passes,
while tracked `game.wad`, `main.dol`, `ticket.bin`, `nand/title/...`, and
`generated/ppc/func_80004050.cpp` each fail with the offending path reported.

- [ ] **Step 2: Run the tests and observe the missing checker failure**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tests/repository_policy.Tests.ps1`

Expected: nonzero exit because `tools/Check-RepositoryPolicy.ps1` is absent.

- [ ] **Step 3: Implement the checker and ignore rules**

The checker must enumerate tracked paths with `git ls-files -z`, normalize `/`,
and reject case-insensitive matches for these categories:

```powershell
$ForbiddenExtensions = @('.wad', '.dol', '.tmd', '.tik', '.cert', '.app',
  '.iso', '.wbfs', '.wia', '.rvz', '.gcm', '.rpx', '.rpl')
$ForbiddenRoots = @('local/', 'generated/', 'nand/', 'extracted/',
  'game/', 'content/', 'title/')
```

Also reject `boot.bin`, `bi2.bin`, `ticket.bin`, `title.tmd`, `uid.sys`, and
`setting.txt` by basename. `.gitignore` must cover those rules plus build trees,
IDE output, logs, generated translation-unit patterns, and user configuration.

- [ ] **Step 4: Run policy tests and the checker against this repository**

Run both:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/repository_policy.Tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Check-RepositoryPolicy.ps1 -RepositoryRoot .
```

Expected: all test cases pass and the repository is accepted.

- [ ] **Step 5: Commit the safety boundary**

```text
git add .gitignore LICENSE THIRD_PARTY_NOTICES.md README.md tools/Check-RepositoryPolicy.ps1 tests/repository_policy.Tests.ps1 docs/superpowers
git commit -m "chore: establish asset-safe port repository"
```

### Task 2: Pinned NWiiRecomp runtime and tooling import

**Files:**
- Create: `third_party/NWiiRecomp/` from the pinned upstream revision
- Create: `third_party/NWiiRecomp/UPSTREAM_REVISION`
- Create: `cmake/NWiiRecomp.cmake`
- Create: `CMakeLists.txt`

**Interfaces:**
- Consumes: imported `nWiiAnalyzer`, `nWiiRecomp`, and `nWiiRuntime` sources.
- Produces: CMake targets `nwiianalyzer`, `nwiirecomp`, and `nwiiruntime`; option `PKMNRBL_BUILD_TOOLS` defaults to `ON`.

- [ ] **Step 1: Write an asset-free configure smoke test**

Add `tests/cmake_configure.Tests.ps1`. It must copy the source tree to a clean
temporary directory, configure with `-DPKMNRBL_BUILD_TOOLS=OFF
-DPKMNRBL_FETCH_DEPS=OFF`, and assert that configuration succeeds without a
WAD, DOL, generated source, network fetch, SDL, or OpenGL.

- [ ] **Step 2: Run it and observe failure because no project exists**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tests/cmake_configure.Tests.ps1`

Expected: nonzero exit because root `CMakeLists.txt` does not exist.

- [ ] **Step 3: Import the minimal upstream source set**

Copy `nWiiAnalyzer`, `nWiiRecomp`, `nWiiRuntime`, the vendored GLAD source, and
the upstream license from revision `595f176f1d24cc54ff2e8389feed12d7fb553cc2`.
Do not copy `image`, `nWiiStudio`, example game input, build output, or
`third_party/dolphin-sigdb/totaldb.dsy`. Record the exact revision.

- [ ] **Step 4: Add root CMake orchestration**

Create an interface target `pkmnrbl_build_options` requiring C++20. Add
`/W4 /permissive- /EHsc` for MSVC, and expose a helper that adds `/bigobj` to
generated translation targets. When `PKMNRBL_FETCH_DEPS=OFF`, build only the
standalone boot library/tests and do not enter the SDL-dependent runtime.

- [ ] **Step 5: Re-run configure smoke and repository-policy tests**

Expected: both pass; no downloaded or copyrighted title data appears.

- [ ] **Step 6: Commit the pinned source import**

```text
git add CMakeLists.txt cmake tests/cmake_configure.Tests.ps1 third_party/NWiiRecomp THIRD_PARTY_NOTICES.md
git commit -m "build: import pinned NWiiRecomp components"
```

### Task 3: Test-driven Wii low-memory bootstrap

**Files:**
- Create: `runtime/boot/guest_memory.h`
- Create: `runtime/boot/wii_memory_layout.h`
- Create: `runtime/boot/wii_memory_layout.cpp`
- Create: `tests/wii_memory_layout_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `GuestMemory::write32_be(uint32_t address, uint32_t value)`.
- Produces: `WiiMemoryLayout make_wpse01_layout(uint32_t arena_lo, uint32_t arena_hi)`; `ValidationResult validate(const WiiMemoryLayout&)`; `bool install_wii_memory_layout(GuestMemory&, const WiiMemoryLayout&, std::string&)`.

- [ ] **Step 1: Write failing tests with literal expectations**

Use a real in-memory `GuestMemory` implementation. Assert the literal bytes at
all addresses in the spec, not values computed from production constants.
Include tests that reject unaligned MEM1 bounds, reversed MEM1/MEM2/IPC ranges,
MEM2/IPC overlap, IPC outside mapped MEM2, and an IPC arena smaller than
`0x800 + 0x1540 + 0xA0` bytes.

- [ ] **Step 2: Build the test and verify it fails before implementation**

Run:

```text
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target wii_memory_layout_tests
ctest --preset windows-msvc-debug -R wii_memory_layout --output-on-failure
```

Expected: compile/link failure because the layout functions are undefined.

- [ ] **Step 3: Implement validation and big-endian installation**

Keep constants private to `wii_memory_layout.cpp`. Installation first validates
the complete layout and performs no writes on failure. On success, write the
full table from the spec, including MEM1 `0x3100..0x3110`, accessible MEM2 end,
Hollywood revision, and GDDR vendor code.

- [ ] **Step 4: Run tests and mutation-check the boundaries**

Expected: all cases pass. Temporarily alter one expected production address,
one range comparison, and byte order; each alteration must make a named test
fail before being reverted.

- [ ] **Step 5: Commit the bootstrap module**

```text
git add runtime/boot tests/wii_memory_layout_tests.cpp CMakeLists.txt
git commit -m "fix: validate Wii MEM1 MEM2 and IPC bootstrap"
```

### Task 4: Integrate the boot contract into NWiiRecomp

**Files:**
- Modify: `third_party/NWiiRecomp/nWiiRuntime/src/core/main.cpp`
- Modify: `third_party/NWiiRecomp/nWiiRuntime/CMakeLists.txt`
- Create: `runtime/boot/nwii_guest_memory.h`
- Create: `runtime/boot/nwii_guest_memory.cpp`
- Create: `tests/nwii_boot_integration_tests.cpp`

**Interfaces:**
- Consumes: NWiiRecomp `CPUContext::mmu` and `WiiMemoryLayout`.
- Produces: `install_wpse01_boot_memory(CPUContext&, uint32_t arena_lo, uint32_t arena_hi, std::string&)` called once after DOL/FST placement and before `run_game`.

- [ ] **Step 1: Write a failing integration test against a real NWii MMU**

Construct `CPUContext`, install a layout with arena bounds
`0x81000000..0x81700000`, then read through the real MMU. Assert boot magic,
MEM1 usable bounds, MEM2 usable bounds, IPC bounds, and that bytes immediately
outside the written low-memory fields remain unchanged.

- [ ] **Step 2: Run the test and observe the missing adapter failure**

Expected: compile failure for `install_wpse01_boot_memory`.

- [ ] **Step 3: Add the MMU adapter and replace loader writes**

Remove the scattered Wii-specific writes in `main.cpp`. Retain separate
GameCube behavior. Compute DOL/stack/FST-adjusted MEM1 bounds as before, pass
them to the new installer, print the validated ranges once, and abort before
entry with a clear error if validation or installation fails.

- [ ] **Step 4: Run unit and integration tests**

Run: `ctest --preset windows-msvc-debug --output-on-failure`

Expected: all boot tests pass.

- [ ] **Step 5: Commit the runtime integration**

```text
git add runtime/boot third_party/NWiiRecomp/nWiiRuntime tests/nwii_boot_integration_tests.cpp
git commit -m "fix: install validated Wii boot memory before game entry"
```

### Task 5: Windows presets and CI

**Files:**
- Create: `CMakePresets.json`
- Create: `.github/workflows/windows.yml`
- Create: `tools/Invoke-NativeBuild.ps1`
- Create: `config/WPSE01_01/recomp.toml`
- Modify: `README.md`
- Modify: `third_party/NWiiRecomp/nWiiRecomp/src/recompiler.cpp`

**Interfaces:**
- Consumes: ignored `local/WPSE01_01/` and verified `main.dol`.
- Produces: asset-free CI targets and local `build/windows-release/PokemonRumble.exe` when legal title data is present.

- [ ] **Step 1: Write failing script tests**

Add `tests/native_build_script.Tests.ps1` using temporary fake inputs. Assert
that the build script refuses a missing DOL, a wrong SHA-1, and a wrong entry
point before running NWiiRecomp. Assert that `-ConfigureOnly -SkipGame` invokes
the asset-free CMake preset successfully.

- [ ] **Step 2: Run tests and observe failure before the script exists**

Expected: nonzero exit.

- [ ] **Step 3: Add presets, build script, and generated-project fixes**

The script must hash with `Get-FileHash -Algorithm SHA1`, pass only ignored
paths to NWiiRecomp, and never copy title files into the source tree. Update
the generator so MSVC translation targets receive `/bigobj` and the final
target uses `OUTPUT_NAME PokemonRumble`.

- [ ] **Step 4: Add Windows GitHub Actions**

Use `windows-2022`, configure with the asset-free preset, build boot/runtime
tests, run CTest and PowerShell policy tests, and upload only test logs on
failure. Do not download or synthesize game data.

- [ ] **Step 5: Verify locally where possible and in CI**

Run all PowerShell tests locally. Push the development branch, wait for the
Windows workflow, and fix failures until every job is green.

- [ ] **Step 6: Commit Windows tooling**

```text
git add .github CMakePresets.json config tools tests README.md third_party/NWiiRecomp/nWiiRecomp/src/recompiler.cpp
git commit -m "ci: add native Windows x64 build pipeline"
```

### Task 6: Reproduce and advance the local boot

**Files:**
- Create: `runtime/diagnostics/ipc_trace.h`
- Create: `runtime/diagnostics/ipc_trace.cpp`
- Create: `tests/ipc_arena_sequence_tests.cpp`
- Modify: relevant IOS device/runtime files identified by the trace

**Interfaces:**
- Consumes: verified local WPSE01_01 extraction and IOS request/allocation events.
- Produces: deterministic trace records containing PC, device, command, result, and IPC low/high before/after.

- [ ] **Step 1: Obtain explicit permission before running downloaded extraction tooling**

Request permission to download and execute the official pinned decomp-toolkit
Windows release. Verify its published release identity/hash before execution.
Do not bypass this gate with another downloaded executable.

- [ ] **Step 2: Prepare the local title and generate translated code**

Extract only below `local/WPSE01_01`, verify `main.dol` SHA-1, translate below
`generated/WPSE01_01`, and build the release executable. Run the repository
policy checker before and after generation.

- [ ] **Step 3: Capture the first post-bootstrap failure**

Run the native executable with debug IPC tracing. The acceptance condition for
this step is that none of these strings appears:

```text
APP ERROR: Not enough IPC arena
(ddrAllocAligned32) Not enough space
Allocation of diCommand blocks failed
Something overwrote the context magic
```

- [ ] **Step 4: Add one failing regression test for the next failure**

Use the trace to isolate the first incorrect runtime boundary. The test must
name the exact bad transition and exercise the real allocator, scheduler, or
device handler involved; do not assert on log text alone.

- [ ] **Step 5: Implement the minimal fix and re-run the complete gate**

Run unit tests, integration tests, repository policy, Windows CI, and the local
boot. Record the new last-good guest PC and subsystem in the commit body.

- [ ] **Step 6: Commit the verified boot advancement**

```text
git add runtime tests third_party/NWiiRecomp
git commit -m "fix: advance WPSE01 IPC boot" -m "Record the last-good guest PC and subsystem in this commit body."
```

Repeat Steps 3-6 for each new boot failure until the title reaches the next
user-visible milestone.
