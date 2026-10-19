# Auth + Tabs + Dashboards Implementation Plan (Iran Signal Tracker)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Register-first/login UX, tabbed main window (Home routing / Network Data / Dashboard / Reports / Users-for-admin), 7-day trend-able dataset, per-test DB restore for E2E isolation, and 15 green E2E tests speaking an extended DSL.

**Architecture:** `app/`: `Db` gains a `users` table + auth/register/remove/list APIs + day-indexed seeding; a blocking `SignInDialog` gates `main.cpp` (loop: dialog → MainWindow, logout returns to dialog); MainWindow becomes a `QTabWidget` with `HomeView` (nav cards) + relocated network screen + `DashboardView` + `ReportsView` + `UsersView`. `automation/`: conftest restores the committed DB before every test; DSL grows additively (`network()`/`dashboard()`/`reports()`/`users()` sub-handles, `register`/`login`/`logout` actions + separate message readers); page objects layer under the frozen locator strategy (name-first, foreground enforced at launch).

**Tech Stack:** Qt 6.8.3 Widgets + Charts + Sql (MinGW), SQLite, pytest + Appium/WinAppDriver, Allure.

**Spec:** the grilling session (2026-10-19) in this conversation + current session handoff PRD; rulings recorded in `.superpowers/sdd/2026-10-19-iran-signal-tracker-e2e/progress.md`.

## Global Constraints

- Every interactive control sets BOTH `objectName` and `accessibleName` to the same stable id (`makeStable` in `app/src/mainwindow.cpp`). New ids this round: `signinWindow`, `usernameField`, `passwordField`, `loginButton`, `registerToggle`, `registerButton`, `signinStatus`, `registerStatus`, `mainTabs`, `homeTab`, `networkTab`, `dashboardTab`, `reportsTab`, `usersTab`, `currentUserLabel`, `logoutButton`, `homeCardNetwork`, `homeCardDashboard`, `homeCardReports`, `homeCardUsers`, `kpiAvgQuality`, `kpiCells`, `kpiBestOperator`, `kpiWorstOperator`, `trendChart`, `worstCellsList`, `reportGroupBy`, `reportTable`, `dailyTrendChart`, `exportReportButton`, `usersTable`, `removeUserButton`.
- DSL is the only language of tests (`automation/tests/**`: zero `find_element`/driver calls). Actions perform, readers return data — tests do the asserting.
- Locator strategy: name-first UIA (`Name` = accessibleName); WAD quirks hold: no `starts-with`, row counts via status bar `Showing N rows`, foreground enforcement via `drivers/foreground.py` at every launch.
- `db.cpp` stays the single source of domain truth (province/city/operator/tech tables + all aggregates). Never duplicate literals elsewhere.
- Seeder deterministic: fixed `std::mt19937(20241019)`; committed `database/signaltracker.db` regenerated with `& app\build-mingw\app_dbgen.exe database\signaltracker.db` after any seed change (Tasks 1–2).
- Passwords stored PLAINTEXT (owner ruling: demo-grade, no roles column); the Users tab keys off the literal username `admin`.
- Chart pinned contract: persistent axes created once, categories set BEFORE attach, `@value` label tag + explicit black labelBrush, quality `(dBm+120)/0.7`, splitter 380/260. Network Data's chart is untouched; new chart widgets replicate these patterns.
- Kill lingering `IranSignalTracker.exe`/`app_smoke.exe` before every relink (ld Permission denied).
- Commits conventional (`feat:`/`fix:`/`test:`/`docs:`), incremental.

## Review Focus

- **Empty-password register** must create NO user and surface `Password can not be empty.` — pinned by db_test (Task 1) + TC011.
- **Duplicate username register** → `Username already exists.` and no mutation — pinned db_test (Task 1) + TC011.
- **removeUser('admin')** must fail (self-delete guard) — db_test (Task 1) + smoke (Task 4).
- **Trend granularity**: seeder timestamps bucketed by day index so `dailyQuality` always returns exactly 7 entries — db_test (Task 2/5), E2E TC014.
- **Isolation**: a crashed/aborted test must still restore state before the next one (fixture `yield`-style teardown), and exports-dir leftovers must be purged per test — conftest (Task 7), exercised by TC009/TC015.

---

