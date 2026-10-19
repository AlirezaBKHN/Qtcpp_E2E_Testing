# Runs the 10 E2E tests (TC001-TC010) via pytest and writes Allure results.
# Requires the Appium server (scripts\run-appium.ps1) or DRIVER_MODE=direct.
param(
    [string]$Results = (Join-Path (Split-Path $PSScriptRoot -Parent) 'reports\allure-results'),
    [string[]]$Tests = @('tests')
)
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\automation')
if (-not (Test-Path (Join-Path (Split-Path $PSScriptRoot -Parent) 'app\build-mingw\IranSignalTracker.exe'))) {
    & (Join-Path $PSScriptRoot 'build.ps1')
}
& python -m pytest @Tests -v "--alluredir=$Results"
exit $LASTEXITCODE
