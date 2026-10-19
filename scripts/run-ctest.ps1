# Runs the C++ unit/smoke tests (ctest: app_smoke, app_dbtest).
param(
    [string]$Bd = $env:IST_APP_BUILD ?? 'app\build-mingw'
)
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$cmakeBin = 'C:\Qt\Tools\CMake_64\bin'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.8.3\mingw_64\bin;$cmakeBin;$env:PATH"
& (Join-Path $cmakeBin 'ctest.exe') --test-dir $Bd --output-on-failure
exit $LASTEXITCODE
