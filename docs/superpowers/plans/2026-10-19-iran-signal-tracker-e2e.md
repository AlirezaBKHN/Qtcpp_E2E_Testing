# Iran Signal Tracker Demo + Appium E2E — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A standalone Qt 6 Widgets "Iran Signal Tracker" (SQLite-backed) plus an Appium/WinAppDriver E2E framework with a human-readable DSL, 10 pytest tests, TC docs, and Allure reporting — built AND run on this machine.

**Architecture:** Qt Widgets app exposes every control with a stable identifier. A Python DSL layer wraps page objects, which wrap a driver factory (Appium 2 + community windows driver → WinAppDriver, with a hidden direct-WinAppDriver fallback). Allure pytest plugin emits results to `reports/allure-results`; the npm Allure 3 CLI renders the report.

**Tech Stack:** Qt 6.8 LTS (Widgets + Charts, MinGW kit via aqtinstall), CMake/Ninja, QtSql/SQLite, Python 3.12, pytest, appium-python-client, Appium 2.x (`appium-windows-driver`), WinAppDriver 1.2.1, Allure 3 (npm).

**Spec:** The customer handoff PRD reproduced verbatim in this conversation (session handoff titled "Qt Signal Tracker Demo + Appium E2E Automation"). Decisions recorded below came from the grilling session with the owner: full local run with guided elevation; cascading filters; option (c) data mapping; silent export; binding DSL contract; seed-on-launch + committed DB; group-by chart; incremental commits; aqt/MinGW toolchain (no Visual Studio); Appium 2 in the loop + direct fallback; standard install paths.

## Global Constraints

Platform and plumbing:
- Host: Windows 11, shell is non-admin; every per-machine install triggers a UAC prompt the owner will approve. Guide the owner at each prompt with exact instructions.
- Install paths: Qt tree at `C:\Qt` (aqt), Python 3.12 at default (`C:\Program Files\Python312`), Appium/Allure via `npm i -g`, WinAppDriver MSI default path. No PATH surgery required by any later task — build/test commands use absolute tool paths resolved in `automation/config.py` and documented in README.
- Ports: Appium server on `4723`; direct-WinAppDriver fallback on `4729`. Fallback driver is only activated when env `DRIVER_MODE=direct` is set.
- Repo `C:\git\final_project` on `main`; already contains a stub README, `.agents/`, `.opencode/`, `skills-lock.json` — leave the scaffolding in place, add it to `.gitignore`.

Application:
- App name / window title: `Iran Signal Tracker`. Executable: `IranSignalTracker.exe`. Qt Widgets only (no QML), QtSql + QtCharts modules.
- Mandatory stable identifiers (set BOTH `objectName` AND `setAccessibleName` to the identical string): `provinceCombo`, `cityCombo`, `technologyCombo`, `operatorCombo`, `searchButton`, `refreshButton`, `resetButton`, `exportButton`, `signalTable`, `signalChart` (chart widget), plus `chartGroupBy` for the new group-by combo.
- Provinces (exactly these 10): `Tehran, Isfahan, Fars, Mazandaran, Khorasan Razavi, East Azerbaijan, West Azerbaijan, Khuzestan, Gilan, Kerman`.
- City mapping (option c, ~20 cities): `Tehran → Tehran, Karaj, Qom` (Karaj/Qom folded under Tehran per approved simplification); `Isfahan → Isfahan, Kashan`; `Fars → Shiraz`; `Mazandaran → Sari, Amol`; `Khorasan Razavi → Mashhad, Neyshabur`; `East Azerbaijan → Tabriz`; `West Azerbaijan → Urmia`; `Khuzestan → Ahvaz, Abadan`; `Gilan → Rasht`; `Kerman → Kerman, Bam` (17 cities).
- Technologies: `2G, 3G, 4G, 5G`. Operators: `MCI, Irancell, Rightel, Shatel Mobile`.
- Seed: exactly 1500 rows into `cells` on first launch if `signaltracker.db` missing; signal_strength uniform in `[-120, -50]`; `std::mt19937` with seed `20241019`; no QFileDialog anywhere.

