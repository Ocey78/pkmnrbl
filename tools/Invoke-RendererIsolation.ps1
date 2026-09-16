[CmdletBinding()]
param(
    [string] $RepositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path,
    [ValidateRange(6, 60)] [int] $Seconds = 7,
    [ValidateRange(1, 10000)] [int] $ScreenshotInterval = 30
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$GameDirectory = Join-Path $RepositoryRoot 'local\WPSE01_01\extracted'
$ExecutableCandidates = @(
    (Join-Path $RepositoryRoot 'build\windows-release\Release\PokemonRumble.exe'),
    (Join-Path $RepositoryRoot 'build\windows-release\PokemonRumble.exe')
)
$Executable = $ExecutableCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1

if (-not $Executable) {
    throw "PokemonRumble.exe was not found. Checked: $($ExecutableCandidates -join ', ')"
}
if (-not (Test-Path -LiteralPath (Join-Path $GameDirectory 'main.dol') -PathType Leaf)) {
    throw "Pokemon Rumble game data is missing: $GameDirectory"
}

$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$OutputRoot = Join-Path $RepositoryRoot (Join-Path 'build\renderer-isolation' $runStamp)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$IsolationVariables = @(
    'NWII_FLATCOLOR',
    'NWII_NOALPHATEST',
    'NWII_NOCULL',
    'NWII_FLATZ',
    'NWII_XFB',
    'NWII_RENDER_DIAG',
    'NWII_DIAG_SECONDS',
    'NWII_SCREENSHOT',
    'NWII_SCREENSHOT_INTERVAL'
)

$savedEnvironment = @{}
foreach ($name in $IsolationVariables) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

function Clear-IsolationEnvironment {
    foreach ($name in $IsolationVariables) {
        Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
    }
}

function Set-IsolationEnvironment {
    param([Parameter(Mandatory)] [hashtable] $Values)
    foreach ($entry in $Values.GetEnumerator()) {
        Set-Item -LiteralPath "Env:$($entry.Key)" -Value ([string]$entry.Value)
    }
}

function Get-LastStatLine {
    param([Parameter(Mandatory)] [string] $LogPath)
    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    return Get-Content -LiteralPath $LogPath | Where-Object { $_ -match '^\[STAT\] ' } | Select-Object -Last 1
}

function Get-DecimalField {
    param(
        [AllowNull()] [string] $Line,
        [Parameter(Mandatory)] [string] $Name
    )
    if ($Line -and $Line -match ("(?:^|\s)" + [regex]::Escape($Name) + '=(\d+)')) {
        return [int64]$Matches[1]
    }
    return [int64]0
}

function Get-HexField {
    param(
        [AllowNull()] [string] $Line,
        [Parameter(Mandatory)] [string] $Name
    )
    if ($Line -and $Line -match ("(?:^|\s)" + [regex]::Escape($Name) + '=0x([0-9a-fA-F]+)')) {
        return [Convert]::ToUInt32($Matches[1], 16)
    }
    return [uint32]0
}

$cases = @(
    [pscustomobject]@{ Name = 'baseline';      Variables = @{} },
    [pscustomobject]@{ Name = 'flat-red';      Variables = @{ NWII_FLATCOLOR = '1' } },
    [pscustomobject]@{ Name = 'reject-bypass'; Variables = @{ NWII_FLATCOLOR = '1'; NWII_NOALPHATEST = '1'; NWII_NOCULL = '1' } },
    [pscustomobject]@{ Name = 'flat-z';         Variables = @{ NWII_FLATCOLOR = '1'; NWII_NOALPHATEST = '1'; NWII_NOCULL = '1'; NWII_FLATZ = '1' } },
    [pscustomobject]@{ Name = 'xfb';            Variables = @{ NWII_XFB = '1' } }
)

$results = [System.Collections.Generic.List[object]]::new()
$oldNativePreference = $null
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $oldNativePreference = $PSNativeCommandUseErrorActionPreference
}

