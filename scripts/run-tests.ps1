# Runs the E2E tests (TC001-TC018) via pytest into a PER-RUN Allure folder:
#   reports\runs\<YYYYMMDD-HHMMSS>_<branch>_<5-of-commit>\allure-results
# so two runs (different branches / app versions) never overwrite each other.
# Requires the Appium server (scripts\run-appium.ps1) or DRIVER_MODE=direct.
param(
    [string]$RunName = '',     # override the run folder name (rare)
    [string]$Tests = 'tests',  # pytest path or ';'-separated file list
    [string]$K = ''            # optional -k filter
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent

# Run identity: timestamp + branch (or tag when detached) + first 5 of the
# commit hash, e.g. 20261020-201522_main_5cffd. 'nogit' when git is absent.
$branch = 'nogit'
$commit = ''
try {
    $commit = git -C $repo rev-parse HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $commit) {
        $branch = git -C $repo rev-parse --abbrev-ref HEAD 2>$null
        if (-not $branch -or $branch -eq 'HEAD') {
            $tag = git -C $repo describe --tags 2>$null
            $branch = if ($tag) { $tag } else { 'nogit' }
        }
        $branch = ($branch -replace '[/\\:*?\"<>|]', '-').Trim()
        if (-not $branch) { $branch = 'nogit' }
    } else {
        $commit = ''
    }
} catch {
    $branch = 'nogit'
    $commit = ''
}
$hash5 = if ($commit) { $commit.Substring(0, [Math]::Min(5, $commit.Length)) } else { 'nogit' }
$runName = if ($RunName) { $RunName } else { '{0}_{1}_{2}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $branch, $hash5 }

$runDir = Join-Path $repo "reports\runs\$runName"
$suffix = 2
while (Test-Path $runDir) {
    if ($RunName) { throw "run folder already exists: $runDir" }
    $runDir = Join-Path $repo "reports\runs\${runName}-${suffix}"
    $suffix++
}
$results = Join-Path $runDir 'allure-results'
New-Item -ItemType Directory -Force -Path $results | Out-Null
Write-Host "Run folder: $runDir"

# Fixtures are the only source of truth: materialize the working db if absent.
if (-not (Test-Path (Join-Path $repo 'database\signaltracker.db'))) {
    & (Join-Path $PSScriptRoot 'restore-fixture.ps1')
}
Set-Location (Join-Path $PSScriptRoot '..\automation')

# Resolve python: pinned user-scope install wins; PATH next (skipping the
# Microsoft Store alias stub that only prints "Python was not found").
$python = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
if (-not (Test-Path $python)) {
    $python = (Get-Command python -ErrorAction SilentlyContinue).Source
    if ($python -and $python -like '*WindowsApps*') { $python = $null }
}
if (-not $python) { throw 'python not found (see docs/env-setup.md)' }

if (-not (Test-Path (Join-Path $repo 'app\build-mingw\IranSignalTracker.exe'))) {
    & (Join-Path $PSScriptRoot 'build.ps1')
}
$pytestArgs = @('-m','pytest') + ($Tests -split ';') + @('-v', "--alluredir=$results")
if ($K) { $pytestArgs += @('-k', $K) }
& $python @pytestArgs
exit $LASTEXITCODE