Filtering semantics:
- Each combo has a leading `All` entry (empty filter). City combo cascades: when Province changes to a specific province, City repopulates to `All` + that province's cities (sorted, capitalized as above); when Province is `All`, City shows all 13 cities. Selecting a city only allowed contextually — selecting a City never resets Province.
- Search applies Province + City + Technology + Operator WHERE clauses (empty = no filter) with `ORDER BY province, city, operator, technology`.
- Reset: all four combos → `All`, results refill unfiltered.
- Refresh: re-run the current query and repopulate table + chart.
- Export CSV: writes current table view to `reports/export/export_YYYYMMDD_HHMMSS.csv` (create dirs if missing) and shows the path in the status bar; if zero rows, writes only header row + still creates file.
- Chart: `QChartView` bar chart of `AVG(signal_strength)` per Operator (default) or per Technology, switched by `chartGroupBy` combo, tracking the current filters.

DSL binding contract (tests must read exactly like this; only additive helpers allowed):
```python
tracker.launch()
tracker.province("Tehran"); tracker.city("Tehran"); tracker.technology("5G"); tracker.operator_name("Irancell")
tracker.search(); tracker.reset(); tracker.refresh()
tracker.table().should_have_rows()            # > 0 rows
tracker.table().should_have_row_count(n)      # additive helper
tracker.table().should_contain("Irancell")    # any visible cell text match
tracker.chart().should_be_visible()
tracker.export().should_create_file()         # newest file in reports/export appears after click
tracker.quit()
```

Test suite: exactly 10 tests `TC001`–`TC010` per the handoff; TC003 selects Province `Fars` then City `Shiraz` (cascade). TC docs in `docs/test-cases/TC00{1..10}.md` using the handoff template. Allure results to `reports/allure-results/`; screenshot attached on failure. CI/CD is a documented flow in `ci/README.md` only.

## Review Focus

Inputs/conditions the spec implies but no task's automated test covers today — each is pinned by the named task's live verification step:

1. **Qt UIA exposes identifiers** — WinAppDriver may not surface objectName as AutomationId. Expect: `driver.find_element(by ID, "provinceCombo")` or by NAME "provinceCombo" succeeds within 10 s. Pinned: Task 6 locator-discovery run.
2. **Cascade must not self-trigger a search or drop user city choice** — changing Province repopulates City but leaves `All` selected and no auto-query. Pinned: Task 4 manual smoke + TC003.
3. **Export with zero visible rows** — still creates a header-only CSV. Pinned: Task 4 inspection + TC009 run right after a Reset-all-but-operator-impossible filter if needed.
4. **Combo identifiers when window resizes / DPI changes** — element IDs must be resolvable, not screen positions. Pinned implicitly by every TC using locator strategies, never coordinates.
5. **Stale rows after Refresh just after DB exists but query changes** — Refresh re-queries but must not double-load rows. Pinned: TC008 row-count equality assertion.

---

### Task 1: Toolchain provisioning (guided installs, no repo changes)

Provisions the missing stack. Every elevation step states what the owner should see and click. No TDD here; each step has a verifiable command.

**Files:** none in repo.

- [ ] **Step 1: Install Python 3.12.** Download `python-3.12.x-amd64.exe` from python.org via `Invoke-WebRequest` to `C:\Users\user\AppData\Local\Temp\opencode`, then `Start-Process -Wait -Verb RunAs` with `/quiet InstallAllUsers=1 PrependPath=1`. Run non-elevated `python --version`; if PATH is not refreshed in the current shell, resolve `C:\Program Files\Python312\python.exe` directly. If `Invoke-WebRequest` or the exe fails, tell the owner to download/install manually from `https://www.python.org/downloads/windows/` and click "Add python.exe to PATH".
  Run: `python --version` → `Python 3.12.x`.

