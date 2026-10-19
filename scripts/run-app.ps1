# Launches the application (visible window) after building when missing.
param(
    [string]$Exe = $env:IST_APP_EXE ?? (Join-Path (Split-Path $PSScriptRoot -Parent) 'app\build-mingw\IranSignalTracker.exe'),
    [switch]$NoBuild
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Exe) -and -not $NoBuild) { & (Join-Path $PSScriptRoot 'build.ps1') }
if (-not (Test-Path $Exe)) { throw "application not found: $Exe (build first: scripts\build.ps1)" }
Clear-Host
Write-Host "Launching $Exe"
Start-Process $Exe
