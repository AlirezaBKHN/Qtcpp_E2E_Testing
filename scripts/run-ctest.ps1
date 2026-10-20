# Runs the C++ unit/smoke tests (ctest: app_smoke, app_dbtest).
param(
    [string]$Bd = $env:IST_APP_BUILD ?? 'app\build-mingw'
)
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
# Fixtures are the only source of truth (spec 2026-10-20): smoke's data pins
# need the FULL dataset and have no per-test restore, so ctest always starts
# from the default fixture - an E2E run may legitimately leave the disposable
# working db in the `empty` state (no teardown restore by owner ruling).
& (Join-Path $PSScriptRoot 'restore-fixture.ps1')
$cmakeBin = 'C:\Qt\Tools\CMake_64\bin'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.8.3\mingw_64\bin;$cmakeBin;$env:PATH"
& (Join-Path $cmakeBin 'ctest.exe') --test-dir $Bd --output-on-failure
exit $LASTEXITCODE
