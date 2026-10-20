# Serves the repo root over HTTP so Allure reports load (file:// breaks
# Allure 3's ES modules) and all runs stay browsable side by side under
# /reports/runs/<run>/allure-report/. Reuses a server started earlier (port
# remembered in reports\.server-port, plus common ports) when it answers for
# us; otherwise picks a FREE port (OS-assigned) and starts python's
# http.server in a hidden background process.
# -Open <relative/path.html> additionally opens that URL in the default browser.
param(
    [string]$Open = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$portFile = Join-Path $repo 'reports\.server-port'

function Test-Serving([int]$Port) {
    # README.md must come back 200: that identifies a server actually serving
    # THIS repo root (python's http.server does; random other services won't).
    try {
        return (Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$Port/README.md" -TimeoutSec 2).StatusCode -eq 200
    } catch { return $false }
}

# Reuse pass: remembered port first, then well-known candidates.
$ports = @(2222, 8000, 8080)
if (Test-Path $portFile) {
    $remembered = (Get-Content $portFile | Select-Object -First 1)
    if ($remembered -match '^\d+$') { $ports = @([int]$remembered) + $ports }
}
$port = 0
foreach ($p in $ports) {
    if (Test-Serving $p) { $port = $p; break }
}

if (-not $port) {
    # Resolve python: pinned user-scope install wins; PATH next (skipping the
    # Microsoft Store alias stub) - same pattern as run-tests.ps1.
    $python = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
    if (-not (Test-Path $python)) {
        $python = (Get-Command python -ErrorAction SilentlyContinue).Source
        if ($python -and $python -like '*WindowsApps*') { $python = $null }
    }
    if (-not $python) { throw 'python not found (see docs/env-setup.md)' }
    # A free port: ask the OS for one, then hand it to http.server (the tiny
    # race between Stop() and python's bind is acceptable for a dev machine).
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
    $listener.Start()
    $port = $listener.LocalEndpoint.Port
    $listener.Stop()
    Start-Process -WindowStyle Hidden $python -ArgumentList '-m', 'http.server', "$port", `
        '--bind', '127.0.0.1', '--directory', "`"$repo`""
    $ready = $false
    foreach ($i in 1..30) {
        Start-Sleep -Seconds 1
        if (Test-Serving $port) { $ready = $true; break }
    }
    if (-not $ready) { throw "could not serve $repo on port $port" }
    Set-Content -Path $portFile -Value $port -Encoding ascii
    Write-Host "Serving $repo at http://127.0.0.1:$port (all reports under /reports/runs/)"
}
if ($Open) { Start-Process "http://127.0.0.1:$port/$($Open.TrimStart('/'))" }