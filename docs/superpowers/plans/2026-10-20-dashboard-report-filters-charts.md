# Dashboard/Report Filters + Pie/Bar Charts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement
> this plan task-by-task (owner ruled native execution). Steps use checkbox (`- [ ]`)
> syntax for tracking.

**Goal:** Dashboard and Reports tabs get their own filter sections (province/city/
technology/operator + date from–to) that drive every widget on the tab, plus new
charts (operator pie + technology bar on Dashboard; group-share pie on Reports)
verifiable end-to-end through readouts — demonstrated by TC020–TC025.

**Architecture:** DB layer gains defaulted date-range params (`QDate from = {},
QDate to = {}`) on every aggregate plus `countBy`/`dataEra`; the views hold their
own filter state in widgets (combos + `QDateEdit`s) and `refresh()` reads that
state — no duplicated filter structs. Every new chart pairs with a caption
(`Name` = text, carries the count) and a one-column `QListWidget` readout whose
item text is the pinned data format, because WAD cannot see inside chart series.
E2E reads only those surfaces; date edits are typed via OS keyboard digits
(probe-first).

**Tech Stack:** Qt 6.8.3 Widgets + Qt Charts (`QPieSeries`, `QBarSeries`), SQLite
(QSQLITE), pytest + Appium/WinAppDriver, existing DSL → page objects →
`resolve_element` architecture.

**Spec:** This plan is the spec (design agreed in grilling session 2026-10-20;
owner approved). Companion TC specs: `docs/test-cases/TC020.md`–`TC025.md`
(written in Task 9).

## Global Constraints

- **Stable identifiers:** every interactive control sets BOTH `objectName` and
  `accessibleName` to the same id (`makeStable`). New ids, verbatim:
  - Dashboard: `dashboardFilterSection`, `dashboardProvinceCombo`,
    `dashboardCityCombo`, `dashboardTechnologyCombo`, `dashboardOperatorCombo`,
    `dashboardFromDateEdit`, `dashboardToDateEdit`, `dashboardSearchButton`,
    `dashboardResetButton`, `operatorShareCaption`, `operatorShareChart`,
    `operatorShareList`, `techQualityCaption`, `techQualityChart`,
    `techQualityList`
  - Reports: `reportsFilterSection`, `reportsProvinceCombo`, `reportsCityCombo`,
    `reportsTechnologyCombo`, `reportsOperatorCombo`, `reportsFromDateEdit`,
    `reportsToDateEdit`, `reportsSearchButton`, `reportsResetButton`,
    `groupShareCaption`, `groupShareChart`, `groupShareList`
- **Readout contract:** captions and the date edits keep `objectName` = stable id
  but set `accessibleName` to the DISPLAYED text. `QListWidget` items: their text
  is what UIA exposes as the item's `Name` (no override needed).
- **Caption formats (pinned, verbatim):**
  - `operatorShareCaption` → `Cells by operator - N slices`
  - `techQualityCaption` → `Avg quality by technology - N groups`
  - `groupShareCaption` → `Rows by <Group> - N slices` (e.g. `Rows by Province - 31 slices`)
  - Existing `trendCaption` (`Daily quality trend - N points`) and `worstCaption`
    (`Weakest cells - N rows`) keep their formats, N now = filtered count.
- **Readout list item formats (pinned, verbatim):**
  - `operatorShareList` items → `<label>: <count> cells (P.P%)` e.g. `MCI: 3600 cells (35.7%)`
  - `techQualityList` items → `<label>: Q.Q%` e.g. `5G: 84.2%`
  - `groupShareList` items → `<label>: <count> rows (P.P%)` e.g. `Tehran: 1440 rows (14.3%)`
  - Percent = share of the filtered total, one decimal, `QString::number(x, 'f', 1)`.
- **Chart contract (extended to the new charts):** persistent axes where the chart
  has axes (created once in the ctor); series replaced per refresh; points/
  categories set BEFORE attaching; bar labels use the `@value` tag with precision 3
  and an explicit black `labelBrush`; quality values always through `Db::qualityFrom`
  (never inline `(dBm+120)/0.7`). Pie charts have no axes: `removeAllSeries()` then
  a fresh `QPieSeries` per refresh; every slice label visible with black label brush.