- [ ] **Step 2: pip install aqtinstall; enumerate exact Qt components.**
  `python -m pip install aqtinstall` then:
  - `aqt list-qt windows desktop` → confirm latest `6.8.x` (use that version below as `<V>`)
  - `aqt list-qt windows desktop <V> --arch` → confirm `win64_mingw`
  - `aqt list-tool windows desktop` → note exact tool directory names for cmake, ninja, mingw (expected `tools_cmake`, `tools_ninja`, `tools_mingw1310` or similar; record actual).

- [ ] **Step 3: Install Qt + compiler + build tools.**
  `aqt install-qt windows desktop <V> win64_mingw -m qtcharts` → `C:\Qt\<V>\mingw_64`
  `aqt install-tool windows desktop tools_cmake` → `C:\Qt\Tools\CMake_64\bin\cmake.exe`
  `aqt install-tool windows desktop tools_ninja`, `aqt install-tool windows desktop <mingw tool dir from Step 2>`.
  Run: `C:\Qt\Tools\CMake_64\bin\cmake.exe --version` → `3.x`.

- [ ] **Step 4: Install Appium 2 + windows driver.** `npm install -g appium@2` then `appium driver install --source npm appium-windows-driver`.
  Run: `appium driver list --installed` → shows `windows`.

- [ ] **Step 5: Install Allure CLI (npm v3, no Java) and pytest deps.**
  `npm install -g allure` and `python -m pip install pytest allure-pytest appium-python-client`.
  Run: `allure --version`, `python -m pytest --version` → both succeed.

- [ ] **Step 6: Install WinAppDriver (guided elevation).** Download `WindowsApplicationDriver_1.2.1.msi` from `https://github.com/microsoft/WinAppDriver/releases` (tag v1.2.1) into the temp dir, then `Start-Process msiexec -ArgumentList '/i <path> /qn' -Verb RunAs -Wait`. Tell the owner: "A UAC prompt for Windows Installer will appear — approve it."
  Run: `Test-Path 'C:\Program Files (x86)\Windows Application Driver\WinAppDriver.exe'` → `True`. If the MSI refuses (`msiexec` in use or defender flag), fall back to double-click guidance the owner follows.

- [ ] **Step 7: Record the resolved environment in `docs/env-setup.md`** (exact Qt version/tool paths installed) — created as part of Task 5's docs commit if Task 5 runs later; otherwise stage the content here in the session notes. **No commit** (not yet a tracked deliverable).

---

### Task 2: Repo skeleton

**Files:**
- Create: `.gitignore`
- Create (dirs only, via files added later): `app/src/`, `app/tests/`, `database/`, `automation/`, `docs/test-cases/`, `reports/export/`, `ci/`
- Modify: `README.md`
- Create: `reports/export/.gitkeep`

**Interfaces:** Produces repo structure consumed by every later task.

- [ ] **Step 1: Write `.gitignore`** with entries: `build*/`, `reports/allure-results/`, `reports/allure-report/`, `reports/export/*.csv`, `*.db-journal`, `__pycache__/`, `.agents/`, `.opencode/`, `skills-lock.json`, `.pytest_cache/`.
- [ ] **Step 2: Write placeholder `README.md`** (one sentence: what the repo is; real content is Task 10).
- [ ] **Step 3: Commit**
```bash
git add .gitignore README.md reports/export/.gitkeep
git commit -m "chore: project skeleton and gitignore"
```

---

### Task 3: Qt app shell — window, header, layout, all controls

**Files:**
- Create: `app/CMakeLists.txt`
- Create: `app/src/main.cpp`
- Create: `app/src/mainwindow.h`
- Create: `app/src/mainwindow.cpp`
- Create: `app/src/db.h` (declarations only; bodies Task 4)
- Create: `app/src/db.cpp` (stub bodies compiling)

**Interfaces:**
- Produces: class `MainWindow : public QMainWindow` with private members `QComboBox *provinceCombo, *cityCombo, *technologyCombo, *operatorCombo, *chartGroupBy; QPushButton *searchButton, *refreshButton, *resetButton, *exportButton; QTableWidget *signalTable; QChartView *signalChart; QStatusBar` and slots `void onSearch(); void onRefresh(); void onReset(); void onExport(); void onProvinceChanged(int)`. `Db` (Task 4): `bool init(); QVector<QVariantList> queryFilters(const QString&, const QString&, const QString&, const QString&); QVector<QPair<QString,double>> avgBy(const QString& groupColumn, const QString& where…)`.
- Produces: `app/build-mingw/IranSignalTracker.exe` (build dir committed off via `.gitignore` `build*/`).