### Task 1: `users` table + auth APIs (Db layer, TDD)

**Files:**
- Modify: `app/src/db.h` (declarations only, no body changes)
- Modify: `app/src/db.cpp` (init DDL + new statics)
- Test: `app/tests/db_test.cpp` (extend)

**Interfaces:**
- Consumes: existing `Db::init` DDL/seed flow.
- Produces (all later tasks):
  - `static QStringList usernames();` — sorted, current user list.
  - `static bool registerUser(const QString& username, const QString& password);` — false when username empty or already exists, or password empty; true + insert otherwise. Uniqueness is CASE-SENSITIVE exact match. (No length rule — owner ruling: only non-empty + unique.)
  - `static bool authUser(const QString& username, const QString& password);` — exact match on both.
  - `static bool removeUser(const QString& username);` — false for `admin` or unknown users.
- Seed change (Task 1): init creates `users` table + seeds exactly: `admin/admin`, `guest/guest`, `e2e_user/e2e123` (plaintext; owner-confirmed credentials).
- Seeder revision (Task 2): owner estimated ~10.5k rows ("7 × 1500"); the plan pins **10080 = 1440/day × 7 exactly** so the trend always yields clean 7 day-buckets — equivalent intent, cleaner invariant, falls out of the same ruling.

- [ ] **Step 1: Write failing db_test slot** `users_auth()`: registerUser success; duplicate → false; empty password → false; authUser true/false; removeUser("admin") false; removeUser on a registered temp user → true then absent from usernames(); usernames() sorted.
- [ ] **Step 2: Run** `powershell -ExecutionPolicy Bypass -File scripts\run-ctest.ps1` → expect db_test FAIL (missing symbols).
- [ ] **Step 3: Implement** DDL `CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password TEXT NOT NULL)` + the four statics (plain SQL through existing `QSqlQuery` style) + seed admin/guest/e2e_user rows at init.
- [ ] **Step 4: Re-run ctest** → 2/2 PASS.
- [ ] **Step 5: Commit** `feat: users table + register/auth/remove/list SQL layer`.

### Task 2: 7-day deterministic seeder + regenerate committed DB

**Files:**
- Modify: `app/src/db.cpp` (`seed`, constants)
- Modify: `app/tests/db_test.cpp` (row-count/aggregation pins)
- Regenerate: `database/signaltracker.db` (git artifact)

**Interfaces:**
- Produces: seeder constants `kSeedDays = 7`, `kRowsPerDay = 1440` (10080 total); timestamps = `(utcToday - (6 - day) days).addSecs(rng(0..86399))` with day = row/1440 — guarantees exactly 7 date buckets for `dailyQuality`. Row COUNT pins move 1500 → 10080 wherever a literal existed.

- [ ] **Step 1: Update db_test pins** (RED): total rows 10080; per-day buckets exactly 7; Tehran 959, Mazandaran 952, Fars 1050 (=province total ×7; read exact per-province counts from the regenerated DB after Step 2 and update — keep only relative assertions where literals are brittle).
- [ ] **Step 2: Implement** seeder change; run ctest → FAIL then regenerate (`& app\build-mingw\app_dbgen.exe database\signaltracker.db`) → re-run ctest PASS.
- [ ] **Step 3: Commit** db + app: `feat: 7-day deterministic dataset (10080 rows, day-bucketed timestamps)`.

### Task 3: SignInDialog + login gate in main.cpp

**Files:**
- Create: `app/src/signin_dialog.h`, `app/src/signin_dialog.cpp`
- Modify: `app/src/main.cpp`
- Test: `app/tests/smoke_main.cpp` (extend)

**Interfaces:**
- Consumes: `Db::registerUser/authUser`.
- Produces: `class SignInDialog : public QDialog` with ids `signinWindow` (objectName on the dialog itself) and mode toggle in-place (Login ⇄ Register radio-less: one `registerToggle` button switches layout). Status labels `signinStatus` / `registerStatus` per mode. Signals: `acceptedWithUser(const QString&)`. Register success: label `Registered — log in.`, fields cleared, stays in dialog. main.cpp: `for (;;) { dialog.exec(); if (rejected) return 0; MainWindow w(currentUser); w.show(); app.exec(); if (w.shouldExit()) return 0; }` — logout closes the main window and re-opens signin.
- Pinned copy: `Wrong password.`, `Unknown username.`, `Username already exists.`, `Password can not be empty.`, `Registered — log in.`

