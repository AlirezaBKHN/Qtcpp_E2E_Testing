"""Reports tab DSL: group summary + drill-down + report export."""
from __future__ import annotations

import config as C
from dsl.export_result import ExportResult
from pageobjects.resolve_element import find
from pageobjects.reports_view import ReportsView
from pageobjects.summary_table import SummaryTable

GROUP_COLUMNS = ("Province", "Operator", "Technology")


class ReportsDsl:
    def __init__(self, driver, route_home):
        self._driver = driver
        self._table = SummaryTable(driver)
        self._reports = ReportsView(driver)
        route_home("homeCardReports")

    def _el(self, control: str):
        return find(self._driver, control)

    # -- actions -------------------------------------------------------
    def group_by(self, value: str) -> "ReportsDsl":
        """Select the group column via keyboard type-ahead (WAD quirk)."""
        if value not in GROUP_COLUMNS:
            raise KeyError(
                f"unknown report group {value!r}; known: {sorted(GROUP_COLUMNS)}")
        self._el("reportGroupBy").send_keys(value)
        return self

    def select_row(self, row: int) -> "ReportsDsl":
        """Select a summary row to drive the drill-down trend chart."""
        self._table.click_row(row)
        return self

    # -- filter section (spec 2026-10-20) ---------------------------------
    def province(self, value: str) -> "ReportsDsl":
        self._reports.select_combo("reportsProvinceCombo", value)
        return self

    def city(self, value: str) -> "ReportsDsl":
        self._reports.select_combo("reportsCityCombo", value)
        return self

    def technology(self, value: str) -> "ReportsDsl":
        self._reports.select_combo("reportsTechnologyCombo", value)
        return self

    def operator_name(self, value: str) -> "ReportsDsl":
        self._reports.select_combo("reportsOperatorCombo", value)
        return self

    def from_date(self, value: str) -> "ReportsDsl":
        self._reports.set_date("reportsFromDateEdit", value)
        return self

    def to_date(self, value: str) -> "ReportsDsl":
        self._reports.set_date("reportsToDateEdit", value)
        return self

    def search(self) -> "ReportsDsl":
        self._reports.search()
        return self

    def reset(self) -> "ReportsDsl":
        self._reports.reset()
        return self

    def from_date_text(self) -> str:
        """The reports from-date readout's displayed text."""
        return self._reports.from_date_text()

    def to_date_text(self) -> str:
        """The reports to-date readout's displayed text."""
        return self._reports.to_date_text()

    # -- group share pie (the TEST asserts) --------------------------------
    def group_slices(self) -> list[tuple[str, int, float]]:
        """Group pie slices as (label, row count, percent)."""
        return self._reports.group_slices()

    def caption_text(self, control: str) -> str:
        """A caption's full text ('Rows by Province - 31 slices')."""
        return self._reports.caption_text(control)

    def list_items(self, control: str) -> list[str]:
        """Raw item texts of the groupShareList readout."""
        return self._reports.list_items(control)

    # -- readers (the TEST asserts) ------------------------------------
    def summary_rows(self) -> int:
        """Number of data rows in the summary table."""
        return self._table.row_count()

    def row_label(self, row: int) -> str:
        """Label text of the given summary row."""
        return self._table.row_label(row)

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