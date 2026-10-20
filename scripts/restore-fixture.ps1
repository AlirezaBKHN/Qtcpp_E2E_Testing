# Restores a fixture over the disposable working db and migrates it.
# No arg = the map's default (database/fixtures.json).
param(
    [string]$Fixture = ''
)
$ErrorActionPreference = 'Stop'
$python = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
if (-not (Test-Path $python)) {
    $python = (Get-Command python -ErrorAction SilentlyContinue).Source
    if ($python -and $python -like '*WindowsApps*') { $python = $null }
}
if (-not $python) { throw 'python not found (see docs/env-setup.md)' }
$cli = Join-Path (Split-Path $PSScriptRoot -Parent) 'automation\fixtures.py'
if ($Fixture) { & $python $cli $Fixture } else { & $python $cli }
exit $LASTEXITCODE
