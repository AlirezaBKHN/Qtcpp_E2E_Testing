# Qtcpp_E2E_Testing — Iran Signal Tracker + Appium E2E Demo

A standalone **Qt 6 Widgets** desktop application ("Iran Signal Tracker", SQLite-backed, mock
telecom data) and a complete **Appium + WinAppDriver E2E automation stack** in front of it:
a human-readable DSL, page objects, 10 pytest tests, and an Allure report.

The E2E tests never touch driver APIs — they read like the domain:

```python
tracker.province("Tehran").city("Tehran").technology("5G").search()
tracker.table().should_have_rows()
tracker.chart().should_be_visible()
```

## Repository layout

```text
app/            Qt app (CMake, Widgets + Charts + Sql), smoke/db ctest targets
database/       committed signaltracker.db (1500 deterministic rows)
automation/     pytest framework: config, locators, driver factory, DSL, page objects, tests/TC001-010
docs/           test-cases/TC001-TC010.md, env-setup.md, superpowers/plans/
scripts/        build / run / test / report PowerShell helpers (below)
reports/        allure-results/ + allure-report/ (generated), export CSVs land in reports/export/
ci/README.md    future CI/CD flow (documentation only)
```

## Prerequisites

Pinned versions and the exact install walkthrough live in **[docs/env-setup.md](docs/env-setup.md)**. Summary:

| Component | Version | Notes |
|---|---|---|
| Windows 10/11 | x64 | **Developer Mode enabled** (WinAppDriver requirement) |
| Python | 3.12.x | + `pip install -r automation/requirements.txt` (pytest, allure-pytest, appium-python-client) |
| Node / npm | 20+ | + `npm i -g appium` + `appium driver install windows` |
| Qt | **6.8.3 win64_mingw** | via [aqtinstall](https://aqtproject.org/): `aqt install-qt windows desktop 6.8.3 win64_mingw -m qtcharts -O C:\Qt` |
| MinGW | 13.1.0 | `aqt install-tool windows desktop tools_mingw1310` |
| CMake / Ninja | 3.30.5 / latest | `aqt install-tool windows desktop tools_cmake` / `tools_ninja` → `C:\Qt\Tools` |
| WinAppDriver | 1.2.1 | MSI from [GitHub releases](https://github.com/microsoft/WinAppDriver/releases) |
| Allure CLI | 3.x | `npm i -g allure` (no Java needed) |

Appium house fix (drive peer resolution): after installing the driver, run
`New-Item -ItemType Junction -Path "$env:USERPROFILE\.appium\node_modules\appium" -Target "$env:APPDATA\npm\node_modules\appium"`
if Appium logs `Cannot find package 'appium'` on session start (see env-setup.md).

## Quick start (PowerShell, from the repo root)

```powershell
# 0) one-time: python deps
python -m pip install -r automation\requirements.txt

# 1) build the app + deploy Qt runtime next to the exe   -> app\build-mingw\IranSignalTracker.exe
powershell -ExecutionPolicy Bypass -File scripts\build.ps1

# 2) C++ test level (smoke + db_test)
powershell -ExecutionPolicy Bypass -File scripts\run-ctest.ps1

# 3) run the app by hand
powershell -ExecutionPolicy Bypass -File scripts\run-app.ps1

# 4) start Appium (new console window, keep it open) OR go direct:
powershell -ExecutionPolicy Bypass -File scripts\run-appium.ps1     # server on :4723
$env:DRIVER_MODE = 'direct'    # fallback: talk to WinAppDriver on :4729 without Appium

# 5) the 10 E2E tests (TC001-TC010)
powershell -ExecutionPolicy Bypass -File scripts\run-tests.ps1

# 6) Allure report
powershell -ExecutionPolicy Bypass -File scripts\run-allure.ps1 -Serve
```

Equivalent manual commands (what the scripts wrap):

```powershell
cmake -S app -B app\build-mingw -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe `
    -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\mingw_64 `
    -DCMAKE_C_COMPILER=C:\Qt\Tools\mingw1310_64\bin\gcc.exe `
    -DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe
cmake --build app\build-mingw
C:\Qt\6.8.3\mingw_64\bin\windeployqt.exe --compiler-runtime app\build-mingw\IranSignalTracker.exe
ctest --test-dir app\build-mingw
WinAppDriver.exe          # direct mode; launch `appium --port 4723` for the Appium path
cd automation; python -m pytest tests -v --alluredir=..\reports\allure-results
allure generate ..\reports\allure-results -o ..\reports\allure-report --clean
```

## What the app does

Main window with Province / City / Technology / Operator filters (each has an `All` entry; City
cascades by Province), Search / Refresh / Reset / Export CSV buttons, a results table
(ID, Province, City, Operator, Technology, Signal Strength, Timestamp), and a bar chart of
average signal grouped by operator or technology. Export writes the current view to
`reports/export/export_<timestamp>.csv` and shows the path in the status bar, which also
always states `Showing N rows`.

- Database: SQLite `signaltracker.db`, created + **seeded deterministically (fixed RNG seed,
  1500 rows, -120..-50 dBm)** on first launch if the table is missing; the seeded file is also
  committed under `database/`.
- Every interactive control carries a **stable identifier** (`provinceCombo`, `searchButton`,
  `signalTable`, `signalChart`, …) set on both `objectName` and `accessibleName` — that's what
  automation locates (UIA `Name` strategy; see `automation/locators.py`).

## The 10 E2E tests

`TC001` launch · `TC002` province filter · `TC003` city filter (cascade, Fars→Shiraz) ·
`TC004` technology filter · `TC005` operator filter · `TC006` combined filters ·
`TC007` reset · `TC008` refresh row-count equality · `TC009` export CSV · `TC010` chart visibility.
Catalog with preconditions/steps/expected results: `docs/test-cases/`. Failures attach a
screenshot to the Allure report.

## Troubleshooting

- `libgcc_s_seh-1.dll / libstdc++-6.dll not found` → you skipped `windeployqt --compiler-runtime`;
  run `scripts\build.ps1` (it deploys for you).
- Appium `Could not find a driver for automationName 'Windows'` → missing windows driver or the
  `appium` peer junction (see Prerequisites).
- WinAppDriver exits instantly with `0x80004005` → enable Developer Mode.
- Both Appium (`:4723`) and direct WAD (`:4729`) unreachable → `pytest` skips the suite with a
  clear message rather than failing.
- Stale windows from a crashed run are killed automatically before each session
  (`automation/drivers/factory.py`), and sessions end with `ms:forcequit`.