- **Filters:** same four combo populations as the network tab (`makeCombo` pattern:
  `All` first, then `Db::provinces()` / `Db::citiesFor(province)` / technology
  2G/3G/4G/5G / operator MCI/Irancell/Rightel/Shatel Mobile). City cascades from
  province; changing province repopulates city back to `All`. `All` = no filter.
  Filters apply ONLY on the tab's Search click; Reset restores `All` + era-default
  dates then refreshes. Network tab is FROZEN — no changes to its widgets, TCs,
  or `ORDER BY province, id`.
- **Date semantics:** inclusive day range via `substr(sample_time, 1, 10) BETWEEN
  :from AND :to` (ISO text compares lexicographically); empty/invalid `QDate` =
  unbounded side. Defaults = `Db::dataEra()` (MIN/MAX day in the working db);
  invalid era (empty db) → `2000-01-01` / `2100-01-01`.
- **DSL is the only language of the tests:** `automation/tests/**` contains zero
  driver calls; page objects use drivers only through `resolve_element.find()/
  find_within()`. DSL changes are ADDITIVE only.
- **WAD quirks kept:** app must be OS-foreground before interaction; `send_keys`
  APPENDS (date edits are typed via `drivers/keyboard.py` OS keystrokes, never
  WAD `send_keys`); combo popup selection = click → `keyboard.type_text` + Enter;
  find performance via small named sections (`find_within` into
  `dashboardFilterSection`/`reportsFilterSection`).
- **Fixtures are sacred:** do not regenerate `database/fixtures/*`; `dataEra()`
  reads whatever the working db holds. Per-test DB isolation (`isolate_db`) keeps
  working unchanged. TC019 stays the intentional demo failure.
- **TDD:** failing smoke/db_test/pytest first, watch it fail, minimal implement,
  watch it pass; run the full relevant suite (`scripts\run-ctest.ps1`, full
  pytest over `automation/tests`) before claiming done. Commits incremental,
  conventional (`feat:`, `test:`, `docs:`).

## Review Focus

- **Out-of-era date range yields 0 rows** — every widget must degrade to the pinned
  empty state (`0 cells`, `- 0 slices`, `- 0 groups`, `- 0 points`, `- 0 rows`),
  not crash or show stale data. Pinned by smoke `dashboardContract` + TC025.
- **Percent rounding drift** — rounded one-decimal percents may not sum to exactly
  100; smoke pins `|sum - 100| <= 0.05 * slices + 0.05`, not exact 100.
- **Filter state leakage across tabs** — Dashboard/Reports/Network filters are three
  independent states; switching tabs must never apply or clear another tab's
  filters. Pinned by TC025.
- **Date edit typing lands in the wrong section** — a WAD click may focus the day
  section instead of year; the probe task (Task 7) pins the exact keystroke recipe
  (arrow-left clamp + digits) BEFORE any TC depends on it.
- **Best/worst KPIs with zero groups** — filtered result with no operator rows must
  leave the KPI readouts empty (`""`), not `""`-prefixed garbage. Pinned by smoke
  `dashboardContract`.

---

### Task 1: DB layer — date ranges, `countBy`, `dataEra`, filtered `worstCells`

**Files:**
- Modify: `app/src/db.h`
- Modify: `app/src/db.cpp` (queryFilters ~331, avgBy ~377, dailyQuality ~274,
  networkQuality ~256, cellCount ~266, worstCells ~308)
- Test: `app/tests/db_test.cpp`

**Interfaces:**
- Produces (Task 2–5 consume these exact signatures):
  - `static FilterBindings queryFilters(const QString& province, const QString& city,
    const QString& technology, const QString& operatorName,
    const QDate& from = {}, const QDate& to = {});`
  - `static QVector<LabelAvg> avgBy(const QString& groupColumn, const QString& province,
    const QString& city, const QString& technology, const QString& operatorName,
    const QDate& from = {}, const QDate& to = {});`
  - `static double networkQuality(const QDate& from = {}, const QDate& to = {});`
  - `static int cellCount(const QDate& from = {}, const QDate& to = {});`
  - `static QVector<LabelAvg> dailyQuality(const QString& groupColumn,
    const QString& groupValue, const QDate& from = {}, const QDate& to = {});`
  - `static QVector<QStringList> worstCells(int limit = 8, const QString& province = {},
    const QString& city = {}, const QString& technology = {},
    const QString& operatorName = {}, const QDate& from = {}, const QDate& to = {});`
  - `static QVector<QPair<QString, int>> countBy(const QString& groupColumn,
    const QString& province, const QString& city, const QString& technology,
    const QString& operatorName, const QDate& from = {}, const QDate& to = {});`
    (groupColumn whitelisted province/operator/technology; `GROUP BY` + `COUNT(*)`,
    ascending by label)
  - `static QPair<QDate, QDate> dataEra();` — `(MIN, MAX)` of
    `substr(sample_time, 1, 10)` as QDates; invalid dates when the table is empty.