- [ ] **Step 1: Write failing build smoke.** Create `app/tests/smoke_main.cpp`: `#include "mainwindow.h"`, `int main(int argc, char**argv){ QApplication app(argc,argv); MainWindow w; w.show(); QMetaObject::invokeMethod(&w, &QWidget::close, Qt::QueuedConnection); return app.exec(); }` — asserts the class exists and constructs with every mandatory identifier set (ctor sets `objectName` + `setAccessibleName` for the 10 names in Global Constraints). Registered as a CMake target `app_smoke` and wired into `ctest`.
- [ ] **Step 2: Run and watch it fail.** `build-mingw` dir absent; expect configure failure / compile error.
- [ ] **Step 3: Implement CMakeLists.txt** — `cmake_minimum_required(VERSION 3.24)`, `project(IranSignalTracker LANGUAGES CXX)`, `set(CMAKE_CXX_STANDARD 17)`, `find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Sql)`, `qt_standard_project_setup()`, `qt_add_executable(IranSignalTracker WIN32 ...)`, `target_link_libraries(... Qt6::Widgets Qt6::Charts Qt6::Sql)`, smoke target linking same. Test target must not be `WIN32` (console output for ctest).
- [ ] **Step 4: Implement `main.cpp`** (QApplication, set org/app name "Iran Signal Tracker", MainWindow, show).
- [ ] **Step 5: Implement `MainWindow` constructor** — filter row (four QComboBox with `All` first), four QPushButtons with verbatim Global-Constraint names, `signalTable` with 7 columns captioned `ID, Province, City, Operator, Technology, Signal Strength, Timestamp`, pane splitter: table top, `signalChart` bottom, `chartGroupBy` combo (`Operator` default / `Technology`) beside chart. DB calls stubbed in Task 3; wiring real, body minimal (`onSearch` fills table with data from stub `Db::queryFilters`, `onProvinceChanged` repopulates cityCombo from `Db` stub, `onExport` writes header-only CSV — real logic refined in Task 4).
- [ ] **Step 6: Build & run ctest.**
  Run: `C:\Qt\Tools\CMake_64\bin\cmake.exe -S app -B app\build-mingw -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\<V>\mingw_64 -DCMAKE_C_COMPILER=C:\Qt\Tools\<mingwToolDir>\bin\gcc.exe -DCMAKE_CXX_COMPILER=C:\Qt\Tools\<mingwToolDir>\bin\g++.exe` then `cmake --build app\build-mingw` then `ctest --test-dir app\build-mingw`.
  Expected: configure + build succeed; ctest passes `app_smoke`.
- [ ] **Step 7: Commit**
```bash
git add app
git commit -m "feat: Qt Widgets shell for Iran Signal Tracker with stable control names"
```

---

### Task 4: Database layer + seeding + committed DB file

**Files:**
- Modify: `app/src/db.h`, `app/src/db.cpp`
- Create: `app/tests/db_test.cpp`
- Create: `database/signaltracker.db` (generated, committed)

**Interfaces:**
- Produces: `bool Db::init(const QString& dbPath)` — opens QtSql `QSQLITE` connection to `signaltracker.db` resolved next to the .exe (falling back to CWD app-relative `..\..\database`), creates `cells` table if absent (schema: `id INTEGER PRIMARY KEY, province TEXT, city TEXT, operator TEXT, technology TEXT, signal_strength INTEGER, sample_time TEXT`) and seeds with `std::mt19937(20241019)`: 1500 rows, uniform provinces/cities mapped per Global Constraints, operators & tech uniform uniform_int, uniform_int signal `[-120,-50]`, sample_time `QDateTime::currentDateTime()` staggered back one minute per row (deterministic content except timestamps — acceptable, timestamps are not asserted).
- Produces: `QVector<QVariantList> queryFilters(p,c,t,o)` (see Task 3), `QVector<QPair<QString,double>> avgBy(groupColumn, where filters)` producing `(label, avg)` series for the chart.
- Produces: `app/build-mingw/database/seed_test.exe`? No — `app_dbtest` ctest target.

