# AGENTS.md — working in this repository

Instructions for AI coding agents (and humans) touching this repo. The PRD lives in the
session handoff; the executed plan is `docs/superpowers/plans/2026-10-19-iran-signal-tracker-e2e.md`,
and its task ledger is `.superpowers/sdd/2026-10-19-iran-signal-tracker-e2e/progress.md`
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
powershell -ExecutionPolicy Bypass -File scripts\run-tests.ps1  # 10 E2E tests + allure-results
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
  `drivers/foreground.py` (ALT-unlock + SetForegroundWindow); combos select via keyboard
  type-ahead — with the table visible, popup items share Names with table cells, so
  element-click matching is ambiguous.
- **Qt runtime must be deployed** (`windeployqt --compiler-runtime` + `Qt6Test.dll` copy).
  `scripts/build.ps1` handles it; without it the exe fails with missing-DLL dialogs when
  launched outside a shell with Qt on PATH — exactly how WinAppDriver launches it.
- **Single source of domain truth** is `app/src/db.cpp` (`kMap` + operator/tech tables). UI and
  `db_test` both derive from it; never duplicate province/city literals elsewhere.
- **Seeder determinism:** fixed `std::mt19937(20241019)`, 1500 rows when the `cells` table is
  absent. Changing seed values invalidates committed `database/signaltracker.db` — regenerate
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
- **Sessions must not leak app windows**: factory kills lingering `IranSignalTracker.exe`
  before opening a session and sets `ms:forcequit`; keep that behavior.

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
