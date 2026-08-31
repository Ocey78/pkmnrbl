$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

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

$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$cmakeExecutable = Resolve-CMakeExecutable
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("pkmnrbl-configure-" + [guid]::NewGuid().ToString('N'))
$copiedSourceRoot = Join-Path $temporaryRoot 'source'
$buildRoot = Join-Path $temporaryRoot 'build'
$ninjaProbe = Join-Path $temporaryRoot 'ninja-probe.cmd'

try {
    New-Item -ItemType Directory -Path $copiedSourceRoot -Force | Out-Null
    Get-ChildItem -LiteralPath $sourceRoot -Force | Where-Object { $_.Name -ne '.git' } |
        Copy-Item -Destination $copiedSourceRoot -Recurse -Force
    Set-Content -LiteralPath $ninjaProbe -NoNewline -Value "@echo off`r`necho 1.11.1`r`nexit /b 0`r`n"

    $savedErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $configureOutput = & $cmakeExecutable -S $copiedSourceRoot -B $buildRoot `
            -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninjaProbe" `
            '-DPKMNRBL_BUILD_TOOLS=OFF' '-DPKMNRBL_FETCH_DEPS=OFF' `
            '-DPKMNRBL_BUILD_BOOT_TESTS=OFF' 2>&1 | Out-String
        $configureExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }

    if ($configureExitCode -ne 0) {
        throw "Asset-free CMake configuration failed:`n$configureOutput"
    }

    Write-Host 'PASS: asset-free CMake configuration completed without source assets or fetched dependencies.'
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