- [ ] **Step 1: Write failing `app/tests/db_test.cpp`.** Assertions: init() creates file on first call and binds schema; seeded row count is 1500 and stable across repeated init on the same file; every row's province is from the Global-Constraint list, city matches the province map, technology ∈ {2G,3G,4G,5G}, operator ∈ {MCI…}, `signal_strength` ∈ [-120,-50]; `queryFilters("Fars","Shiraz","", "")` returns >0 rows and every returned city == Shiraz; `avgBy("operator","…")` returns exactly one point per operator with avg in range; delete DB file, re-init → count 1500 again.
- [ ] **Step 2: Run → FAIL** (bodies stubbed, likely link/assert failure). `ctest --test-dir app\build-mingw -R app_dbtest`.
- [ ] **Step 3: Implement `db.cpp`** (`QSqlQuery` for schema + insert with prepared statements; use `QSqlDatabase::connectionName` reuse; groups/cities as two `static const char*` arrays whose lengths must match — asserted in db_test).
- [ ] **Step 4: Run → PASS.**
- [ ] **Step 5: Generate committed DB artifact** (run the exe's init once with a tiny helper or copy the freshly seeded file): copy `app\build-mingw\..\..\database\signaltracker.db` once created; commit it so a fresh clone starts deterministic.
- [ ] **Step 6: Commit**
```bash
git add app/src/db.h app/src/db.cpp app/tests/db_test.cpp database/signaltracker.db
git commit -m "feat: SQLite layer with deterministic 1500-row seed + committed db"
```

---

### Task 5: Behavior wiring + cascade + export + chart

**Files:**
- Modify: `app/src/mainwindow.cpp` (bodies from Task 3 finalized, cascade `onProvinceChanged` repopulates cityCombo preserving `All`, `searchButton` default button, `chartGroupBy` switches chart series), `app/src/db.cpp` if needed.
- Modify: `app/tests/db_test.cpp` (extend with export-string-format check) — small.
- Create: `reports/export/` runtime output only (gitignored csv).

**Interfaces:** Consumes Task 3/4 APIs; produces behavior for Tasks 8+.

- [ ] **Step 1: Write failing behavior assertions** into `app_dbtest`: helper `expectedCitiesFor(provinceSql)` equals cityCombo expectations; extend export test: Db exposes `writeExportCsv(path, filters)` shared by MainWindow::onExport so CSV logic is testable outside the GUI. Test asserts: exported file exists after call, first line is header `id,province,city,operator,technology,signal_strength,timestamp`, data lines count == queryFilters count, zero-filter case still yields header-only file + `true`.
- [ ] **Step 2: Run → FAIL.**
- [ ] **Step 3: Implement MainWindow bodies** (cascading, export via `Db::writeExportCsv`, chart refresh hooking `chartGroupBy` + onSearch/onRefresh/onReset; status bar shows export path; `refresh()` reuses current filter widgets).
- [ ] **Step 4: Run → PASS; launch exe manually** (Task 8 verification builds E2E on this.
- [ ] **Step 5: Update Companion docs now:** `docs/env-setup.md` (from Task 1 notes) + `docs/test-cases/TC001.md`–`TC010.md` (handoff template, cascade note on TC003/TC006) + `ci/README.md` (commit→build→appium→tests→allure flow, no pipeline code).
- [ ] **Step 6: Commit**
```bash
git add app/src app/tests docs
git commit -m "feat: filters/search/export/chart behavior + build docs + test catalog"
```

---

### Task 6: Automation core — config, locators, driver factory

**Files:**
- Create: `automation/requirements.txt` (`pytest`, `allure-pytest`, `appium-python-client`, plus `python-dotenv` only if needed)
- Create: `automation/pytest.ini`
- Create: `automation/config.py` (`APP_EXE` abs path to `app\build-mingw\IranSignalTracker.exe`, `EXPORT_DIR = repo/reports/export`, `ALLURE_RESULTS = repo/reports/allure-results`, `APPIUM_URL = http://127.0.0.1:4723`, `DIRECT_URL = http://127.0.0.1:4729`, `DRIVER_MODE` from env: `appium`|`direct`)
- Create: `automation/locators.py` — per control a tuple of strategies `[(id,"provinceCombo"),(name,"provinceCombo"),(xpath with Name,text…)]` — no hardcoded coordinates.
- Create: `automation/drivers/factory.py` — creates WinAppDriver session via Appium (caps: `platformName:"Windows"`, `appium:app:<exe>`, `appium:automationName:"Windows"`, implicit wait 10 s) OR direct (caps without `appium:` prefixes, remote `4729`, app launched via `subprocess.Popen`); exposes `get_driver()`, `quit_driver()`.
- Create: `automation/tests/__init__.py`, `automation/tests/__init__session.py` fixture (screenshot-on-failure + allure.attach).

**Interfaces:**
- Produces: `dut = factory.get_driver()` returning a Selenium/Appium `WebDriver` handle used only by page objects (Task 7); `Config.APP_EXE`, `Config.EXPORT_DIR`, screenshot failure helper `attach_failure(driver)`.

- [ ] **Step 1: pip install -r automation/requirements.txt; run `python -c "import automation.config, automation.drivers.factory"`** from repo root (add `automation/__init__.py`).
  Expected: no ImportError.
- [ ] **Step 2: Locator discovery run (live, ungated).** Start WinAppDriver (`WinAppDriver.exe 4729`) and Appium (`appium`); run a one-off `discovery.py` printing all elements' automation-id/name via `find_elements("id","*.*")==` — pin the actually-working strategy per control and hard-code it into `locators.py` (Risk item 1 from Review Focus). Record outcome in `docs/env-setup.md`.
  Expected: every control resolves by a stable strategy ≤ 2.
- [ ] **Step 3: Commit**
```bash
git add automation
git commit -m "feat: automation core - config, locators, driver factory"
```

---

### Task 7: Page objects + DSL

**Files:**
- Create: `automation/pageobjects/main_page.py` (`MainPage`: `apply_filters` via combo expanders, `click_search/refresh/reset/export`, `is_visible()`)
- Create: `automation/pageobjects/table_widget.py` (`table().rows()`, `contains(text)`, `row_count()` — reads cells via safe text gets)
- Create: `automation/pageobjects/chart_widget.py` (`is_displayed()`)
- Create: `automation/dsl/tracker.py` (`SignalTrackerDsl` facade + pinned Global-Constraint API incl. additive `should_have_row_count(n)`, `quit()`)

**Interfaces:**
- Consumes: `locator(provinceComboElementName) → CtlId` from Task 6; produces DSL runtime elements that all 10 tests (Tasks 8–9) consume; also `tracker.table().should_contain` reading only *visible* rows via table row `IsDisplayed` (WinAppDriver truncation workaround: rows outside the viewport still exist in AutomationTree but text may be cut — count by table's own data rows).

- [ ] **Step 1: Live probe script** (ungated `automation/tools/probe.ts` python): launch via factory, run the full DSL happy path, assert each combo sets value, search fewer rows, export file newest mtime, chart element `is_displayed()==True`. Failures tuned against.
- [ ] **Step 2: Cleanup to zero earlier helper artifacts and commit**
```bash
git add automation
git commit -m "feat: page objects and DSL layer per handoff contract"
```

---

### Task 8: TC001–TC004 (green)

**Files:**
- Create: `automation/tests/test_tc001_launch.py`, `test_tc002_province_filter.py`, `test_tc003_city_filter.py`, `test_tc004_technology_filter.py`
- Create: `automation/conftest.py`

**Interfaces:**
- Consumes: DSL from Task 7; produces the loaders every test file uses (`tracker` DSL fixture, winappdriver-available skip guard).
- TC003′ (cascade): select Province `Fars` → City combo must show only `Shiraz` (+All) → select `Shiraz` → search → rows > 0.
- TC002/TC004 pin techs: 5G; provinces Tehran; assert `should_have_rows()` + `should_contain("Irancell")` for TC002 only if data supports (assert >0 rows universally; content narrowed by single filter optionally).

- [ ] **Step 1: `automation/conftest.py`** — session-scoped pre-openned driver + DSL fixture; `pytest_addoption --allure` not needed (allure-pytest auto); skip guard: if neither Appium (4723) nor direct (4729) reachable, skip all 10 tests with clear message.
- [ ] **Step 2: Write 4 test files, each ≤ 6 lines of body** (launch → assert window visible; filter → search → assert rows; per Global Constraints).
- [ ] **Step 3: Run.** `python -m pytest automation/tests/test_tc001_launch.py automation/tests/test_tc002_province_filter.py automation/tests/test_tc003_city_filter.py automation/tests/test_tc004_technology_filter.py -v`
  Expected: 4 passed.
- [ ] **Step 4: Commit**
```bash
git add automation/tests automation/conftest.py
git commit -m "test: TC001-TC004 green"
```

---

### Task 9: TC005–TC010 (green)

**Files:**
- Create: `automation/tests/test_tc005_operator_filter.py`, `test_tc006_combined_filter.py`, `test_tc007_reset.py`, `test_tc008_refresh.py`, `test_tc009_export_csv.py`, `test_tc010_chart_visibility.py`
- Create: `automation/dsl/export_result.py` (`export()` value object wrapping newest-file-in-dir check `should_create_file()`)

**Interfaces:**
- TC005: operator Irancell → search → `should_contain("Irancell")`.
- TC006: Tehran + Tehran + 5G → search → `should_have_rows()` (and >0 count helper recorded).
- TC007: apply province Tecn+operator → reset → row count == unfiltered count (pin equality to TC001 baseline; deterministic 1500 seeds make this safe).
- TC008: refresh returns identical row count to pre-refresh (stale/duplicate check).
- TC009: export → `should_create_file()` → csv parses and has exact expected header line.
- TC010: `chart().should_be_visible()`.

- [ ] **Step 1: Write the 6 tests + DSL export object.**
- [ ] **Step 2: Run all 10 in one -v invocation.** Expected: 10 passed.
- [ ] **Step 3: Commit**
```bash
git add automation
git commit -m "test: TC005-TC010 green; full 10-test suite passing"
```

---

### Task 10: Allure report + final docs

**Files:**
- Modify: `README.md` (full Build/Run + WinAppDriver guides + pytest + `allure serve reports\allure-results`)
- Create: `reports/allure-results/.gitkeep`? No — results are gitignored; verify generation only.

- [ ] **Step 1: Run suite with results capture.** Clear old results dir first.
  Run: `allure generate reports\allure-results -o reports\allure-report --clean` → `reports/allure-report/index.html` exists and openable; 10 tests visible in browser.
- [ ] **Step 2: Finalize README** (mirroring the handoff's markdown contract verbatim), links to docs, TC catalog, env-setup.
- [ ] **Step 3: Commit + push (unless owner defers)**
```bash
git add README.md docs
git commit -m "docs: full README, allure workflow, final docs"
git push origin main
```

---

## Verification snapshot (what the owner will see at the end)

| PRD criterion | Proof |
|---|---|
| Application launches | exe double-clickable; smoke ctest; TC001 |
| SQLite populated automatically | fresh-clone run shows 1500 rows; committed db; db_test |
| Appium detects controls | Task 6 discovery log |
| Stable identifiers | Global Constraints list; locator table in env-setup.md |
| DSL hides Appium | tests contain zero driver calls |
| 10 tests pass | pytest output; allure report |
| Allure report | allure-report/index.html |
| Clone-and-run | fresh clone → build → appium → pytest → allure, docs cover it |
