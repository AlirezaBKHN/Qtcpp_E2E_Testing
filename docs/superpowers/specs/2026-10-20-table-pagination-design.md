# Table Pagination + Row-Id Assertions — Design

Date: 2026-10-20
Status: approved in chat by owner (page-size selector directive + "anything else is ok"; ordering directive below)

## Intent and why

The table widget caps display at 200 rows (owner decision 2026-10-20, kept
for UIA find performance). The owner wants pagination on top of that cap so
the user can reach the rest of the data, with a page-size selector, and a
DSL enrichment so E2E tests can assert on specific row IDs.

Non-goals / anti-goals:
- No jump-to-page input, no arbitrary page sizes — selector offers exactly
  50 / 100 / 200 / 1000 ("that's enough" — owner).
- No change to what the status bar and CSV export report: they keep the
  FULL query size and content (the existing contract; pagination is a
  display concern only).
- No seeder or data changes; committed `database/signaltracker.db` stays
  byte-clean; `app_dbgen` regeneration NOT needed.
- No API change to `Db::queryFilters` signature — pagination slices an
  in-memory copy.

## Owner decisions (binding)

1. **Ordering:** `Db::queryFilters` returns rows **ordered by province,
   then id** (`ORDER BY province, id`). Province blocks, id-ascending
   within each block. This replaces the old `ORDER BY province, city,
   operator, technology` (which had no tiebreaker and could paginate
   non-deterministically). The new order is a total order (id is unique),
   so every page is deterministic across queries and restarts.
2. **Page sizes:** selector offers exactly 50, 100, 200, 1000.
   Default 200 (the old cap value, so existing pins keep their meaning).
3. **Page-size changes and every re-query** (search, reset, refresh,
   filter change) return to page 1.

## Contract (what the product does after this)

- New stable identifiers (objectName == accessibleName via `makeStable`),
  grouped in a small `pagerSection` widget below the table:
  - `pagerSection` (container; keeps UIA finds scoped and fast)
  - `pageSizeCombo` (50/100/200/1000; keyboard type-ahead safe: "100" is
    the first match for "100", "1000" unique when fully typed)
  - `prevPageButton`, `nextPageButton`
  - `pageLabel` — READOUT (read-only, follows the readout contract:
    objectName = id, accessibleName mirrors the displayed text).
- Page label text: `Page %1 of %2` where %2 = ceil(N / pageSize), minimum
  1. Empty result (N = 0) shows `Page 1 of 1`, table empty, both buttons
  disabled.
- Buttons disabled at the edges: prev disabled on page 1, next disabled on
  the last page.
- The status bar keeps `Showing N rows` with the FULL count N; the chart
  keeps aggregating the FULL result; CSV export writes the FULL ordered
  result (same order as the table).

## Implementation shape (builder's starting point)

- `app/src/db.cpp`: change the `queryFilters` ORDER BY to
  `province, id`. Verify `Db::writeExportCsv` emits the same order (it
  should run the same query; if it re-orders, align it — the E2E
  export-slice cross-check below depends on the two orders matching).
- `app/src/mainwindow.cpp`: `onSearch` already fetches the full result for
  status + chart — cache it in a member; keep `m_page` (1-based) and
  `m_pageSize` (default 200). A private `populatePage()` fills the table
  from rows `[(page-1)*size, page*size)` of the cached vector. Page label,
  button enablement and slice all derive from cached rows + page state.
  No SQL on page flips.
- Existing smoke slot `tableDisplayCappedStatusReal` keeps holding
  (default page = 200 rows; the "cap" is now page 1 of the pager).

## Tests (TDD; RED witnessed before each GREEN)

- **db_test**: ordering pin — `queryFilters` output is strictly
  (province, id)-ordered: provinces appear as contiguous alphabetical
  blocks; ids ascending within each block.
