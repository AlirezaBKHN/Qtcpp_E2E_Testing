# Table Pagination + Row-Id DSL Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Paginate the Network Data table (page-size selector 50/100/200/1000, prev/next, page label) and expose row-id assertions to the E2E DSL.

**Architecture:** `Db::queryFilters` gets a deterministic total order (`ORDER BY province, id` — owner directive). MainWindow caches the full query result it already fetches and slices pages from memory (no SQL paging, no Db API change); a small `pagerSection` widget carries the selector + buttons + readout. The DSL gains pager navigation and realized-id assertions; the status bar and CSV export keep the FULL query (unchanged contract).

**Tech Stack:** Qt 6.8.3 Widgets/Sql (MinGW, CMake), QtTest smoke, pytest + Appium/WinAppDriver, allure.

**Spec:** `docs/superpowers/specs/2026-10-20-table-pagination-design.md` (read both; the spec's Builder-check resolutions are binding).

## Global Constraints

- Stable identifiers: every new control sets BOTH `objectName` and `accessibleName` to the same string via `makeStable` — new ids: `pagerSection`, `pageSizeCombo`, `prevPageButton`, `nextPageButton`, `pageLabel` (pageLabel is a READOUT: read-only QLineEdit, objectName keeps the id, accessibleName mirrors the displayed text — same pattern as `currentUserLabel`).
- Page sizes exactly `50 / 100 / 200 / 1000`; default `200`. `pageLabel` text: `Page %1 of %2`, %2 = max(1, ceil(N / pageSize)).
- Status bar keeps `Showing N rows` with the FULL query count; CSV export writes the FULL ordered result; chart aggregates the FULL result. Pagination never changes them.
- Re-query paths (search, refresh, reset) AND page-size changes return to page 1. Filter combos stay passive until Search (narrow-then-query — no auto-re-query).
- Ordering `ORDER BY province, id` is a total order; `writeExportCsv` iterates `queryFilters` so export order == table order automatically (verified in Task 1).
- DSL-only tests: `automation/tests/**` contains zero `find_element`/driver calls. Page objects use drivers via `resolve_element` (`find`/`find_within`/`read_text`).
- Committed `database/signaltracker.db` stays byte-clean; `reports/export/*.csv` purged per test (`isolate_db` fixture already does both — do not weaken it).
- Seeder untouched; no `app_dbgen` regeneration needed.
- Full verification before claiming done: `pwsh -ExecutionPolicy Bypass -File scripts\run-ctest.ps1` → 2/2, `pwsh -ExecutionPolicy Bypass -File scripts\run-tests.ps1` → 17 PASS (15 existing + TC016 + TC017). Kill stale `IranSignalTracker.exe` before relinking (build.ps1 does it).

## Review Focus

1. **Partial last page** (10080 % 50 = 30 rows on page 202): expect the label `Page 202 of 202`, exactly 30 realized rows, `nextPageButton` disabled. → pinned by smoke step in Task 2.
2. **Empty result** (0 rows after a query): expect `Page 1 of 1`, empty table, both buttons disabled. → pinned by smoke slot `pagerEmptyResultScratch` in Task 2.
3. **Stale page after re-query**: page 2 of the unfiltered view, then Search with the same filters → expect page 1 of the (unchanged) count, no off-by-one rows. → pinned by smoke step in Task 2.
4. **Page-size change while deep in the table**: page 2 at size 1000, then select 50 → expect `Page 1 of 202` (reset), first-page ids. → pinned by smoke step in Task 2.
5. **Type-ahead "100" vs "1000"** in `pageSizeCombo`: typing `100` + Enter must select `100` (first match in item order 50,100,200,1000). → pinned by TC016 selecting `50`, `200`, and `1000` over UIA (100 is covered by "50→200→1000" transitions using full-text type-ahead; if TC016's `1000` step ever lands on `100`, the label assert `Page 1 of 11` fails loudly).

---

### Task 1: Deterministic query order (`ORDER BY province, id`)

**Files:**
- Modify: `app/src/db.cpp:343` (the `sql += " ORDER BY ..."` line inside `Db::queryFilters`)
- Test: `app/tests/db_test.cpp` (add an ordering block inside the existing queryFilters test section, after the Tehran count CHECKs)

**Interfaces:**
- Consumes: `Db::queryFilters(province, city, technology, operatorName) -> FilterBindings` (= `QVector<QVariantList>`, column 0 = id, column 1 = province — unchanged).
- Produces: rows sorted by province ascending, id ascending within each province. Later tasks and `writeExportCsv` (which iterates `queryFilters`) rely on this.

- [ ] **Step 1: Write the failing ordering pin** in `app/tests/db_test.cpp`, in the same CHECK-block style as the existing queryFilters section:

```cpp
// queryFilters order (spec 2026-10-20): provinces in contiguous ascending
// blocks, ids strictly ascending within each block - the total order
// pagination slices (page k = rows [(k-1)*size, k*size)).
{
    const auto rows = Db::queryFilters(QString(), QString(), QString(), QString());
    CHECK(!rows.isEmpty());
    QString prevProvince;
    int prevId = -1;
    bool firstRow = true;
    for (const auto& row : rows) {
        const QString province = row.at(1).toString();
        const int id = row.at(0).toInt();
        if (firstRow || province != prevProvince) {
            if (!firstRow)
                CHECK(province > prevProvince); // contiguous alphabetical blocks
            prevProvince = province;
            prevId = -1;
            firstRow = false;
        } else {
            CHECK(id > prevId); // ascending ids within the block
        }
        prevId = id;
    }
}
```

- [ ] **Step 2: Run** `pwsh -ExecutionPolicy Bypass -File scripts\run-ctest.ps1`
  Expected: db_test FAIL on the `CHECK(id > prevId)` line (current order is province, city, operator, technology — ids not ascending within a province).

- [ ] **Step 3: Implement** — change the ORDER BY line in `Db::queryFilters` to:

```cpp
sql += " ORDER BY province, id";
```

(Owner directive: province groups, id-ordered within. `id` is INTEGER PRIMARY KEY so the order is total and stable. `writeExportCsv` iterates `queryFilters`, so export order aligns automatically — no other code change.)

- [ ] **Step 4: Re-run** `pwsh -ExecutionPolicy Bypass -File scripts\run-ctest.ps1` → 2/2 PASS.

- [ ] **Step 5: Commit**

```bash
git add app/src/db.cpp app/tests/db_test.cpp
git commit -m "feat(db): queryFilters orders by province then id (stable pagination)"
```

---

### Task 2: Pager in MainWindow (smoke RED → GREEN)

**Files:**
- Modify: `app/src/mainwindow.h` (members + slots below), `app/src/mainwindow.cpp` (pager UI + page state + `populatePage()`)
- Modify: `app/src/mainwindow.cpp:286-306` (`onSearch` — cache rows, reset page, call `populatePage()`; the `kMaxDisplayedRows` constant and its comment are REPLACED by the pager state)
- Modify: `app/src/mainwindow.cpp:163-168` (splitter assembly: wrap table + pager in one pane so `setSizes({380, 260})` and the chart pins survive)
- Modify: `app/tests/smoke_main.cpp` (update `tableDisplayCappedStatusReal`'s comment; add slots `pagerContract()` and `pagerEmptyResultScratch()` + declarations)

**Interfaces:**
- Consumes: `Db::queryFilters` (Task 1 order), `makeStable`/`makeCombo` helpers already in mainwindow.cpp, `Db::init`/`Db::defaultDatabasePath` (scratch slot).
- Produces: stable ids `pagerSection`, `pageSizeCombo`, `prevPageButton`, `nextPageButton`, `pageLabel` (readout). `pageSizeCombo` items in order `50, 100, 200, 1000` with `200` selected initially. New private members on MainWindow: `QComboBox* pageSizeCombo; QPushButton* prevPageButton; QPushButton* nextPageButton; QLineEdit* pageLabel; Db::FilterBindings m_rows; int m_page = 1; int m_pageSize = 200;` and `void populatePage(); void onPageSizeChanged(int index); void onNextPage(); void onPrevPage();`

- [ ] **Step 1: Write the failing smoke slots** in `app/tests/smoke_main.cpp` (declarations after `tableDisplayCappedStatusReal`; add `#include <QtSql/QSqlQuery>` and `#include <QtSql/QSqlDatabase>` at the top):

```cpp
// Pager contract (spec 2026-10-20): page-size selector 50/100/200/1000
// (default 200), prev/next with edge disabling, page-label math,
// re-query and page-size changes reset to page 1. The old display cap
// is now simply page 1 of the pager.
void SmokeMain::pagerContract()
{
    MainWindow w(QStringLiteral("admin"));
    w.show();
    QTest::qWait(50);
    auto* table = w.findChild<QTableWidget*>(QString::fromLatin1("signalTable"));
    auto* label = w.findChild<QLineEdit*>(QString::fromLatin1("pageLabel"));
    auto* next = w.findChild<QPushButton*>(QString::fromLatin1("nextPageButton"));
    auto* prev = w.findChild<QPushButton*>(QString::fromLatin1("prevPageButton"));
    QVERIFY(table && label && next && prev);
    QCOMPARE(table->rowCount(), 200);
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 51"));
    QCOMPARE(label->accessibleName(), label->text());
    QVERIFY2(!prev->isEnabled(), "prev must be disabled on page 1");

    next->click();
    QCOMPARE(label->text(), QStringLiteral("Page 2 of 51"));
    const auto rows = Db::queryFilters(QString(), QString(), QString(), QString());
    QCOMPARE(table->item(0, 0)->text(), rows.at(200).at(0).toString()); // first id of page 2
    prev->click();
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 51"));

    // Partial last page: size 50 -> 202 pages, last page 30 rows.
    auto* size = w.findChild<QComboBox*>(QString::fromLatin1("pageSizeCombo"));
    QVERIFY(size != nullptr);
    size->setCurrentText(QStringLiteral("50"));
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 202"));
    QCOMPARE(table->rowCount(), 50);
    for (int i = 0; i < 201; ++i)
        next->click();
    QCOMPARE(label->text(), QStringLiteral("Page 202 of 202"));
    QCOMPARE(table->rowCount(), 30);
    QVERIFY2(!next->isEnabled(), "next must be disabled on the last page");

    // Page-size change resets to page 1 (Review Focus 4).
    size->setCurrentText(QStringLiteral("1000"));
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 11"));
    QCOMPARE(table->rowCount(), 1000);
    next->click();
    QCOMPARE(label->text(), QStringLiteral("Page 2 of 11"));

    // A re-query after paging resets to page 1 (Review Focus 3).
    auto* search = w.findChild<QPushButton*>(QString::fromLatin1("searchButton"));
    QVERIFY(search != nullptr);
    search->click();
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 11"));
    QCOMPARE(table->item(0, 0)->text(), rows.at(0).at(0).toString());
}

// Empty result (Review Focus 2): Page 1 of 1, empty table, both pager
// buttons disabled. Scratch DB emptied of cells; default connection
// restored afterwards.
void SmokeMain::pagerEmptyResultScratch()
{
    const QString scratch = QStringLiteral("smoke_pager_scratch.db");
    QFile::remove(scratch);
    QVERIFY(Db::init(scratch));
    QSqlQuery wipe(QSqlDatabase::database());
    QVERIFY(wipe.exec(QStringLiteral("DELETE FROM cells")));

    MainWindow w(QStringLiteral("admin"));
    w.show();
    QTest::qWait(50);
    auto* table = w.findChild<QTableWidget*>(QString::fromLatin1("signalTable"));
    auto* label = w.findChild<QLineEdit*>(QString::fromLatin1("pageLabel"));
    QVERIFY(table && label);
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 1"));
    QVERIFY(!w.findChild<QPushButton*>(QString::fromLatin1("prevPageButton"))->isEnabled());
    QVERIFY(!w.findChild<QPushButton*>(QString::fromLatin1("nextPageButton"))->isEnabled());

    QFile::remove(scratch);
    QVERIFY(Db::init(Db::defaultDatabasePath())); // restore for later slots
}
```

Also update `tableDisplayCappedStatusReal`'s leading comment: the cap is now page 1 of the pager (default page size 200) — assertions unchanged.

- [ ] **Step 2: Run** `pwsh -ExecutionPolicy Bypass -File scripts\run-ctest.ps1`
  Expected: smoke FAIL — `pageLabel` not found (pager UI does not exist yet).

- [ ] **Step 3: Implement in mainwindow.h/mainwindow.cpp**
  - mainwindow.h: add the members/slots listed under Interfaces (`private slots: onPageSizeChanged(int), onNextPage(), onPrevPage(); private: void populatePage();` + member pointers and `Db::FilterBindings m_rows; int m_page = 1; int m_pageSize = 200;`).
  - ctor, table area: build a `pagerSection` (`makeStable(new QWidget(this), "pagerSection")`) with an QHBoxLayout holding `prevPageButton` (`tr("Prev")`), `pageLabel` readout (same read-only-QLineEdit pattern as `currentUserLabel`), `nextPageButton` (`tr("Next")`), and `pageSizeCombo` (`makeCombo(this, "pageSizeCombo", {"50", "100", "200", "1000"}, false)` then `setCurrentText(QStringLiteral("200"))`). Wrap `signalTable` + `pagerSection` in one pane widget for `splitter->addWidget(...)` (replace the direct `splitter->addWidget(signalTable)`; keep `setSizes({380, 260})`).
  - `populatePage()`: slice `m_rows` from `(m_page-1)*m_pageSize`; fill the table exactly like today's loop (7 columns, `toString()`); label `Page %1 of %2` with %2 = `qMax(1, (m_rows.size() + m_pageSize - 1) / m_pageSize)`; set label text + accessibleName; enable prev iff `m_page > 1`, next iff `m_page < pages`.
  - `onSearch()`: replace the `kMaxDisplayedRows` slice loop with `m_rows = rows; m_page = 1; populatePage();` — the status line keeps `rows.size()` (full count) and `refreshChart()` is unchanged.
  - Slots: `onNextPage()` → `++m_page; populatePage();`, `onPrevPage()` → `--m_page; populatePage();`, `onPageSizeChanged(int)` → set `m_pageSize` from the combo's current text (`toInt()`), `m_page = 1; populatePage();` (Review Focus 4). Connect all four (combo `currentIndexChanged`, buttons `clicked`) in the ctor.
  - Delete the now-unused `kMaxDisplayedRows` constant and its comment block.

- [ ] **Step 4: Re-run** `pwsh -ExecutionPolicy Bypass -File scripts\run-ctest.ps1` → 2/2 PASS (all existing pins included).

- [ ] **Step 5: Commit**

```bash
git add app/src/mainwindow.h app/src/mainwindow.cpp app/tests/smoke_main.cpp
git commit -m "feat(app): paginate the network table (page-size selector 50/100/200/1000, prev/next, page label)"
```

---

### Task 3: DSL pager + row-id assertions, E2E TC016/TC017 (RED → GREEN)

**Files:**
- Modify: `automation/locators.py` (add `"pageLabel"` to `READOUT_IDS`; add `"pagerSection"`, `"pageSizeCombo"`, `"prevPageButton"`, `"nextPageButton"` to the plain CONTROLS list)
- Modify: `automation/pageobjects/main_page.py` (route pager members through `find_within(driver, "pagerSection", ...)`; add `click_prev_page()`, `click_next_page()`, `page_label_text()`, `select_page_size(value)`)
- Modify: `automation/pageobjects/table_widget.py` (add `ids()`, `realized_row_count()`, `should_contain_row(id)`, `should_not_contain_row(id)`)
- Modify: `automation/dsl/network.py` (add `next_page()`, `prev_page()`, `page_label()`, `page_size(value)`; `table()` already returns SignalTable)
- Create: `automation/tests/test_tc016_pagination.py`, `automation/tests/test_tc017_row_ids.py`
- Create: `docs/test-cases/TC016.md`, `docs/test-cases/TC017.md`

**Interfaces:**
- Consumes: `find_within(driver, section, control)` + `read_text(el)` (resolve_element), `MainPage.select_combo(control, value)` (keyboard type-ahead), `ExportResult.newest_file() -> Path | None`, Task 1 order + Task 2 ids.
- Produces (exact DSL API used by TC016/TC017):
  - `NetworkDsl.next_page() -> NetworkDsl`, `.prev_page() -> NetworkDsl` (fluent)
  - `.page_label() -> str` (the pageLabel readout text, e.g. `"Page 2 of 51"`)
  - `.page_size(value: str) -> NetworkDsl` — value in `{"50","100","200","1000"}`, unknown values raise `KeyError` listing the known ones
  - `SignalTable.ids() -> list[int]` (realized ID-column cells of the CURRENT page, row-major stride 7, header row excluded)
  - `SignalTable.realized_row_count() -> int` (`len(self.ids())`)
  - `SignalTable.should_contain_row(id: int) -> SignalTable` / `.should_not_contain_row(id: int) -> SignalTable` (assert over `ids()`; failure message includes the realized ids sample)

- [ ] **Step 1: Write the failing tests** (exact bodies; `csv` and `pathlib` are fine in tests — the DSL-only rule bans DRIVER calls, not file reads):

```python
# automation/tests/test_tc016_pagination.py
# TC016 — Pagination: default page (200 rows, 'Page 1 of 51'), prev/next
# with id windows cross-checked against the CSV export (full ordered
# result), page-size selector, 1000-size step LAST (UIA tree regrowth).
import csv

LARGE = "1000"  # selected LAST: regrows the UIA tree ~7x


def test_tc016_pagination(signed):
    net = signed.network()
    assert net.page_label() == "Page 1 of 51"
    assert net.table().realized_row_count() == 200

    net.next_page()
    assert net.page_label() == "Page 2 of 51"
    result = net.export().should_create_file()
    with open(result.newest_file(), newline="", encoding="utf-8") as fh:
        export_ids = [int(r["id"]) for r in csv.DictReader(fh)]
    page_ids = net.table().ids()
    assert page_ids[0] == export_ids[200]      # first id of page 2
    assert page_ids[-1] == export_ids[399]     # last id of page 2
    assert page_ids[100] == export_ids[300]    # spread id

    net.prev_page()
    assert net.page_label() == "Page 1 of 51"

    net.page_size("50")
    assert net.page_label() == "Page 1 of 202"
    assert net.table().realized_row_count() == 50

    net.page_size("200")
    assert net.page_label() == "Page 1 of 51"

    net.page_size(LARGE)
    assert net.page_label() == "Page 1 of 11"
    assert net.table().realized_row_count() == 1000
```

```python
# automation/tests/test_tc017_row_ids.py
# TC017 — Row-id DSL: should_contain_row / should_not_contain_row over the
# realized ID column; an unknown id must FAIL should_contain_row.
import csv

import pytest


def test_tc017_row_ids(signed):
    net = signed.network()
    result = net.export().should_create_file()
    with open(result.newest_file(), newline="", encoding="utf-8") as fh:
        export_ids = [int(r["id"]) for r in csv.DictReader(fh)]

    page1_ids = net.table().ids()
    assert export_ids[0] in page1_ids  # sanity: page 1 holds the export head

    net.table().should_contain_row(id=export_ids[0])
    net.next_page()
    net.table().should_not_contain_row(id=export_ids[0])
    net.table().should_contain_row(id=export_ids[200])

    net.prev_page()
    with pytest.raises(AssertionError):
        net.table().should_contain_row(id=99999)  # no such cell id
```

Also write `docs/test-cases/TC016.md` and `TC017.md` in the existing catalog format (Title / Preconditions / Steps / Expected Result / Tags: E2E) describing exactly these steps.

- [ ] **Step 2: Run RED** — `pwsh -ExecutionPolicy Bypass -File scripts\run-tests.ps1` runs everything; for the subset: `& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe -m pytest automation\tests\test_tc016_pagination.py automation\tests\test_tc017_row_ids.py -v` (Appium must be up on :4723).
  Expected: both FAIL with `AttributeError: 'NetworkDsl' object has no attribute 'page_label'` (DSL does not exist yet).

- [ ] **Step 3: Implement the page objects + DSL**
  - `locators.py`: `"pageLabel"` into the `READOUT_IDS` tuple; `"pagerSection"`, `"pageSizeCombo"`, `"prevPageButton"`, `"nextPageButton"` into the plain CONTROLS key list (with a one-line comment pointing at the spec).
  - `main_page.py`: `PAGER_SECTION_ID = "pagerSection"` and `_PAGER_MEMBERS = ("pageSizeCombo", "prevPageButton", "nextPageButton", "pageLabel")`; route those through `find_within(self._driver, self.PAGER_SECTION_ID, control)` in `_el` (same caching shape as `_SECTION_MEMBERS`); add `click_prev_page()`, `click_next_page()` (click + return), `page_label_text()` (`read_text` over `_el("pageLabel")`), `select_page_size(value)` (`self.select_combo("pageSizeCombo", value)`).
  - `table_widget.py`: add

    ```python
    def ids(self) -> list[int]:
        """Realized ID-column cells of the CURRENT page (row-major stride
        7, header row excluded). Small bounded read: pages hold at most
        the selected page size."""
        names = self._data_items()
        if names and names[0] == "ID":  # header cell, usersTable-style
            names = names[1:]
        return [int(n) for n in names[0::7]]
    ```

    plus `realized_row_count()` (`len(self.ids())`), `should_contain_row(id)` / `should_not_contain_row(id)` (assert over `self.ids()`; message embeds `self.ids()[:20]`). NOTE: verify the stride/header assumption ONCE against the live tree with `automation/tools/inspect_ui.py` (Appium up, app on the network tab) before trusting `0::7` — adjust the offset if the first realized DataItem is not the first data cell.
  - `network.py`: add

    ```python
    PAGE_SIZES = ("50", "100", "200", "1000")

    def next_page(self) -> "NetworkDsl":
        self._main.click_next_page()
        return self

    def prev_page(self) -> "NetworkDsl":
        self._main.click_prev_page()
        return self

    def page_label(self) -> str:
        return self._main.page_label_text()

    def page_size(self, value: str) -> "NetworkDsl":
        if value not in PAGE_SIZES:
            raise KeyError(f"unknown page size {value!r}; known: {list(PAGE_SIZES)}")
        self._main.select_page_size(value)
        return self
    ```

- [ ] **Step 4: Run GREEN** — the two-test subset command from Step 2 → both PASS. Then the full suite `pwsh -ExecutionPolicy Bypass -File scripts\run-tests.ps1` → **17 PASS** (all 15 previous + TC016 + TC017).

- [ ] **Step 5: Commit**

```bash
git add automation/locators.py automation/pageobjects/main_page.py automation/pageobjects/table_widget.py automation/dsl/network.py automation/tests/test_tc016_pagination.py automation/tests/test_tc017_row_ids.py docs/test-cases/TC016.md docs/test-cases/TC017.md
git commit -m "feat(automation): pager DSL + row-id assertions; TC016/TC017 (17 green)"
```

---

### Task 4: Docs, ledger, whole-suite verification, push

**Files:**
- Modify: `README.md` (pagination section: selector, prev/next, label; "The 17 E2E tests" list + cap paragraph rewritten to "page 1 of the pager")
- Modify: `AGENTS.md` (convention update: the 200-row cap is now page 1 of the pager — status bar/CSV/chart keep the FULL result; small-page realized-row counting is the accepted E2E exception; new ids list; `ORDER BY province, id` note under the db.cpp domain-truth bullet)
- Modify: `.superpowers/sdd/2026-10-19-iran-signal-tracker-e2e/progress.md` (rulings: ordering directive, small-page count exception, sampled export cross-check, 1000-step-last)

**Interfaces:** Consumes Tasks 1–3 as merged. Produces: pushed branch.

- [ ] **Step 1: README + AGENTS.md edits** per Files above (README's "table widget displays at most the first 200 rows" sentence becomes the pagination description; AGENTS.md's "Find performance" bullet's cap clause becomes "the table is PAGINATED (page-size selector 50/100/200/1000, default 200; page 1 == the old cap)" and adds the small-page-count exception).

- [ ] **Step 2: Full verification** — `pwsh -ExecutionPolicy Bypass -File scripts\run-ctest.ps1` (2/2) then `pwsh -ExecutionPolicy Bypass -File scripts\run-tests.ps1` (17 PASS). Then `git checkout -- database/signaltracker.db` after `taskkill /F /IM IranSignalTracker.exe` if dirty (must NOT be dirty — isolate_db holds). Regenerate the report: `pwsh -ExecutionPolicy Bypass -File scripts\run-allure.ps1`.

- [ ] **Step 3: Ledger rulings** in progress.md (the four bullets above, with RED→GREEN witness notes per task).

- [ ] **Step 4: Commit + push** (owner pre-approval: the previous plan's push convention — confirm with the owner before pushing if not yet given for THIS plan)

```bash
git add README.md AGENTS.md
git commit -m "docs: table pagination documentation pass"
git push origin main
```