- [ ] **Step 1: RED smoke slots** `signinGateBlocksMainWindow()` (constructing app flow requires auth — smoke constructs the dialog directly + asserts ids/copy) and `registerModeValidation()` (click register with empty password → registerStatus == "Password can not be empty."). Implementers follow existing smoke slot style.
- [ ] **Step 2: RED run** ctest → smoke FAIL.
- [ ] **Step 3: Implement** SignInDialog + main.cpp loop (`MainWindow::shouldExit()` bool member set by logout).
- [ ] **Step 4: GREEN run** ctest → 2/2.
- [ ] **Step 5: Commit** `feat: blocking sign-in dialog with register-mode and logout loop`.

### Task 4: MainWindow → tabs + Home + users/admin + logout wiring

**Files:**
- Create: `app/src/users_view.h`, `app/src/users_view.cpp`
- Modify: `app/src/mainwindow.h`, `app/src/mainwindow.cpp`
- Test: `app/tests/smoke_main.cpp`

**Interfaces:**
- Consumes: Task 1 APIs (removeUser/usernames), Task 3 acceptedWithUser.
- Produces: `MainWindow(const QString& user, QWidget* parent = nullptr)`; top bar: `currentUserLabel` = `user: <name>` + `logoutButton`; `QTabWidget mainTabs` tabs in order Home, Network Data, Dashboard (Task 5), Reports (Task 6), and `usersTab` ONLY when user == "admin". Home cards: plain QPushButton row (`homeCardNetwork`…) switching `mainTabs` (SIGNAL→setCurrentIndex). `UsersView(user)`: `usersTable` (usernames, sorted) + `removeUserButton` — enabled only for admin; refuses removing `admin` (re-check in view, not just Db). `MainWindow::shouldExit()` public bool.
- All existing ids (provinceCombo…signalChart) UNCHANGED inside `networkTab`.

