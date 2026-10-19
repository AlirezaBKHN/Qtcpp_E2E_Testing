# Generates (and optionally serves) the Allure report from pytest results.
param(
    [string]$Results = (Join-Path (Split-Path $PSScriptRoot -Parent) 'reports\allure-results'),
    [string]$Report  = (Join-Path (Split-Path $PSScriptRoot -Parent) 'reports\allure-report'),
    [switch]$Serve
)
$ErrorActionPreference = 'Stop'
& allure generate "$Results" -o "$Report" --clean
if ($Serve) {
    & allure open "$Report"
} else {
    Write-Host "Report ready: $Report\index.html"
}