- Consumes: existing `FilterBindings`, `LabelAvg`, `qualityFrom`.

- [ ] **Step 1: Write the failing db_test cases** in `app/tests/db_test.cpp`,
  following the file's existing test style (seeded DB fixture):
  - `dateRangeSplitsTotal`: `cellCount() == cellCount(era.first, mid) + cellCount(mid.addDays(1), era.last)` where `era = Db::dataEra()` and `mid = era.first.addDays(3)`; and `queryFilters("", "", "", "", era.first, mid).size() + queryFilters("", "", "", "", mid.addDays(1), era.last).size() == queryFilters().size()`.
  - `dateBoundsInclusive`: rows on the boundary day are counted by both a
    `[day, day]` range query and a same-day `queryFilters` count for that day
    (compute the day from any row's `sample_time`).
  - `unboundedEqualsFull`: `cellCount({}, {}) == cellCount()` and
    `avgBy("operator") == avgBy("operator", "", "", "", "", {}, {})`.
  - `provinceAndDateCombined`: `queryFilters("Fars", "", "", "", era.first, era.last).size() <= queryFilters("Fars").size()` and equals it (era covers all rows).
  - `countByMatchesQueryFilters`: for each label `l` in `countBy("operator")`:
    `l.second == queryFilters("", "", "", l.first).size()`; same for technology.
  - `dataEraMatchesRows`: `era.first` <= every row's day <= `era.second` and spans exactly 7 days on the seeded fixture.
  - `worstCellsFiltered`: `worstCells(8, "Fars")` rows all have province `Fars`,
    and `worstCells(8, "", "", "", "", era.first, mid).size() <= worstCells(8).size()`.
  - `dailyQualityDateRange`: `dailyQuality("", "", mid.addDays(1), {}).size() <= dailyQuality().size()`.
- [ ] **Step 2: Run `powershell -ExecutionPolicy Bypass -File scripts\run-ctest.ps1`** —
  Expected: db_test FAILS to compile (signatures don't exist yet) — the failing
  state.
- [ ] **Step 3: Implement** in `db.h`/`db.cpp`: add the defaulted params; one
  private helper `static void appendDateClause(const QDate& from, const QDate& to,
  QStringList& clauses)` emitting `substr(sample_time, 1, 10) >= :from` /
  `<= :to` clauses; bind only when the clause was appended (mirror the existing
  `clauses.contains(...)` binding pattern); keep `ORDER BY province, id` untouched;
  `worstCells` builds its WHERE from the existing strength predicate + the same
  column-clause pattern used in `queryFilters`; `dataEra()` selects MIN/MAX day.
- [ ] **Step 4: Run `scripts\run-ctest.ps1`** — Expected: app_dbtest PASS (and
  app_smoke unchanged-pass).
- [ ] **Step 5: Commit** `feat: db date-range filters, countBy and dataEra`

### Task 2: Dashboard filter section + filter-aware refresh

**Files:**
- Modify: `app/src/dashboard_view.h`, `app/src/dashboard_view.cpp`
- Test: `app/tests/smoke_main.cpp` (`dashboardContract`)

**Interfaces:**
- Consumes: Task 1 signatures. Produces: `DashboardView` whose every widget
  reflects the widget-held filter state; stable ids per Global Constraints;
  `refresh()` (no-arg, unchanged signature) now reads the widgets.

- [ ] **Step 1: Write the failing smoke assertions** in `dashboardContract()`:
  all nine `dashboard*` widgets + captions/lists exist (find by objectName via
  `QTest`/`findChild` as the file already does); date-edit `accessibleName`
  equals the displayed date text and defaults equal `Db::dataEra()`; with
  province `Fars` programmatically set on the combo + `refresh()` (call the
  private path via the Search button `click()`): `kpiCells->accessibleName()`
  equals `QString::number(Db::queryFilters("Fars").size()) + " cells"`;
  `trendCaption`/`worstCaption` numbers <= their unfiltered values; out-of-era
  dates (from/to = 2020-01-01/2020-01-02) + refresh leave `kpiCells` == `0 cells`
  and the best/worst operator readouts EMPTY (zero-group guard, never garbage).
- [ ] **Step 2: Run ctest** — Expected: smoke `dashboardContract` FAILS (missing widgets).
- [ ] **Step 3: Implement** in the ctor: a `makeStable`-named
  `dashboardFilterSection` container (QHBoxLayout) at the TOP of the layout with
  the four combos (mirror `makeCombo`/`repopulateCombo` from
  `mainwindow.cpp:47-59,88-92`), two `QDateEdit`s (`calendarPopup` disabled,
  display format `yyyy-MM-dd`, defaults from `Db::dataEra()` with the invalid-era
  fallback, `accessibleName` updated on `dateChanged`), Search/Reset buttons;
  city cascade on province change (reset city to All); Search → `refresh()`;
  Reset → combos to All, dates to era, `refresh()`. Rework `refresh()` to read
  combo state (`All`→empty), the date edits, and pass them to the Task-1 Db
  calls; KPI best/worst loop guards the zero-group case (leave readouts empty).
- [ ] **Step 4: Run ctest** — Expected: PASS.
- [ ] **Step 5: Commit** `feat: dashboard filter section with date range`

### Task 3: Dashboard operator pie chart + readouts

**Files:**
- Modify: `app/src/dashboard_view.h`, `app/src/dashboard_view.cpp`
- Test: `app/tests/smoke_main.cpp` (`dashboardContract`)

**Interfaces:**
- Consumes: `Db::countBy("operator", ...)` (Task 1). Produces widgets
  `operatorShareCaption`/`operatorShareChart`/`operatorShareList` with the pinned
  caption/list formats.

- [ ] **Step 1: Failing smoke assertions**: caption text matches
  `Cells by operator - N slices` where `N == Db::countBy("operator", <current
  widget filters>).size()`; each list item matches
  `^(.+): (\d+) cells \((\d+\.\d)%\)$`; the parsed counts equal `countBy`
  values; percents sum to 100 within `0.05 * N + 0.05`; chart has exactly one
  series of type `QPieSeries` with `N` slices, each slice label visible and
  `labelBrush` black.
- [ ] **Step 2: Run ctest** — Expected: FAIL.
- [ ] **Step 3: Implement**: caption `QLabel`, `QChartView` + `QPieSeries` from
  `countBy` (slice label `<label> P.P%` visible, black brush, value = cell count),
  one-column `QListWidget` with the formatted item strings; placed under the
  weakest-cells table; `makeStable` on chart, readout contract (`accessibleName`
  = text) on the caption. Refresh alongside everything else in `refresh()`.
- [ ] **Step 4: Run ctest** — Expected: PASS.
- [ ] **Step 5: Commit** `feat: dashboard operator share pie with readable slices`

### Task 4: Dashboard technology bar chart + readouts

**Files:**
- Modify: `app/src/dashboard_view.h`, `app/src/dashboard_view.cpp`
- Test: `app/tests/smoke_main.cpp` (`dashboardContract`)

**Interfaces:**
- Consumes: `Db::avgBy("technology", ...)` + `Db::qualityFrom`. Produces
  `techQualityCaption`/`techQualityChart`/`techQualityList`.

- [ ] **Step 1: Failing smoke assertions**: caption `Avg quality by technology -
  N groups`, `N == avgBy("technology", filters).size()`; list items match
  `^(.+): (\d+\.\d)%$` with values == `qualityFrom(avgBy(...))` to 1 decimal;
  chart contract: exactly 1 `QBarSeries` with 1 `QBarSet`, persistent
  `QBarCategoryAxis` + `QValueAxis` (range 0–100, tickCount 5), category labels ==
  the technology labels, bar labels visible with `@value` format, precision 3,
  black label brush.
- [ ] **Step 2: Run ctest** — Expected: FAIL.
- [ ] **Step 3: Implement**: mirror `mainwindow.cpp:438-491 refreshChart` exactly
  (persistent axes in ctor, categories before attach, `@value`, black brush),
  caption + `QListWidget` per the pinned formats.
- [ ] **Step 4: Run ctest** — Expected: PASS.
- [ ] **Step 5: Commit** `feat: dashboard avg-quality-by-technology bar chart`

### Task 5: Reports filter section + group-share pie

**Files:**
- Modify: `app/src/reports_view.h`, `app/src/reports_view.cpp`
- Test: `app/tests/smoke_main.cpp` (`reportsContract`)

**Interfaces:**
- Consumes: Task 1 signatures; `countBy`/`avgBy`/`queryFilters`/
  `dailyQuality` with filters. Produces: `reportsFilterSection` widgets,
  `groupShareCaption`/`groupShareChart`/`groupShareList`; `refreshSummary()`
  now honors widget-held filters; export unchanged (writes the filtered table).

- [ ] **Step 1: Failing smoke assertions** in `reportsContract()`: the nine
  `reports*` filter widgets exist with the same defaulting/cascade/date rules as
  Task 2; summary rows == `Db::avgBy(<group col>, <filters>)` labels ascending
  with `Rows` column == per-label `countBy`/`queryFilters` counts under the same
  filters; `groupShareCaption` matches `Rows by <Group> - N slices`; list items
  match `^(.+): (\d+) rows \((\d+\.\d)%\)$` and the counts equal the summary
  table's Rows column; pie contract as Task 3 (slices = summary labels).
- [ ] **Step 2: Run ctest** — Expected: FAIL.
- [ ] **Step 3: Implement**: filter section at the top (mirror Task 2's build
  but with `reports*` ids), Search/Reset wiring, `refreshSummary()` reads widget
  filters into every Db call (keep label-ascending order and the existing
  `setSortingEnabled` dance), new pie block after the summary table using
  `countBy(groupColumn, filters)` with the pinned caption/list formats, drill
  chart keeps working via `dailyQuality(column, label, from, to)`.
- [ ] **Step 4: Run ctest** — Expected: PASS.
- [ ] **Step 5: Commit** `feat: reports filter section and group share pie`

### Task 6: Full ctest gate

- [ ] **Step 1: Run `powershell -ExecutionPolicy Bypass -File scripts\run-ctest.ps1`**
  — Expected: app_smoke + app_dbtest ALL PASS.

### Task 7: E2E probe — WAD date-edit typing

**Files:**
- Modify: `automation/drivers/keyboard.py` (add `press_left()`, `press_right()`
  using `_VK_LEFT = 0x25` / `_VK_RIGHT = 0x27` through `_press`)
- Create: `automation/tests/test_probe_date_edit.py` (scratch probe, NOT in
  suites.json)

**Interfaces:**
- Produces: the pinned keystroke recipe for setting a date edit, recorded in the
  test's docstring and used by Task 8's page object: click `dashboardFromDateEdit`
  → clamp to the first section (arrow-left presses) → `keyboard.type_text`
  digits `YYYYMMDD` → verify via `read_text`/Name that the displayed text is the
  expected date.

- [ ] **Step 1: Write the probe test**: launch via the shared fixtures, route to
  Dashboard, read the default date text, set `2026-01-15` by keyboard, assert the
  readout now exposes `2026-01-15` (accessibleName mirrors text). If typing alone
  mis-lands, iterate the recipe (arrow count) inside this task only.
- [ ] **Step 2: Run it standalone** (`pytest automation/tests/test_probe_date_edit.py -v`,
  Appium per `scripts\run-tests.ps1` pre-flight or `scripts\run-appium.ps1`) —
  Expected: PASS, recipe recorded.
- [ ] **Step 3: Commit** `test: pin WAD date-edit keystroke recipe` (probe file
  stays untracked in suites; commit it for the record).

### Task 8: E2E DSL + page objects (additive)

**Files:**
- Create: `automation/pageobjects/filter_section.py` — `FilterSection(driver,
  section_id, member_ids)` with `select_combo` (click → `keyboard.type_text` +
  Enter, copied verbatim from `MainPage.select_combo` semantics), `click_search`,
  `click_reset`; scoped `find_within(section_id, control)` lookups.
- Create: `automation/pageobjects/readout_list.py` — `ReadoutList(driver,
  control_id)` with `items()` → ordered list of item `Name` texts (XPath
  descendant `ListItem` find scoped to the list element; assert realized count ==
  caption N when both are used together).
- Modify: `automation/pageobjects/dashboard_view.py` — add
  `DASH_SECTION_IDS`, filter/date/search/reset methods backed by `FilterSection`,
  `from_date_text()`/`to_date_text()` readers, `set_date(control_id, "YYYY-MM-DD")`
  using the Task-7 recipe, `list_items(control_id)` via `ReadoutList`, format
  parsing helpers: `pie_slices(key)` → `[(label, count:int, pct:float)]` for
  `operatorShare`, `bar_groups()` → `[(label, pct:float)]`.
- Modify: `automation/pageobjects/reports_view.py` — Create if absent (check:
  SummaryTable exists; reports view object may be inline) — same filter section
  methods with `reports*` ids and `group_slices()` → `[(label, count:int,
  pct:float)]`.
- Modify: `automation/dsl/dashboard.py` — additive:
  `province/city/technology/operator_name(v)`, `from_date("YYYY-MM-DD")`,
  `to_date(...)`, `search()`, `reset()`, `from_date_text()`, `to_date_text()`,
  `pie_slices()`, `bar_groups()`, `list_items(key)`, `caption_text(key)`.
- Modify: `automation/dsl/reports.py` — additive: same filter setters +
  `group_slices()`, `caption_text(key)`.
- Test: no new test file yet (Task 9 consumes); sanity via import +
  a temporary pytest smoke of `tracker.dashboard().pie_slices()` may be run
  manually.

- [ ] **Step 1: Write the page objects + DSL methods** (no app changes).
- [ ] **Step 2: Sanity run** an existing dashboard test (`pytest
  automation/tests/test_tc014_dashboard.py -v`) — Expected: PASS (additive-only,
  no regressions).
- [ ] **Step 3: Commit** `feat: dashboard/report filter + chart DSL`

### Task 9: TC020–TC025 + suites + TC docs

**Files:**
- Create: `automation/tests/test_tc020_dashboard_filters.py`,
  `test_tc021_dashboard_pie_vs_csv.py`, `test_tc022_dashboard_tech_bar.py`,
  `test_tc023_reports_filters_pie.py`, `test_tc024_reports_drill_filtered.py`,
  `test_tc025_cross_tab_isolation.py`
- Modify: `automation/suites.json` — new suite `dashboard`:
  `[test_tc020_dashboard_filters, test_tc021_dashboard_pie_vs_csv,
  test_tc022_dashboard_tech_bar]`; `reporting` gains the tc023–tc025 files.
- Create: `docs/test-cases/TC020.md`–`TC025.md` (match existing TC doc format).

**Interfaces:**
- Consumes: full DSL surface (Task 8), network `export()` for CSV cross-checks.

TC contracts (each test file follows the existing header-comment + fixture style,
`isolate_db` automatic):

- [ ] **Step 1: Write TC020** — set Dashboard filters (province Fars + full-era
  dates), Search: `kpi("cells")` == network-tab cross-check (same filters set on
  Network tab → CSV export row count); trend_points() <= 7 and > 0.
- [ ] **Step 2: Write TC021** — Dashboard filters (province Tehran), Search:
  `pie_slices()` counts sum to the CSV-derived operator distribution from a
  Network-tab export under the SAME filters (parse `export_*.csv`, group by
  operator column); per-slice counts equal CSV counts; caption N == slice count.
- [ ] **Step 3: Write TC022** — Dashboard Search default: `bar_groups()` labels ==
  the 4 technologies; values == CSV-derived avg quality through the `(dBm+120)/0.7`
  transform, to 1 decimal, per label.
- [ ] **Step 4: Write TC023** — Reports filters (operator Irancell) + `group_by("Technology")`:
  `summary_rows()` == CSV-derived technology count under the same filters;
  `group_slices()` counts == per-technology CSV counts.
- [ ] **Step 5: Write TC024** — Reports filters (province Tehran), `group_by("Operator")`,
  `select_row(0)`: `dailyTrendChart` visible (reuse the TC010 visibility pattern);
  `row_label(0)` is the expected first label ascending; reports `export()` file
  content rows == filtered summary rows.
- [ ] **Step 6: Write TC025** — Dashboard `from_date("2020-01-01")/to_date("2020-01-02")`
  + Search: pinned empty state (`kpi("cells") == "0 cells"`, caption Ns are 0,
  lists empty); then set Dashboard province Tehran, switch to Reports: Reports
  summary still shows the FULL baseline rows (no leakage); and Network tab
  `row_count()` unchanged from its baseline.
- [ ] **Step 7: Run each new TC** (Appium up; full `pytest
  automation/tests/test_tc02*.py -v`) — Expected: PASS after fixes; iterate
  only within each TC's own step.
- [ ] **Step 8: Update `automation/suites.json` + write the six TC docs.**
- [ ] **Step 9: Full-suite gate**: `powershell -ExecutionPolicy Bypass -File
  scripts\run-tests.ps1 -Suite all` — Expected: 30 passed, 1 failed (TC019
  intentional). Commit `test: TC020-TC025 dashboard/report filters and charts`
  then `docs: TC020-TC025 specs` (or one commit each, conventional style).