- [ ] **Step 1: RED smoke slots**: `tabsAndHomeCards()` (ids present incl. tabs; clicking homeCardNetwork activates networkTab; viewer has no homeCardUsers; admin' MainWindow("admin") has it; currentUserLabel text = `user: admin`), `usersViewAdminOnly()` (UsersView shows usernames sorted; removeUserButton disabled for viewer; smoke calls removeUser flow with dbtest-known data).
- [ ] **Step 2: RED run** ctest → smoke FAIL.
- [ ] **Step 3: Implement** views + restructure (existing central widget becomes networkTab content verbatim).
- [ ] **Step 4: GREEN run** ctest → 2/2 (old stick slots incl. chart contract still pass).
- [ ] **Step 5: Commit** `feat: tabbed main window with home routing cards and admin users view`.

### Task 5: Dashboard (KPIs + 7-day trend + weakest cells)

**Files:**
- Create: `app/src/dashboard_view.h`, `app/src/dashboard_view.cpp`
- Modify: `app/src/db.h`, `app/src/db.cpp`
- Test: `app/tests/smoke_main.cpp`, `app/tests/db_test.cpp`

**Interfaces:**
- Consumes: Task 2 day-bucketed data.
- Produces (also consumed by Task 6):
  - `static double networkQuality();` — overall mean quality (0..100), `(AVG(signal_strength)+120)/0.7`.
  - `static int cellCount();` — COUNT(*).
  - `static QVector<LabelAvg> dailyQuality(const QString& groupColumn, const QString& groupValue);` — exactly 7 pairs `("YYYY-MM-DD", quality)` ascending; groupColumn ∈ {province, operator, technology}; empty groupValue = whole network.
  - `static QVector<QStringList> worstCells(int limit = 8);` — rows with signal_strength < -102.5 (quality < 25%), weakest first, cap `limit`; columns match the main table.
- Ui: `DashboardView` — KPI row: `kpiAvgQuality` (`f.1f` + `%` suffix), `kpiCells` (number, `regular plural`), `kpiBestOperator`/`kpiWorstOperator` (names from `Db::avgBy("operator"…)` — ties break by first). `trendChart`: QLineSeries over a `QDateTimeAxis` (ticks formatted `yyyy-MM-dd`, one point per `dailyQuality` entry) with persistent axes following the pinned chart contract; grid visible, no bar labels. `worstCellsList` rows = `worstCells(8)` fill into the table widget's `setRowCount`-then-populate pattern.

- [ ] **Step 1: RED db_test slot** for the four new aggregates (exact expectations computed from the committed DB in-test via the same SQL).
- [ ] **Step 2: RED smoke slot** `dashboardContract()` (ids; KPI avg equals `Db::networkQuality()` (QString compare on one decimal); trend chart exactly 1 series + category axis `YYYY-MM-DD` labels (7 labels + axis-visible); worstCellsList rows populated).
- [ ] **Step 3: GREEN** ctest 2/2.
- [ ] **Step 4: Commit** `feat: dashboard view (network KPIs, 7-day quality trend, weakest cells)`.

### Task 6: Reports view (group-by summary + drill trend + export)

**Files:**
- Create: `app/src/reports_view.h`, `app/src/reports_view.cpp`
- Modify: `app/src/mainwindow.cpp` (compose into reportsTab)
- Test: `app/tests/smoke_main.cpp`

**Interfaces:**
- Consumes: `Db::avgBy` (per group column), `Db::dailyQuality(groupColumn, groupValue)`, `Db::writeExportCsv` (same silent-writer pattern as export CSV: `reports/export/report_<timestamp>.csv`, status bar confirmation via `reportsStatus_notice` conventional approach — exact copy `Exported: <path>`).
- Produces: `ReportsView(QWidget* parent = nullptr)`; ids: `reportGroupBy` (combo: Province/Operator/Technology), `reportTable` (columns: Label, Rows, Avg Quality; sortable), `dailyTrendChart` (QLineSeries updates on table row select; after selecting a row in `reportTable`, the chart shows quality-per-day for that label), `exportReportButton` (CSV follows silent-writer contract).

- [ ] **Step 1: RED smoke slot** `reportsContract()` (ids; default group Province; table row count == provinces count from kMap (10); row label == province; exportReportButton writes report_*.csv with a header line).
- [ ] **Step 2: GREEN** ctest 2/2. **Step 3: Commit** `feat: reports view (group summary, drill-down trend, report CSV export)`.

### Task 7: Automation core: isolation fixtures + signin/tabs pages + DSL redesign

**Files:**
- Modify: `automation/config.py` (E2E constants: `E2E_USER = "e2e_user"`, `E2E_PASSWORD = "e2e123"`, `ADMIN_USER = "admin"`, `ADMIN_PASSWORD = "admin"`, `SIGNIN_IDS`, `TAB_IDS`)
- Modify: `automation/locators.py` (append: signin ids, tab ids, home card ids as constants — no behavior change)
- Modify: `automation/conftest.py` (RESTORE + fixtures)
- Modify: `automation/pageobjects/main_page.py` (top bar reads + tabs) 
- Create: `automation/pageobjects/signin_page.py`, `automation/pageobjects/tabs_bar.py` (home card + tab activation + logout)
- Modify: `automation/dsl/tracker.py` (NEW: register/login/logout/readers + `network()`/`dashboard()`/`reports()`/`users()` producing `NetworkDsl`/sub-facades that OWN today's filter/table/chart/export APIs)
- Create: `automation/dsl/network.py`, `automation/dsl/dashboard.py`, `automation/dsl/reports.py`, `automation/dsl/users.py`
- Modify: `automation/tools/probe_dsl.py` (happy-path probe rewritten: launch → register → login → home cards click-through → network flow)

**Interfaces:**
- Produces (frozen naming; tests use ONLY these):
  - `SignalTrackerDsl.launch()` (bare → signinWindow visible; forced foreground), `.quit()`.
  - `.register(u, p)`, `.register_message() -> str`, `.login(u, p)`, `.signin_message() -> str`, `.logout()`, `.current_user() -> str`, `.network() -> NetworkDsl`, `.dashboard() -> DashboardDsl`, `.reports() -> ReportsDsl`, `.users() -> UsersDsl` (admin; raises if not present).
  - `NetworkDsl`: today's `province/city/technology/operator_name/search/reset/refresh/row_count/table()/export()` verbatim (moved as-is).
  - `DashboardDsl.kpi(name) -> str`, `.trend_points() -> int`, `.worst_cells_rows() -> int`, `.should_show_kpis()` etc. construct from the SAME page-object layer (no new element types).
  - conftest fixtures:
    - `tracker` — restore pristine DB + clear exports → `SignalTrackerDsl().launch()` → yield → quit.
    - `signed(tracker)` — `tracker.login("e2e_user", "e2e123")`.
  - DB restore exact mechanism: session-scope fixture snapshots the committed `database/signaltracker.db` to `%TEMP%`; function-scope `autouse` fixture copies it back BEFORE each test + clears `reports/export/*.csv`.

- [ ] **Step 1: implement** conftest + config + locators in this order.
- [ ] **Step 2: implement** page objects + DSL (probe updated and green before pytest picks it up).
- [ ] **Step 3: probe run**: `& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe automation\tools\probe_dsl.py` → all steps PASS.
- [ ] **Step 4: Commit** `feat: automation core for auth flow, tabs and dsl sub-handles with per-test db restore`.

### Task 8: Rewrite TC001–TC010 + add TC011–TC015 (15 green)

**Files:**
- Modify: `automation/tests/test_tc00{1..10}_*.py` (add `signed`-based fixtures, `.network()` prefix on filter ops; multi-tab tests: navigate home card)
- Create: `automation/tests/test_tc011_register.py`, `test_tc012_login.py`, `test_tc013_logout.py`, `test_tc014_dashboard.py`, `test_tc015_users_admin.py`
- Modify: `docs/test-cases/TC001..TC015` (rewrite for new flow)

**Test catalog (assert style: messages read/compared in the TEST, not inside DSL):**
- TC001 launch → `signinWindow` appears with all ids; fill + login as seeded admin? no — TC001 registers `assert_login_user` and reaches home tab (covers register-first E2E loop).
- TC002-TC008, TC010: unchanged semantics via `signed` fixture + `.network()` (Tehran/Mazandaran/Fars counts become 7× as before, kept RELATIVE where already relative).
- TC009 export → unchanged contract under network tab.
- TC011 register: fresh name success (`Registered — log in.`), duplicate name error, empty password error.
- TC012 login: `signin_message()` equals `Wrong password.` / `Unknown username.`; success for e2e_user.
- TC013 logout: `.logout()` lands on signin window; `current_user()` reads empty on new sign-in attempt.
- TC014 dashboard: via `homeCardDashboard`; KPI format + `trend_points() == 7`; `worst_cells_rows() >= 1`.
- TC015 users: `latest` — LOGIN as admin → users tab via `homeCardUsers`; `.users().remove("guest")` works; trying `remove("admin")` shows inline nothing-happens (guard pinned by smoke; the E2E asserts the row REMAINS).

- [ ] **Step 1: rewrite/write tests with exact assertions above (no implementation) → RED** run each subset.
- [ ] **Step 2: adjust DSL/page objects only where probes revealed mismatches (no silent assertion relaxation in tests).**
- [ ] **Step 3: full suite**: `powershell -ExecutionPolicy Bypass -File scripts\run-tests.ps1` → 15 PASS (prepend ctest: `run-ctest.ps1` also).
- [ ] **Step 4: Commit** tests + TC docs: `test: E2E rewrite for auth-first flow; TC011-TC015 added (15 green)`.

### Task 9: Docs, ledger, Allure, final review + push

**Files:**
- Modify: `README.md` (features + auth quickstart note), `AGENTS.md` (new conventions: restore fixture; users/admin special case; chart contract already noted), `docs/env-setup.md` (nothing new pinned — verify), `.superpowers/sdd/.../progress.md` (rulings: register-first scope cut, admin special casing, isolation approach, DSL action/read split rationale)
- Regenerate: `reports/allure-report` (from the Task 8 run).

- [ ] **Step 1: docs + ledger lines.**
- [ ] **Step 2: whole-suite ctest + pytest + `run-allure` regeneration (recorded pass counts in ledger).**
- [ ] **Step 3: fresh reviewer subagent on whole-branch diff; fix pass.**
- [ ] **Step 4: commit `docs: …` + push branch main** (owner confirmation from prior plan: push `origin main`).
