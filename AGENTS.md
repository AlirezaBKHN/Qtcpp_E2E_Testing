# AGENTS.md — working in this repository

Instructions for AI coding agents (and humans) touching this repo. The PRD lives in the
session handoff; the executed plans are `docs/superpowers/plans/2026-10-19-iran-signal-tracker-e2e.md`
and `docs/superpowers/plans/2026-10-19-iran-signal-tracker-auth-tabs-dashboards.md`,
and their task ledger is `.superpowers/sdd/2026-10-19-iran-signal-tracker-e2e/progress.md`
(git-ignored scratch; recover facts from `git log` if it's gone).

## What this repo is

Qt 6.8.3 Widgets app (`Iran Signal Tracker`, MinGW/CMake/SQLite, demo telecom dataset) plus a
pytest/Appium/WinAppDriver E2E stack. Two independent deliverables:

1. `app/` — the application + `ctest` targets (`app_smoke` UI contract, `app_dbtest` DB layer)
2. `automation/` — the E2E framework: `pytest` (appium server in the loop, `DRIVER_MODE=direct`
   hidden fallback), DSL → page objects → driver factory.

## Commands (use the scripts — they encode the fiddly bits)

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1      # configure+build+windeployqt
powershell -ExecutionPolicy Bypass -File scripts\run-ctest.ps1  # app_smoke + app_dbtest
powershell -ExecutionPolicy Bypass -File scripts\run-appium.ps1 # appium :4723 (keeps console open)
powershell -ExecutionPolicy Bypass -File scripts\run-tests.ps1  # 15 E2E tests + allure-results
powershell -ExecutionPolicy Bypass -File scripts\run-allure.ps1 # report (add -Serve to open)
powershell -ExecutionPolicy Bypass -File scripts\run-app.ps1    # launch the exe
```

C++ toolchain paths are absolute defaults in `scripts/build.ps1` (overridable via
`IST_QT_BIN`, `IST_CMAKE`, `IST_GCC`, `IST_GXX`, `IST_NINJA`, `IST_APP_BUILD`). Python: user
`C:\Users\user\AppData\Local\Programs\Python\Python312\python.exe` if PATH is stale.

## Critical conventions (breaking these breaks the product)

- **Stable identifiers are the contract.** Every interactive control sets BOTH `objectName` and
  `accessibleName` to the same string (see `app/src/mainwindow.cpp`, `makeStable`). New controls
  must follow suit; the list lives in the plan's Global Constraints.
- **DSL is the only language of the tests.** `automation/tests/**` must contain zero
  `find_element`/driver calls — only `tracker.*` (see `automation/dsl/tracker.py` for the frozen
  API). Page objects may use drivers through the single `resolve_element.find()` seam.
- **UIA locator strategy.** Qt exposes `accessibleName` as UIA `Name`; locators are name-first
  (`automation/locators.py`). `AutomationId` is *prefixed* for nested widgets — don't switch
  strategies casually; re-freeze with `automation/tools/inspect_ui.py` (run with Appium up).
- **WAD quirks baked into the code:** XPath has `contains()` but NO `starts-with()` (silent
  empty result); row counts come from the status-bar text `Showing N rows` (UIA virtualizes
  table rows — never count `DataItem`s); **the app must be OS-foreground before any
  interaction** — WAD sends keystrokes to the focused window and clicks to whatever sits at
  the element's screen coordinates, otherwise they land in OTHER applications (observed:
  typed test strings ended up inside a chat window). `launch()` enforces this via
  `drivers/foreground.py` (ALT-unlock + SetForegroundWindow).
- **`send_keys` APPENDS.** `SignInPage.fill()` REPLACES content first (`_set_text`: clear via
  the UIA ValuePattern, keystroke select-all+delete fallback). A second bare send_keys
  accumulates the first attempt's text and the "valid" login submits a mangled username —
  this bit TC012 (phantom login failures).
- **WAD pins a session to ONE top-level window.** Sign-in ⇄ main-window transitions are
  handled by phase attachment: the factory owns the app process (`appTopLevelWindow`), and
  the DSL re-attaches the session to the current window per phase (`_attach_phase`); attach
  invalidates the handle cache. Never try to drive the new window through the old session
  context.
- **Find performance is architecture, not tuning:** every root find re-walks the whole UIA
  tree. Countermeasures (keep them): (1) the app exposes small named sections
  (`filterSection`, `headerSection`, `pagerSection`) and `find_within()` scopes finds into
  them; (2) the per-phase handle cache in `resolve_element` reuses element refs across steps
  within a phase (invalidated on phase attach); (3) the table is PAGINATED (page-size
  selector 50/100/200/1000, default 200; page 1 == the old display cap) — the status bar and
  the CSV export keep the FULL query size. Probed: full tree walks cost ~11s/find on the fat
  table; with pagination + sections the whole probe runs in ~35s.
- **UIA virtualization is total on this table:** only the VISIBLE rows of the current page
  are realized as `DataItem`s (~10 in the default layout — probed 2026-10-20, TC016).
  Never assert page size by counting realized rows; page-size correctness rides on the
  `Page k of n` readout math, while `SignalTable.ids()` (realized ID column, stride 7,
  header excluded) cross-checks the realized window as an ordered slice of the CSV export.
  Small-page realized-row counting beyond `> 0` is NOT valid E2E either.
- **Combos select via popup keyboard type-ahead** (click combo to open the popup, OS-level
  type-ahead + Enter via `drivers/keyboard.py`; zero WAD calls while the popup is open).
  Popup items share Names with table cells, so element-click matching is ambiguous; closed-
  combo type-ahead is superseded by this.
- **Register state machine** (pinned by TC001/TC011): a register SUCCESS returns the dialog
  to the LOGIN screen; a FAILED register stays in register mode. `register()` always
  re-enters register mode via its toggle, so it is only callable from the login screen.
- **Readout contract:** read-only KPI/status readouts keep `objectName` = stable id but set
  `accessibleName` to the DISPLAYED text (`'49.8%'`, `'10080 cells'`, `'Showing N rows'`) —
  the only readable surface WAD offers. `app_smoke` pins it.
- **Per-test DB isolation:** every E2E test runs under the `isolate_db` fixture (committed
  `database/signaltracker.db` restored, `reports/export/*.csv` purged per test). The
  committed DB must stay byte-clean across runs; if it is dirty: `taskkill /F /IM
  IranSignalTracker.exe` FIRST (the app holds the file), then `git checkout --
  database/signaltracker.db`. Register/remove tests rely on it — do not "clean up" data in
  tests.
- **Users tab is admin-gated:** the Users TabItem and its home card are mounted for admin
  sessions only (hidden pages are pruned from attached-session trees — `users()` raises
  unless the tab is mounted). Removing `admin` is refused app-side (guard pinned by smoke;
  TC015 asserts the row REMAINS).
- **Qt runtime must be deployed** (`windeployqt --compiler-runtime` + `Qt6Test.dll` copy).
  `scripts/build.ps1` handles it; without it the exe fails with missing-DLL dialogs when
  launched outside a shell with Qt on PATH — exactly how WinAppDriver launches it.
- **Single source of domain truth** is `app/src/db.cpp` (`kMap` + operator/tech tables). UI and
  `db_test` both derive from it; never duplicate province/city literals elsewhere.
  `queryFilters` orders by `ORDER BY province, id` (spec 2026-10-20): province groups in
  alphabetical blocks, id-ascending within — a total order the pager slices and the CSV
  export mirrors. Pagination windows are defined against THIS order; don't change it
  without re-pinning TC016/TC017.
- **Seeder determinism:** fixed `std::mt19937(20241019)`, 10080 rows over 7 days when the
  `cells` table is absent. Changing seed values invalidates committed `database/signaltracker.db` — regenerate
  via `app_dbgen` (`& app\build-mingw\app_dbgen.exe database\signaltracker.db`).
- **Chart axes are persistent** (created once in `MainWindow` ctor); `refreshChart()` replaces
  only series/categories and **sets categories BEFORE attaching** — an attached category axis
  synced to a series with no categories auto-generates "1..n" placeholder slots. Bars encode a
  **0–100 quality metric** (`(dBm+120)/0.7`), NOT raw dBm: the Qt bar canvas draws bars from
  the implied zero line, so an all-negative dBm domain clips bars+labels at the top edge.
  Labels must use the `"@value"` tag (`"%.1f"` renders literally and is dropped) with an
  explicit black `labelBrush` (theme picks near-white for blue bars). Splitter starts
  380/260 or the chart pane is an unreadable sliver. `app_smoke` pins: 1 series, 1 axis per
  edge, 9pt labels, value labels `@value`/precision 3/black brush, quality=transform of
  `Db::avgBy`, axis 0..100 tickCount 5.
- **Sessions must not leak app windows**: the factory kills lingering `IranSignalTracker.exe`
  before opening a session, owns the app process it launches, and terminates it when the
  driver session quits (`automation/drivers/factory.py`; the appium `ms:forcequit` cap is
  not used — sessions are attached to the factory-owned app window). Keep that behavior.

## Working style for agents

- TDD: write the failing test first (smoke/db_test for C++; pytest for E2E) and *watch it fail*
  before implementing. The plan and ledger record this history.
- Run the full relevant suite (`run-ctest.ps1`, or pytest over all of `tests/`) before claiming
  done — not just the file you touched.
- `app/build-mingw/` is git-ignored; rebuilds are cheap. Kill stale app processes before
  relinking (`scripts/build.ps1` does it; the linker fails with Permission denied otherwise).
- Commits are incremental per feature (see `git log`); keep messages conventional
  (`feat:`, `fix:`, `test:`, `docs:`, `chore:`).
- Environment facts (exact versions, developer mode, ports, WAD JSONWP notes) live in
  `docs/env-setup.md`; update it when you change any pinned tool.
