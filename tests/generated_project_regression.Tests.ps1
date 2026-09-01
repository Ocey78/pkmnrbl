[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $RecompilerPath,

    [string] $RepositoryRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..'
}

function Assert-Condition {
    param([Parameter(Mandatory)] [bool] $Condition, [Parameter(Mandatory)] [string] $Message)
    if (-not $Condition) { throw $Message }
}

function Write-UInt32BigEndian {
    param([Parameter(Mandatory)] [byte[]] $Bytes, [Parameter(Mandatory)] [int] $Offset, [Parameter(Mandatory)] [uint32] $Value)
    $encoded = [BitConverter]::GetBytes($Value)
    [Array]::Reverse($encoded)
    [Array]::Copy($encoded, 0, $Bytes, $Offset, 4)
}

$resolvedRepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$resolvedRecompilerPath = (Resolve-Path -LiteralPath $RecompilerPath).Path
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("pkmnrbl-generated-project-" + [guid]::NewGuid().ToString('N'))

try {
    $inputDirectory = Join-Path $temporaryRoot 'input'
    $outputDirectory = Join-Path $temporaryRoot 'export'
    New-Item -ItemType Directory -Force -Path $inputDirectory | Out-Null

    # One text section containing a PowerPC `blr`, with the WPSE01 entry point.
    $dol = [byte[]]::new(0x104)
    Write-UInt32BigEndian -Bytes $dol -Offset 0x00 -Value 0x100
    Write-UInt32BigEndian -Bytes $dol -Offset 0x48 -Value ([Convert]::ToUInt32('80004050', 16))
    Write-UInt32BigEndian -Bytes $dol -Offset 0x90 -Value 4
    Write-UInt32BigEndian -Bytes $dol -Offset 0xE0 -Value ([Convert]::ToUInt32('80004050', 16))
    Write-UInt32BigEndian -Bytes $dol -Offset 0x100 -Value 0x4E800020
    [System.IO.File]::WriteAllBytes((Join-Path $inputDirectory 'main.dol'), $dol)

    $tomlPath = Join-Path $temporaryRoot 'synthetic-recomp.toml'
    $runtimeSource = (Join-Path $resolvedRepositoryRoot 'third_party\NWiiRecomp\nWiiRuntime').Replace('\', '/')
    $bootSource = (Join-Path $resolvedRepositoryRoot 'runtime\boot').Replace('\', '/')
    $inputTomlPath = $inputDirectory.Replace('\', '/')
    $outputTomlPath = $outputDirectory.Replace('\', '/')
    @(
        'project_name = "SyntheticDOL"',
        "input_game_dir = `"$inputTomlPath`"",
        "output_dir = `"$outputTomlPath`"",
        "runtime_source_dir = `"$runtimeSource`"",
        "runtime_boot_source_dir = `"$bootSource`"",
        'split_output = true',
        'instructions_per_file = 1'
    ) | Set-Content -LiteralPath $tomlPath -Encoding utf8

    $output = & $resolvedRecompilerPath $tomlPath 2>&1 | Out-String
    Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "Synthetic DOL recompilation failed:$([Environment]::NewLine)$output"

    $generatedCMake = Join-Path $outputDirectory 'CMakeLists.txt'
    $generatedRuntime = Join-Path $outputDirectory 'nWiiRuntime'
    $generatedRuntimeCMake = Join-Path $generatedRuntime 'CMakeLists.txt'
    $generatedDispatcher = Join-Path $outputDirectory 'main_output.cpp'
    Assert-Condition -Condition (Test-Path -LiteralPath $generatedCMake -PathType Leaf) -Message 'The synthetic export did not create CMakeLists.txt.'
    Assert-Condition -Condition (Test-Path -LiteralPath (Join-Path $generatedRuntime 'runtime\boot\wii_memory_layout.cpp') -PathType Leaf) -Message 'The synthetic export did not copy the boot runtime sources.'
    Assert-Condition -Condition (Test-Path -LiteralPath (Join-Path $generatedRuntime 'runtime\boot\nwii_guest_memory.h') -PathType Leaf) -Message 'The synthetic export did not copy the boot runtime headers.'

    $cmakeContent = Get-Content -LiteralPath $generatedCMake -Raw
    $runtimeCmakeContent = Get-Content -LiteralPath $generatedRuntimeCMake -Raw
    $dispatcherContent = Get-Content -LiteralPath $generatedDispatcher -Raw
    Assert-Condition -Condition ($cmakeContent -match 'OUTPUT_NAME\s+"PokemonRumble"') -Message 'Generated target lacks OUTPUT_NAME PokemonRumble.'
    Assert-Condition -Condition ($cmakeContent -match '/bigobj') -Message 'Generated target lacks MSVC /bigobj.'
    Assert-Condition -Condition ($cmakeContent -match 'PKMNRBL_ENABLE_BRINGUP_INTERPRETER') -Message 'Generated target does not expose the temporary PPC fallback explicitly.'
    Assert-Condition -Condition ($cmakeContent -match 'nWiiRuntime/src/hle/interpreter\.cpp') -Message 'Generated target does not link its currently required bring-up fallback.'
    Assert-Condition -Condition ($cmakeContent -match 'SDL_MAIN_HANDLED') -Message 'Generated target does not define an unambiguous native Windows entry point.'
    Assert-Condition -Condition ($cmakeContent -match 'RUNTIME_OUTPUT_DIRECTORY') -Message 'Generated target lacks an explicit runtime output directory.'
    Assert-Condition -Condition ($runtimeCmakeContent -match 'runtime/boot/wii_memory_layout\.cpp') -Message 'Generated runtime CMake does not consume its copied boot source.'
    Assert-Condition -Condition ($runtimeCmakeContent -match 'target_include_directories\(nwiiruntime PUBLIC "\$\{CMAKE_CURRENT_SOURCE_DIR\}"\)') -Message 'Generated runtime CMake does not include its self-contained boot headers.'
    Assert-Condition -Condition ($runtimeCmakeContent -notmatch 'src/hle/interpreter\.cpp') -Message 'Generated runtime includes the default PPC interpreter.'
    Assert-Condition -Condition ($runtimeCmakeContent -match 'target_link_libraries\(nwiiruntime PUBLIC ws2_32\)') -Message 'Generated runtime does not link Winsock on Windows.'
    Assert-Condition -Condition ($dispatcherContent -match '(?<!_)setjmp\(ctx\.exception_jmp_buf\)') -Message 'Generated dispatcher does not use portable setjmp.'
    Assert-Condition -Condition ($dispatcherContent -notmatch '_setjmp\(') -Message 'Generated dispatcher uses the MSVC-specific one-argument _setjmp form.'

    Write-Host 'PASS: synthetic DOL export produces a standalone Windows-ready project without the default interpreter.'
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
