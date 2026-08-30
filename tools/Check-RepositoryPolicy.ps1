[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $RepositoryRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ForbiddenExtensions = @('.wad', '.dol', '.tmd', '.tik', '.cert', '.app',
    '.iso', '.wbfs', '.wia', '.rvz', '.gcm', '.rpx', '.rpl')
$ForbiddenRoots = @('local/', 'generated/', 'nand/', 'extracted/',
    'game/', 'content/', 'title/')
$ForbiddenBasenames = @('boot.bin', 'bi2.bin', 'ticket.bin', 'title.tmd', 'uid.sys', 'setting.txt')

try {
    $resolvedRepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot -ErrorAction Stop).Path
}
catch {
    Write-Output "Repository root does not exist: $RepositoryRoot"
    exit 2
}

if (-not (Test-Path -LiteralPath (Join-Path $resolvedRepositoryRoot '.git'))) {
    Write-Output "Repository root is not a Git worktree: $resolvedRepositoryRoot"
    exit 2
}

$gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue
$gitPath = if ($gitCommand) {
    $gitCommand.Source
}
else {
    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe'
}

if (-not (Test-Path -LiteralPath $gitPath)) {
    Write-Output 'Git executable was not found. Install Git or add git.exe to PATH.'
    exit 2
}

$trackedPathOutput = & $gitPath -C $resolvedRepositoryRoot ls-files -z 2>&1
if ($LASTEXITCODE -ne 0) {
    $trackedPathOutput | Write-Output
    exit $LASTEXITCODE
}

$trackedPaths = ($trackedPathOutput -join '') -split "`0" | Where-Object { $_.Length -gt 0 }
$violations = foreach ($trackedPath in $trackedPaths) {
    $normalizedPath = $trackedPath -replace '\\', '/'
    $comparisonPath = $normalizedPath.ToLowerInvariant()
    $extension = [System.IO.Path]::GetExtension($comparisonPath)
    $basename = [System.IO.Path]::GetFileName($comparisonPath)

    if ($ForbiddenExtensions -contains $extension -or
        $ForbiddenBasenames -contains $basename -or
        ($ForbiddenRoots | Where-Object { $comparisonPath.StartsWith($_) })) {
        $normalizedPath
    }
}

if ($violations) {
    foreach ($violation in $violations) {
        Write-Output "Forbidden tracked path: $violation"
    }
    exit 1
}

Write-Host "Repository policy accepted: $resolvedRepositoryRoot"
