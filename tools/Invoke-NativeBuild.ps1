[CmdletBinding()]
param(
    [string] $RepositoryRoot = (Join-Path $PSScriptRoot '..'),
    [switch] $ConfigureOnly,
    [switch] $SkipGame
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ExpectedDolSha1 = 'fd9a2c00c97e420a42355e2c27f3dc0ebbd3d8f9'
$ExpectedDolEntryPoint = [Convert]::ToUInt32('80004050', 16)
$TitleKey = 'WPSE01_01'

function Resolve-CMakeExecutable {
    if (-not [string]::IsNullOrWhiteSpace($env:CMAKE_EXE)) {
        if (-not (Test-Path -LiteralPath $env:CMAKE_EXE -PathType Leaf)) {
            throw "CMAKE_EXE does not point to a file: $env:CMAKE_EXE"
        }
        return (Resolve-Path -LiteralPath $env:CMAKE_EXE).Path
    }

    $cmakeCommand = Get-Command cmake.exe -CommandType Application -ErrorAction SilentlyContinue
    if (-not $cmakeCommand) {
        throw 'CMake was not found. Set CMAKE_EXE or add cmake.exe to PATH.'
    }
    return $cmakeCommand.Source
}

function Resolve-NinjaExecutable {
    if (-not [string]::IsNullOrWhiteSpace($env:NINJA_EXE)) {
        if (-not (Test-Path -LiteralPath $env:NINJA_EXE -PathType Leaf)) {
            throw "NINJA_EXE does not point to a file: $env:NINJA_EXE"
        }
        return (Resolve-Path -LiteralPath $env:NINJA_EXE).Path
    }

    $ninjaCommand = Get-Command ninja.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($ninjaCommand) {
        return $ninjaCommand.Source
    }
    throw 'Ninja was not found. Set NINJA_EXE or add ninja.exe to PATH.'
}

function Invoke-CMake {
    param([Parameter(Mandatory)] [string[]] $Arguments)

    Push-Location $script:ResolvedRepositoryRoot
    try {
        & $script:CMakeExecutable @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "CMake failed: $($Arguments -join ' ')"
        }
    }
    finally {
        Pop-Location
    }
}

function Read-DolEntryPoint {
    param([Parameter(Mandatory)] [string] $DolPath)

    $bytes = [System.IO.File]::ReadAllBytes($DolPath)
    if ($bytes.Length -lt 0xE4) {
        throw "DOL header is too short to contain the entry point: $DolPath"
    }

    return [uint32]((([uint64]$bytes[0xE0]) -shl 24) -bor (([uint64]$bytes[0xE1]) -shl 16) -bor (([uint64]$bytes[0xE2]) -shl 8) -bor [uint64]$bytes[0xE3])
}

$script:ResolvedRepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot -ErrorAction Stop).Path
$CMakeExecutable = Resolve-CMakeExecutable

if ($SkipGame) {
    $ninjaExecutable = Resolve-NinjaExecutable
    Invoke-CMake -Arguments @('--preset', 'windows-asset-free', "-DCMAKE_MAKE_PROGRAM=$ninjaExecutable")
    Write-Host 'Asset-free configuration completed. No title data was read.'
    exit 0
}

$localRoot = Join-Path $script:ResolvedRepositoryRoot (Join-Path 'local' $TitleKey)
$dolPath = Join-Path $localRoot 'extracted\main.dol'
if (-not (Test-Path -LiteralPath $dolPath -PathType Leaf)) {
    throw "Required DOL is missing: $dolPath"
}

$entryPoint = Read-DolEntryPoint -DolPath $dolPath
if ($entryPoint -ne $ExpectedDolEntryPoint) {
    throw ('Unexpected DOL entry point: expected 0x{0:X8}, got 0x{1:X8}.' -f $ExpectedDolEntryPoint, $entryPoint)
}

$actualSha1 = (Get-FileHash -LiteralPath $dolPath -Algorithm SHA1).Hash.ToLowerInvariant()
if ($actualSha1 -ne $ExpectedDolSha1) {
    throw "Unexpected DOL SHA-1: expected $ExpectedDolSha1, got $actualSha1."
}

Invoke-CMake -Arguments @('--preset', 'windows-msvc-release')
if ($ConfigureOnly) {
    Write-Host 'Host configuration completed after verified local title-data validation.'
    exit 0
}

Invoke-CMake -Arguments @('--build', '--preset', 'windows-msvc-release', '--target', 'nwiirecomp')
$recompilerPath = Join-Path $script:ResolvedRepositoryRoot 'build\windows-release\Release\nwiirecomp.exe'
if (-not (Test-Path -LiteralPath $recompilerPath -PathType Leaf)) {
    throw "Expected Release recompiler was not produced: $recompilerPath"
}

$configPath = Join-Path $script:ResolvedRepositoryRoot 'config\WPSE01_01\recomp.toml'
Push-Location $script:ResolvedRepositoryRoot
try {
    & $recompilerPath $configPath
    if ($LASTEXITCODE -ne 0) {
        throw 'nwiirecomp failed to generate the standalone project.'
    }
}
finally {
    Pop-Location
}

$generatedSource = Join-Path $script:ResolvedRepositoryRoot 'generated\WPSE01_01'
$generatedBuild = Join-Path $script:ResolvedRepositoryRoot 'build\windows-release\generated'
Invoke-CMake -Arguments @('-S', $generatedSource, '-B', $generatedBuild, '-G', 'Visual Studio 17 2022', '-A', 'x64')
Invoke-CMake -Arguments @('--build', $generatedBuild, '--config', 'Release')

$finalExecutable = Join-Path $script:ResolvedRepositoryRoot 'build\windows-release\PokemonRumble.exe'
if (-not (Test-Path -LiteralPath $finalExecutable -PathType Leaf)) {
    throw "Expected native executable was not produced: $finalExecutable"
}
Write-Host "Native Windows build completed: $finalExecutable"
