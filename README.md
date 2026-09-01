# pkmnrbl

An asset-free native Windows port foundation for the USA WiiWare release of
Pokémon Rumble (`WPSE01_01`). The eventual port uses ahead-of-time PowerPC
translation and a purpose-built compatibility runtime; it does not ship a
general-purpose emulator.

The current boot position, exact remaining blockers, and acceptance gates for
the first usable build are tracked in
[`docs/FIRST_NATIVE_BUILD.md`](docs/FIRST_NATIVE_BUILD.md).

## Repository safety

This is a redistributable source repository only. It must never contain WADs,
DOLs, tickets, TMDs, NAND or extracted filesystems, Nintendo-owned media, or
generated game translation units. Legally obtained game inputs belong only in
ignored local directories.

Before contributing, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Check-RepositoryPolicy.ps1 -RepositoryRoot .
```

The checker examines tracked paths and rejects known title-data extensions,
title-data filenames, and protected directory roots. `.gitignore` prevents
these materials and normal build artifacts from being added accidentally.

## Windows builds

The asset-free configuration needs no game data or C++ compiler:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-NativeBuild.ps1 -ConfigureOnly -SkipGame
```

For a local native build, place a legally obtained, verified `main.dol` only
at `local/WPSE01_01/extracted/main.dol`, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-NativeBuild.ps1
```

The script verifies the fixed WPSE01 DOL SHA-1 and entry point before it builds
or runs the recompiler. A successful game build writes the executable exactly
to `build/windows-release/PokemonRumble.exe`; `local/`, `generated/`, and
`build/` remain ignored.

## Licensing and notices

The original material in this repository is licensed under the MIT License;
see [LICENSE](LICENSE). Future NWiiRecomp imports retain their upstream
attribution and modified noncommercial license, so no combined distributable
is offered for commercial use. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