- **smoke (app_smoke)**: a `pagerContract()` slot — ids present incl.
  `pageLabel` readout mirroring; default `rowCount == 200` and label
  `Page 1 of 51` (10080 seeded rows); next → `Page 2 of 51`, first
  displayed id equals `Db::queryFilters(...)[200]` id computed in-test;
  prev returns to `Page 1 of 51`; prev disabled on page 1, next disabled
  on the last page; page size 50 → 50 rows, label `Page 1 of 202`; page
  size 1000 → 1000 rows, label `Page 1 of 11`; changing page size resets
  to page 1; empty-result edge (a filter matching nothing) shows
  `Page 1 of 1` with both buttons disabled.
- **E2E TC016 (pagination)**: via `signed` fixture — default page
  (`Page 1 of 51`, 200 rows), next/prev page label + id windows,
  cross-check: export the CSV (full ordered result) and assert page 2's
  FIRST/LAST/spread ids equal the corresponding export rows 200–399 ids;
  page-size selector to 50 → label + realized row count; back to 200;
  FINAL step: page size 1000 (done last — regrows the UIA tree) → label
  `Page 1 of 11` + realized row count.
- **E2E TC017 (row-id DSL)**: `table().should_contain_row(id=X)` with X
  taken from the export's first-page slice; `should_not_contain_row(id=Y)`
  with Y from page 2's slice; an unknown id (e.g. 99999) fails
  `should_contain_row`.
- **DSL additions** (tests stay DSL-only):
  - `NetworkDsl.next_page()` / `.prev_page()` (click the buttons)
  - `.page_label()` (readout read)
  - `.page_size("50"|"100"|"200"|"1000")` (keyboard type-ahead combo via
    the existing `MainPage.select_combo` machinery)
  - `SignalTable.should_contain_row(id=…)` / `.should_not_contain_row(id=…)`
    — reads the realized ID-column cells only (never counts DataItems for
    row counts; row counts still come from the status bar / page label).
    Exact cell-index arithmetic (whether a header cell appears among
    realized DataItems) is a builder detail — verify with
    `automation/tools/inspect_ui.py` against the live tree.

## Docs to update in the same effort

- README (pagination section + new ids), AGENTS.md (convention: pagination
  contract — cap is now page 1; status/export keep full size), new
  `docs/test-cases/TC016.md`, `TC017.md`, ledger rulings in
  `.superpowers/sdd/2026-10-19-iran-signal-tracker-e2e/progress.md`.

## Builder-check resolutions (all settled, none owner-blocking)

1. **"Filter change" means the re-query, not the combo.** The app is
   narrow-then-query (owner directive from the auth plan): filter combos
   stay passive until Search/Refresh/Reset is clicked. Only re-query
   paths (and page-size changes) reset to page 1. Combos do NOT
   auto-re-query.
2. **TC016 export cross-check is SAMPLED, not full-set.** Reading all 200
   realized ids through UIA on the fat tab would cost minutes (repo perf
   history). The page object exposes a sampled id read: TC016 asserts the
   page's FIRST id, LAST id, and ~3 spread ids against the export slice.
   The ordering is a total order, so boundary + spread ids verify both
   membership and order at a fraction of the cost. (My call.)
3. **Page row count E2E**: for SMALL pages (bounded by the page size,
   ≤ 1000) counting the realized rows of the table's element-relative
   subtree is reliable and is the accepted exception to the "never count
   DataItems" rule — that rule exists for the 10,080-row FULL count,
   which still comes only from the status bar / page label. Documented
   here; ledgered as a ruling at execution time. (My call.)
4. **TC016 DOES include a page-size-1000 step, executed LAST** (after
   every other assertion), because it regrows the UIA tree ~7×; the test
   ends there and the next test runs in a fresh app session. (Resolves
   the design's internal contradiction — the tradeoff paragraph stands.)

## Known tradeoff (accepted)

Page size 1000 regrows the UIA tree (~7× find cost vs 200). Tests mostly
exercise 50/200; 1000 is selected only inside TC016's page-size step.
Default stays 200 so the everyday tree stays small.