try {
    foreach ($case in $cases) {
        Clear-IsolationEnvironment

        $logPath = Join-Path $OutputRoot ($case.Name + '.log')
        $screenshotPrefix = Join-Path $OutputRoot ($case.Name + '-screenshot')
        Set-IsolationEnvironment @{
            NWII_RENDER_DIAG = '1'
            NWII_DIAG_SECONDS = [string]$Seconds
            NWII_SCREENSHOT_INTERVAL = [string]$ScreenshotInterval
            NWII_SCREENSHOT = $screenshotPrefix
        }
        Set-IsolationEnvironment $case.Variables

        Write-Host "[renderer-isolation] running $($case.Name)"
        if ($PSVersionTable.PSVersion.Major -ge 7) {
            $PSNativeCommandUseErrorActionPreference = $false
        }
        & $Executable $GameDirectory *> $logPath
        $exitCode = $LASTEXITCODE

        $statLine = Get-LastStatLine -LogPath $logPath
        $shotLine = $null
        if (Test-Path -LiteralPath $logPath -PathType Leaf) {
            $shotLine = Get-Content -LiteralPath $logPath | Where-Object { $_ -match '^\[SHOT\] path=' } | Select-Object -Last 1
        }

        $results.Add([pscustomobject]@{
            Mode = $case.Name
            ExitCode = $exitCode
            Frames = Get-DecimalField -Line $statLine -Name 'frames'
            Draws = Get-DecimalField -Line $statLine -Name 'draws'
            EfbNonBlack = Get-DecimalField -Line $statLine -Name 'efb_nonblack'
            XfbNonBlack = Get-DecimalField -Line $statLine -Name 'xfb_nonblack'
            GlError = Get-HexField -Line $statLine -Name 'glerr'
            Log = $logPath
            Screenshot = if ($shotLine) { $shotLine.Substring('[SHOT] path='.Length) } else { '' }
        })
    }
}
finally {
    if ($PSVersionTable.PSVersion.Major -ge 7) {
        $PSNativeCommandUseErrorActionPreference = $oldNativePreference
    }
    Clear-IsolationEnvironment
    foreach ($name in $IsolationVariables) {
        $value = $savedEnvironment[$name]
        if ($null -ne $value) {
            Set-Item -LiteralPath "Env:$name" -Value $value
        }
    }
}

$firstVisible = $results | Where-Object { $_.EfbNonBlack -gt 0 } | Select-Object -First 1
$xfbResult = $results | Where-Object { $_.Mode -eq 'xfb' } | Select-Object -First 1

if ($firstVisible) {
    switch ($firstVisible.Mode) {
        'baseline'      { $classification = 'presentation-or-sampling' }
        'flat-red'      { $classification = 'tev-or-texture' }
        'reject-bypass' { $classification = 'alpha-or-cull' }
        'flat-z'        { $classification = 'depth-or-projection' }
        default         { $classification = 'presentation-or-sampling' }
    }
}
elseif ($xfbResult -and $xfbResult.XfbNonBlack -gt 0) {
    $classification = 'efb-copy-or-presentation'
}
else {
    $classification = 'geometry-transform-fbo'
}

$summaryPath = Join-Path $OutputRoot 'summary.txt'
$summary = [System.Collections.Generic.List[string]]::new()
$summary.Add("mode`texit`tframes`tdraws`tefb_nonblack`txfb_nonblack`tglerr`tlog`tscreenshot")
foreach ($result in $results) {
    $summary.Add(('{0}`t{1}`t{2}`t{3}`t{4}`t{5}`t0x{6:X}`t{7}`t{8}' -f
        $result.Mode, $result.ExitCode, $result.Frames, $result.Draws,
        $result.EfbNonBlack, $result.XfbNonBlack, $result.GlError,
        $result.Log, $result.Screenshot))
}
$summary.Add("classification=$classification")
$summary | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host ''
Write-Host 'Renderer isolation summary:'
$results | Format-Table Mode, ExitCode, Frames, Draws, EfbNonBlack, XfbNonBlack, @{ Label = 'GlError'; Expression = { '0x{0:X}' -f $_.GlError } } -AutoSize
Write-Host "classification=$classification"
Write-Host "summary=$summaryPath"

if (-not ($results | Where-Object { $_.Frames -gt 0 -or $_.Draws -gt 0 })) {
    Write-Error "No isolation case produced a usable [STAT] sample. Inspect logs under $OutputRoot"
    exit 1
}

exit 0
