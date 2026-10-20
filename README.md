# Qtcpp_E2E_Testing — Iran Signal Tracker + Appium E2E Demo

A standalone **Qt 6 Widgets** desktop application ("Iran Signal Tracker", SQLite-backed, mock
telecom data) and a complete **Appium + WinAppDriver E2E automation stack** in front of it:
a human-readable DSL, page objects, 25 pytest tests (one intentional demo
failure, TC019), and a per-run Allure report.

The app opens on a **sign-in gate** (register / login); the main window is tabbed (Home,
Network Data, Dashboard, Reports, plus an admin-only Users tab). The E2E tests never touch
driver APIs — they read like the domain:

```python
tracker.login("e2e_user", "e2e123")
net = tracker.network()
net.province("Tehran").city("Tehran").technology("5G").search()
net.table().should_have_rows()
net.chart().should_be_visible()
```

## Repository layout

```text
app/            Qt app (CMake, Widgets + Charts + Sql), smoke/db ctest targets
database/       fixtures/ (committed .db fixtures - the data source of truth), migrations/, fixtures.json; signaltracker.db is a disposable working copy (git-ignored)
automation/     pytest framework: config, locators, driver factory, DSL, page objects, tests/TC001-TC018
docs/           test-cases/TC001-TC018.md, env-setup.md, superpowers/plans/
scripts/        build / run / test / report PowerShell helpers (below)
reports/        runs/<timestamp>_<branch>_<commit>/ - one self-contained folder per test run (allure-results/ + allure-report/); export CSVs land in reports/export/
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

# 5) the 15 E2E tests (TC001-TC015)
# 5) Tests: pick a suite interactively (automation\suites.json) or -Suite all
powershell -ExecutionPolicy Bypass -File scripts\run-tests.ps1

# 6) Allure report (newest run without one; -Run <name> for an older one)
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
cd automation; python -m pytest tests -v --alluredir=..\reports\runs\myrun\allure-results
allure generate ..\reports\runs\myrun\allure-results -o ..\reports\runs\myrun\allure-report
```

## What the app does

**Sign-in gate**: register-mode toggle, validation copy (`Wrong password.`, `Unknown username.`,
`Username already exists.`, `Password can not be empty.`, `Username can not be empty.`,
`Registered — log in.`). Seeded demo accounts (plaintext, demo-grade): `admin`/`admin`,
`guest`/`guest`, `e2e_user`/`e2e123`. Logging out returns to the gate.

**Main window** (tabbed): Home (routing cards), Network Data with Province / City /
Technology / Operator filters (each has an `All` entry; City cascades by Province), Search /
Refresh / Reset / Export CSV buttons, a results table (ID, Province, City, Operator,
Technology, Signal Strength, Timestamp), and a bar chart of **average signal quality
(0–100 %)** grouped by operator or technology — quality is `(avg dBm + 120) / 0.7`, with the
dBm equivalent recoverable from the CSV export and each bar carrying a readable value label.
Export writes the current view to `reports/export/export_<timestamp>.csv` and shows the path
in the status bar, which also always states `Showing N rows` (the FULL query size). The table
is **paginated**: a page-size selector (50/100/200/1000, default 200), Prev/Next buttons with
edge disabling, and a `Page k of n` readout — every re-query and page-size change returns to
page 1, and one page keeps the UIA tree automation-fast.

**Dashboard**: KPI readouts (average quality, cell count, best/worst operator), a 7-day
quality trend, and the weakest-cells list. **Reports**: group summary with CSV export and a
daily drill-down trend. **Users** (admin only): list registered users, remove non-admin
accounts (removing `admin` is refused by the app).

- Database: SQLite. **`database/fixtures/` holds the committed `.db` fixtures — the only
  source of truth** (`seed_full.db`: the deterministic 10080-row/7-day dataset; `empty.db`:
  schema + seeded users, zero cells). The app always opens `database/signaltracker.db`, a
  **git-ignored working copy** that the test stack materializes per test: the mapped fixture
  is restored (copied over) and migrated (`database/migrations/*.sql`, `PRAGMA
  user_version`-gated) before EVERY test — never a stale db. Restore one manually with
  `scripts\restore-fixture.ps1 [-Fixture <name>]`. If the working db is absent, the run
  scripts restore the default fixture; the app's own first-launch self-seed remains only as
  a safety net.
- Every interactive control carries a **stable identifier** (`provinceCombo`, `searchButton`,
  `signalTable`, `signalChart`, …) set on both `objectName` and `accessibleName` — that's what
  automation locates (UIA `Name` strategy; see `automation/locators.py`). Read-only KPI/status
  readouts mirror their DISPLAYED text in `accessibleName` (the only readable surface WAD
  offers).

## The 19 E2E tests

`TC001` launch + register-first login · `TC002` province filter · `TC003` city filter
(cascade, Fars→Shiraz) · `TC004` technology filter · `TC005` operator filter · `TC006`
combined filters · `TC007` reset · `TC008` refresh row-count equality · `TC009` export CSV ·
`TC010` chart visibility · `TC011` register (fresh/duplicate/empty) · `TC012` login
(failure copies + success) · `TC013` logout returns to the gate · `TC014` dashboard KPIs /
trend / weakest cells · `TC015` users admin (remove user, admin guard) · `TC016`
pagination (page windows cross-checked against the CSV export, page-size selector) ·
`TC017` row-id DSL (`should_contain_row` / `should_not_contain_row` over the realized ID
column) · `TC018` empty fixture (sign-in works on 0 cells: `Page 1 of 1`, `Showing 0 rows`) ·
`TC019` **intentional demo failure** (`Page 1 of 999` vs the real `Page 1 of 51` — never
"fix" it; expected full-suite: 24 passed, 1 failed).
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
- Stale windows from a crashed run are killed automatically before each session, and the
  app process is factory-owned (terminated when the driver session quits —
  `automation/drivers/factory.py`). WAD pins a session to ONE top-level window, so the DSL
  re-attaches across the sign-in ⇄ main-window transitions.
