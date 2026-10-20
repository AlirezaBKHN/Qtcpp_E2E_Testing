"""Reports summary-table page object (partially virtualized).

Spec 2026-10-20 follow-up: the group-share pie + drill chart share the
tab height with the table, so at FULL province count only the visible
rows are realized (~5-6 DataItems) - row_count() is only trustworthy for
small FILTERED tables (TC023/TC024 use filtered 1-4-row summaries). The
unfiltered row count rides the pinned groupShare caption instead
(TC025); realized-row counting beyond '> 0' is NOT valid E2E (repo
quirk).
"""
from __future__ import annotations

from math import ceil

from pageobjects.resolve_element import find, read_text

COLUMNS_PER_ROW = 3  # Label / Rows / Avg Quality


class SummaryTable:
    def __init__(self, driver):
        self._driver = driver

    def _items(self):
        return find(self._driver, "reportTable").find_elements(
            "xpath", ".//DataItem")

    def _cell(self, row: int):
        items = self._items()
        cell_index = row * COLUMNS_PER_ROW
        if cell_index >= len(items):
            raise AssertionError(
                f"report row {row} not realized; realized cells: {len(items)}")
        return items[cell_index]

    def click_row(self, row: int) -> None:
        """Select a summary row to drive the drill-down trend chart.

        Realized cells are row-major (3 per row, headers excluded); this is
        a 10-row summary table, so all rows stay realized.
        """
        self._cell(row).click()

    def row_count(self) -> int:
        """Number of data rows (ceil of realized cells)."""
        return ceil(len(self._items()) / COLUMNS_PER_ROW)

    def row_label(self, row: int) -> str:
        """Label text of the given summary row."""
        return read_text(self._cell(row))