"""Reports tab DSL: group summary + drill-down + report export."""
from __future__ import annotations

from math import ceil

import config as C
from dsl.export_result import ExportResult
from pageobjects.main_page import MainPage
from pageobjects.resolve_element import find
from pageobjects.signin_page import _read_text

COLUMNS_PER_ROW = 3  # Label / Rows / Avg Quality


class ReportsDsl:
    def __init__(self, driver, route_home):
        self._driver = driver
        self._main = MainPage(driver)
        route_home("homeCardReports")

    def _el(self, control: str):
        return find(self._driver, control)

    # -- actions -------------------------------------------------------
    def group_by(self, value: str) -> "ReportsDsl":
        """Select the group column via keyboard type-ahead (WAD quirk)."""
        mapping = {"Province": "Province", "Operator": "Operator",
                   "Technology": "Technology"}
        try:
            text = mapping[value]
        except KeyError as exc:
            raise KeyError(
                f"unknown report group {value!r}; known: {sorted(mapping)}") from exc
        self._el("reportGroupBy").send_keys(text)
        return self

    def select_row(self, row: int) -> "ReportsDsl":
        """Select a summary row to drive the drill-down trend chart.

        Realized cells are row-major (3 per row, headers excluded); this is
        a 10-row summary table, so all rows stay realized.
        """
        table = self._el("reportTable")
        items = table.find_elements("xpath", ".//DataItem")
        cell_index = row * COLUMNS_PER_ROW
        if cell_index >= len(items):
            raise AssertionError(
                f"report row {row} not realized; realized cells: {len(items)}")
        items[cell_index].click()
        return self

    # -- readers (the TEST asserts) ------------------------------------
    def summary_rows(self) -> int:
        """Number of data rows in the summary table (ceil of realized cells)."""
        table = self._el("reportTable")
        items = table.find_elements("xpath", ".//DataItem")
        return ceil(len(items) / COLUMNS_PER_ROW)

    def row_label(self, row: int) -> str:
        """Label text of the given summary row."""
        table = self._el("reportTable")
        items = table.find_elements("xpath", ".//DataItem")
        cell_index = row * COLUMNS_PER_ROW
        if cell_index >= len(items):
            raise AssertionError(
                f"report row {row} not realized; realized cells: {len(items)}")
        return _read_text(items[cell_index])

    # -- export ---------------------------------------------------------
    def export(self) -> ExportResult:
        """Report CSV via the silent writer; follows the same watcher."""
        C.EXPORT_DIR.mkdir(parents=True, exist_ok=True)
        before = {p.name for p in C.EXPORT_DIR.glob("report_*.csv")}
        self._el("exportReportButton").click()
        return ReportsExportResult(C.EXPORT_DIR, before)


class ReportsExportResult(ExportResult):
    """ExportResult over report_*.csv files instead of export_*.csv."""

    def _current(self) -> set[str]:
        self._dir.mkdir(parents=True, exist_ok=True)
        return {p.name for p in self._dir.glob("report_*.csv")}

    def should_create_file(self) -> "ReportsExportResult":
        new_files = self._current() - self._before
        assert new_files, (
            f"no new report_*.csv appeared in {self._dir}; "
            f"files before: {sorted(self._before)}")
        return self
