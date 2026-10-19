# Builds the Qt app (configure + compile) and deploys the Qt runtime so the
# exe runs without a developer PATH (required for WinAppDriver/Appium runs).
# Prereqs per docs/env-setup.md. Compatible with Windows PowerShell 5.1+.
param(
    [string]$QtBin  = 'C:\Qt\6.8.3\mingw_64\bin',
    [string]$Cmake  = 'C:\Qt\Tools\CMake_64\bin\cmake.exe',
    [string]$Gcc    = 'C:\Qt\Tools\mingw1310_64\bin\gcc.exe',
    [string]$Gxx    = 'C:\Qt\Tools\mingw1310_64\bin\g++.exe',
    [string]$Ninja  = 'C:\Qt\Tools\Ninja\ninja.exe',
    [string]$Bd     = 'app\build-mingw'
)
if ($env:IST_QT_BIN)     { $QtBin  = $env:IST_QT_BIN }
if ($env:IST_CMAKE)      { $Cmake  = $env:IST_CMAKE }
if ($env:IST_GCC)        { $Gcc    = $env:IST_GCC }
if ($env:IST_GXX)        { $Gxx    = $env:IST_GXX }
if ($env:IST_NINJA)      { $Ninja  = $env:IST_NINJA }
if ($env:IST_APP_BUILD)  { $Bd     = $env:IST_APP_BUILD }

$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

# MinGW + Qt DLL paths must be on PATH for configure-time try-compile.
$mingwBin = Split-Path $Gxx -Parent
$env:PATH = "$mingwBin;$QtBin;$env:PATH"

& $Cmake -S app -B $Bd -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$Ninja" `
    "-DCMAKE_PREFIX_PATH=$(Split-Path $QtBin -Parent)" `
    "-DCMAKE_C_COMPILER=$Gcc" `
    "-DCMAKE_CXX_COMPILER=$Gxx"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

& $Cmake --build $Bd
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

# Kill leftover instances, then deploy the Qt runtime next to the exe.
Get-Process IranSignalTracker -ErrorAction SilentlyContinue | Stop-Process -Force
& (Join-Path $QtBin 'windeployqt.exe') --compiler-runtime (Join-Path $Bd 'IranSignalTracker.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }
# app_smoke links Qt6::Test, which windeployqt only sees for the GUI exe.
$dest = Join-Path $Bd 'Qt6Test.dll'
if (-not (Test-Path $dest)) { Copy-Item (Join-Path $QtBin 'Qt6Test.dll') $dest }

Write-Host "Build OK: $(Resolve-Path (Join-Path $Bd 'IranSignalTracker.exe'))"
