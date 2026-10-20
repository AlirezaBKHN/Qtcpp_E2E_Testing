# Runs tests via pytest into a PER-RUN Allure folder (reports\runs\<ts>_<branch>_<commit>\),
# then generates the report and opens it in the default browser.
# Choose a suite from automation\suites.json interactively, with -Suite <name> (or 'all'),
# or pass an ad-hoc -Tests list. Pre-flight: builds the app if the exe is missing and
# starts Appium if :4723 is not answering (skipped with DRIVER_MODE=direct).
param(
    [string]$Suite = '',  # suite name from automation\suites.json, or 'all' (wins over -Tests)
    [string]$Tests = '',  # ad-hoc pytest path or ';'-separated file list (escape hatch)
    [string]$K = ''       # optional -k filter
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
$runName = if ($Suite) { "{0}_{1}_{2}_{3}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $branch, $hash5, $Suite } else { '{0}_{1}_{2}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $branch, $hash5 }

$runDir = Join-Path $repo "reports\runs\$runName"
$suffix = 2
while (Test-Path $runDir) {
    if ($Suite -or $Tests) { throw "run folder already exists: $runDir" }
    $runDir = Join-Path $repo "reports\runs\${runName}-${suffix}"
    $suffix++
}
$results = Join-Path $runDir 'allure-results'
New-Item -ItemType Directory -Force -Path $results | Out-Null
Write-Host "Run folder: $runDir"

# --- resolve what to run -------------------------------------------------------
$suitesPath = Join-Path $repo 'automation\suites.json'
if (-not (Test-Path $suitesPath)) { throw "suites file not found: $suitesPath" }
$suites = Get-Content $suitesPath -Raw | ConvertFrom-Json
$suiteNames = @($suites.PSObject.Properties.Name)

$selected = $null
if ($Suite) {
    if ($Suite -ne 'all' -and $suiteNames -notcontains $Suite) {
        throw "unknown suite '$Suite' - available: $($suiteNames -join ', '), all"
    }
    $selected = $Suite
} elseif (-not $Tests) {
    Write-Host ''
    Write-Host 'Test suites (automation\suites.json):'
    for ($i = 0; $i -lt $suiteNames.Count; $i++) {
        $count = @($suites.PSObject.Properties[$suiteNames[$i]].Value).Count
        Write-Host ("  [{0}] {1,-10} ({2} test file(s))" -f ($i + 1), $suiteNames[$i], $count)
    }
    Write-Host '  [a] all - every test (unit + E2E)'
    $answer = Read-Host 'Choose'
    if ($answer -match '^[aA]$') {
        $selected = 'all'
    } elseif ($answer -match '^\d+$' -and [int]$answer -ge 1 -and [int]$answer -le $suiteNames.Count) {
        $selected = $suiteNames[[int]$answer - 1]
    } else {
        throw "invalid choice: '$answer'"
    }
    Write-Host ''
}

if ($Suite -or $selected) {
    if ($selected -eq 'all') {
        $targets = @('tests')
    } else {
        # Entries are test-file names from the automation\tests root (a
        # subdirectory prefix comes along when nested); '.py' is optional.
        $targets = @($suites.PSObject.Properties[$selected].Value) | ForEach-Object {
            $t = $_.Trim()
            if (-not $t.EndsWith('.py')) { $t = "$t.py" }
            Join-Path 'tests' $t
        }
        foreach ($t in $targets) {
            if (-not (Test-Path (Join-Path $repo "automation\$t"))) {
                throw "suite '$selected' lists a missing test file: $t"
            }
        }
    }
} else {
    $targets = $Tests -split ';'
}

# --- pre-flight: the app must be built ----------------------------------------
if (-not (Test-Path (Join-Path $repo 'app\build-mingw\IranSignalTracker.exe'))) {
    & (Join-Path $PSScriptRoot 'build.ps1')
}

# --- pre-flight: the driver must be up ----------------------------------------
if ($env:DRIVER_MODE -ne 'direct') {
    $statusUri = 'http://127.0.0.1:4723/status'
    $up = $false
    try { $up = (Invoke-WebRequest -UseBasicParsing -Uri $statusUri -TimeoutSec 2).StatusCode -eq 200 } catch { $up = $false }
    if (-not $up) {
        Write-Host 'Appium not answering on :4723 - starting it ...'
        Start-Process pwsh -ArgumentList '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'run-appium.ps1')
        $ready = $false
        foreach ($i in 1..30) {
            Start-Sleep -Seconds 2
            try { $ready = (Invoke-WebRequest -UseBasicParsing -Uri $statusUri -TimeoutSec 2).StatusCode -eq 200 } catch { $ready = $false }
            if ($ready) { break }
        }
        if (-not $ready) { throw 'Appium did not become ready on :4723 within 60s' }
        Write-Host 'Appium is up.'
    }
}

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

$pytestArgs = @('-m','pytest') + $targets + @('-v', "--alluredir=$results")
if ($K) { $pytestArgs += @('-k', $K) }
& $python @pytestArgs
$pytestExit = $LASTEXITCODE

# Report for THIS run (never an older report-less one) + open in the browser.
& (Join-Path $PSScriptRoot 'run-allure.ps1') -Run (Split-Path $runDir -Leaf)
$index = Join-Path $runDir 'allure-report\index.html'
if (Test-Path $index) { Start-Process $index }

exit $pytestExit