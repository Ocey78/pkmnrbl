$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Find-GitExecutable {
    $gitCommand = Get-Command git.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($gitCommand) {
        return $gitCommand.Source
    }

    $programFilesRoots = @(
        $env:ProgramFiles,
        [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique

    foreach ($programFilesRoot in $programFilesRoots) {
        $visualStudioRoot = Join-Path $programFilesRoot 'Microsoft Visual Studio'
        if (-not (Test-Path -LiteralPath $visualStudioRoot)) {
            continue
        }

        foreach ($yearDirectory in Get-ChildItem -LiteralPath $visualStudioRoot -Directory) {
            foreach ($editionDirectory in Get-ChildItem -LiteralPath $yearDirectory.FullName -Directory) {
                $candidate = Join-Path $editionDirectory.FullName 'Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe'
                if (Test-Path -LiteralPath $candidate) {
                    return $candidate
                }
            }
        }
    }

    return $null
}

$CheckerPath = Join-Path $PSScriptRoot '..\tools\Check-RepositoryPolicy.ps1'
$GitPath = Find-GitExecutable

if (-not (Test-Path -LiteralPath $CheckerPath)) {
    throw "Repository policy checker is missing: $CheckerPath"
}

if (-not $GitPath) {
    throw 'Git executable was not found. Add git.exe to PATH or install Visual Studio with Git support.'
}

function New-TemporaryRepository {
    $repositoryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("pkmnrbl-policy-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $repositoryRoot | Out-Null

    & $GitPath -C $repositoryRoot init -q
    if ($LASTEXITCODE -ne 0) {
        throw "Could not initialize temporary repository: $repositoryRoot"
    }

    return $repositoryRoot
}

function Add-TrackedFile {
    param(
        [Parameter(Mandatory)] [string] $RepositoryRoot,
        [Parameter(Mandatory)] [string] $RelativePath
    )

    $absolutePath = Join-Path $RepositoryRoot ($RelativePath -replace '/', '\\')
    $parentPath = Split-Path -Parent $absolutePath
    New-Item -ItemType Directory -Force -Path $parentPath | Out-Null
    Set-Content -LiteralPath $absolutePath -Value 'fixture' -NoNewline

    & $GitPath -C $RepositoryRoot add -- $RelativePath
    if ($LASTEXITCODE -ne 0) {
        throw "Could not track test file: $RelativePath"
    }
}

function Invoke-PolicyChecker {
    param([Parameter(Mandatory)] [string] $RepositoryRoot)

    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File $CheckerPath -RepositoryRoot $RepositoryRoot 2>&1 | Out-String
    return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = $output
    }
}

function Assert-PolicyResult {
    param(
        [Parameter(Mandatory)] [string] $Name,
        [Parameter(Mandatory)] [bool] $Condition,
        [Parameter(Mandatory)] [AllowEmptyString()] [string] $FailureMessage
    )

    if (-not $Condition) {
        throw "$Name failed: $FailureMessage"
    }

    Write-Host "PASS: $Name"
}

$temporaryRepositories = [System.Collections.Generic.List[string]]::new()
try {
    $safeRepository = New-TemporaryRepository
    $temporaryRepositories.Add($safeRepository)
    Add-TrackedFile -RepositoryRoot $safeRepository -RelativePath 'README.md'
    $safeResult = Invoke-PolicyChecker -RepositoryRoot $safeRepository
    Assert-PolicyResult -Name 'harmless README.md passes' -Condition ($safeResult.ExitCode -eq 0) -FailureMessage $safeResult.Output

    foreach ($forbiddenPath in @(
        'game.wad',
        'main.dol',
        'ticket.bin',
        'nand/title/00000001/00000002/content/title.tmd',
        'generated/ppc/func_80004050.cpp',
        'GAME.WAD',
        'TICKET.BIN',
        'NAND/TITLE/00000001/00000002/content/TITLE.TMD',
        'NAND/TITLE/00000001/00000002/content/payload.cpp'
    )) {
        $repositoryRoot = New-TemporaryRepository
        $temporaryRepositories.Add($repositoryRoot)
        Add-TrackedFile -RepositoryRoot $repositoryRoot -RelativePath $forbiddenPath
        $result = Invoke-PolicyChecker -RepositoryRoot $repositoryRoot

        Assert-PolicyResult -Name "$forbiddenPath is rejected" -Condition ($result.ExitCode -ne 0) -FailureMessage 'the checker returned success'
        Assert-PolicyResult -Name "$forbiddenPath is reported" -Condition ($result.Output -match [regex]::Escape($forbiddenPath)) -FailureMessage $result.Output
    }
}
finally {
    foreach ($repositoryRoot in $temporaryRepositories) {
        if (Test-Path -LiteralPath $repositoryRoot) {
            Remove-Item -LiteralPath $repositoryRoot -Recurse -Force
        }
    }
}
