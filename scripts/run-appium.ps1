# Starts the Appium server (port 4723) with the windows driver in a new console.
# Keep it running while pytest executes; Ctrl+C in that window stops it.
param(
    [int]$Port = 4723
)
$ErrorActionPreference = 'Stop'
$exe = (Get-Command appium -ErrorAction SilentlyContinue).Source
if (-not $exe) { throw "appium not on PATH (npm i -g appium + appium driver install windows) — see docs/env-setup.md" }
Start-Process -WindowStyle Normal powershell -ArgumentList '-NoExit','-Command',
    "appium --port $Port; Read-Host 'Enter to close'"
Start-Sleep -Seconds 5
Write-Host "Appium starting on http://127.0.0.1:$Port (check the new console window)"
