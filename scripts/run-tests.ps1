# Runs the 10 E2E tests (TC001-TC010) via pytest and writes Allure results.
# Requires the Appium server (scripts\run-appium.ps1) or DRIVER_MODE=direct.
param(
    [string]$Results = (Join-Path (Split-Path $PSScriptRoot -Parent) 'reports\allure-results'),
    [string]$Tests = 'tests',  # pytest path or ';'-separated file list
    [string]$K = ''            # optional -k filter
)
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\automation')

# Resolve python: pinned user-scope install wins; PATH next (skipping the
# Microsoft Store alias stub that only prints "Python was not found").
$python = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
if (-not (Test-Path $python)) {
    $python = (Get-Command python -ErrorAction SilentlyContinue).Source
    if ($python -and $python -like '*WindowsApps*') { $python = $null }
}
if (-not $python) { throw 'python not found (see docs/env-setup.md)' }

if (-not (Test-Path (Join-Path (Split-Path $PSScriptRoot -Parent) 'app\build-mingw\IranSignalTracker.exe'))) {
    & (Join-Path $PSScriptRoot 'build.ps1')
}
$pytestArgs = @('-m','pytest') + ($Tests -split ';') + @('-v', "--alluredir=$Results")
if ($K) { $pytestArgs += @('-k', $K) }
& $python @pytestArgs
exit $LASTEXITCODE
