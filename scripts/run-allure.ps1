# Generates (and optionally serves) the Allure report for a test run folder.
# Each run lives in reports\runs\<timestamp>_<branch>_<commit>\ and its report
# is generated INTO that folder, so past runs stay viewable side by side.
param(
    [string]$Run = '',  # run folder NAME under reports\runs\ (default: newest without a report)
    [switch]$Serve
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runsDir = Join-Path $repo 'reports\runs'
if (-not (Test-Path $runsDir)) {
    throw "no runs found under $runsDir - run scripts\run-tests.ps1 first"
}
if ($Run) {
    $runDir = Join-Path $runsDir $Run
    if (-not (Test-Path $runDir)) { throw "run folder not found: $runDir" }
} else {
    # Newest run that has results but no report yet (names sort chronologically).
    $pending = Get-ChildItem $runsDir -Directory |
        Where-Object { (Test-Path (Join-Path $_.FullName 'allure-results')) -and
                       -not (Test-Path (Join-Path $_.FullName 'allure-report')) } |
        Sort-Object Name -Descending
    if (-not $pending) {
        throw "every run in $runsDir already has a report - pass -Run <name> to regenerate an older one"
    }
    $runDir = $pending[0].FullName
}
$results = Join-Path $runDir 'allure-results'
$report  = Join-Path $runDir 'allure-report'
# allure's internal staging rename (EPERM) intermittently loses a race with
# real-time AV scanning of freshly written files on Windows - retry bounded,
# removing any partial output between attempts, and fail loudly in the end.
$ok = $false
for ($attempt = 1; $attempt -le 3; $attempt++) {
    if (Test-Path $report) { Remove-Item -Recurse -Force $report }
    & allure generate "$results" -o "$report" --clean
    if ($LASTEXITCODE -eq 0) { $ok = $true; break }
    Write-Host "allure generate failed (exit $LASTEXITCODE), retry $attempt/3 ..."
    Start-Sleep -Seconds (2 * $attempt)
}
if (-not $ok) {
    throw "allure generate kept failing after 3 attempts - check the newest log under $env:USERPROFILE\.allure\logs"
}
if ($Serve) {
    & allure open "$report"
} else {
    Write-Host "Report ready: $(Join-Path $report 'index.html') (run: $(Split-Path $runDir -Leaf))"
}