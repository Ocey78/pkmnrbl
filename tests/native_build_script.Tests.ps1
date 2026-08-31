$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ScriptPath = Join-Path $PSScriptRoot '..\tools\Invoke-NativeBuild.ps1'
$SourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

function Assert-Condition {
    param(
        [Parameter(Mandatory)] [bool] $Condition,
        [Parameter(Mandatory)] [string] $Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function New-SyntheticDol {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [uint32] $EntryPoint
    )

    $bytes = [byte[]]::new(0x104)
    $entryBytes = [BitConverter]::GetBytes($EntryPoint)
    [Array]::Reverse($entryBytes)
    [Array]::Copy($entryBytes, 0, $bytes, 0xE0, 4)
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function Invoke-NativeBuild {
    param(
        [Parameter(Mandatory)] [string] $RepositoryRoot,
        [switch] $ConfigureOnly,
        [switch] $SkipGame
    )

    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $ScriptPath,
        '-RepositoryRoot', $RepositoryRoot)
    if ($ConfigureOnly) { $arguments += '-ConfigureOnly' }
    if ($SkipGame) { $arguments += '-SkipGame' }

    $savedErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & powershell @arguments 2>&1 | Out-String
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    return [pscustomobject]@{ ExitCode = $exitCode; Output = $output }
}

if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
    throw "Native build script is missing: $ScriptPath"
}

$temporaryRoots = [System.Collections.Generic.List[string]]::new()
try {
    foreach ($case in @(
        @{ Name = 'missing DOL'; EntryPoint = $null; Expected = 'Required DOL is missing' },
        @{ Name = 'wrong entry point'; EntryPoint = [Convert]::ToUInt32('80004054', 16); Expected = 'Unexpected DOL entry point' },
        @{ Name = 'wrong SHA-1'; EntryPoint = [Convert]::ToUInt32('80004050', 16); Expected = 'Unexpected DOL SHA-1' }
    )) {
        $temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("pkmnrbl-native-build-" + [guid]::NewGuid().ToString('N'))
        $temporaryRoots.Add($temporaryRoot)
        $localDirectory = Join-Path $temporaryRoot 'local\WPSE01_01\extracted'
        New-Item -ItemType Directory -Force -Path $localDirectory | Out-Null

        if ($null -ne $case.EntryPoint) {
            New-SyntheticDol -Path (Join-Path $localDirectory 'main.dol') -EntryPoint $case.EntryPoint
        }

        $result = Invoke-NativeBuild -RepositoryRoot $temporaryRoot
        Assert-Condition -Condition ($result.ExitCode -ne 0) -Message "$($case.Name): expected validation failure before any tool build."
        Assert-Condition -Condition ($result.Output -match [regex]::Escape($case.Expected)) -Message "$($case.Name): expected '$($case.Expected)', got:$([Environment]::NewLine)$($result.Output)"
        Assert-Condition -Condition (-not (Test-Path -LiteralPath (Join-Path $temporaryRoot 'generated'))) -Message "$($case.Name): validation reached generation before rejecting the DOL."
        Write-Host "PASS: $($case.Name) is rejected before host tooling runs."
    }

    $configureResult = Invoke-NativeBuild -RepositoryRoot $SourceRoot -ConfigureOnly -SkipGame
    Assert-Condition -Condition ($configureResult.ExitCode -eq 0) -Message "asset-free configure failed:$([Environment]::NewLine)$($configureResult.Output)"
    Write-Host 'PASS: -ConfigureOnly -SkipGame configures the compiler-independent asset-free preset.'
}
finally {
    foreach ($temporaryRoot in $temporaryRoots) {
        if (Test-Path -LiteralPath $temporaryRoot) {
            Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
        }
    }
}
