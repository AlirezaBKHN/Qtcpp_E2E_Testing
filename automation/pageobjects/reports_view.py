"""Reports tab page object: filter section + group share pie surfaces.

All driver access for the ReportsDsl additions (spec 2026-10-20) lives
here through the resolve_element seam; the summary table stays in
SummaryTable.
"""
from __future__ import annotations

import re

from pageobjects.date_edit import set_date
from pageobjects.filter_section import FilterSection
from pageobjects.readout_list import ReadoutList
from pageobjects.resolve_element import find, read_text

# Pinned list format: "<label>: <count> rows (P.P%)".
GROUP_SLICE_RE = re.compile(r"^(.+): (\d+) rows \((\d+\.\d)%\)$")


class ReportsView:
    def __init__(self, driver):
        self._driver = driver
        self._filters = FilterSection(driver, "reportsFilterSection")

    # -- filter actions -----------------------------------------------------
    def select_combo(self, control: str, value: str) -> None:
        self._filters.select_combo(control, value)

    def search(self) -> None:
        self._filters.click("reportsSearchButton")

    def reset(self) -> None:
        self._filters.click("reportsResetButton")

    def set_date(self, control: str, date: str) -> None:
        """Set a QDateEdit via the pinned send_keys recipe (Task 7)."""
        set_date(self._driver, control, date)

    def from_date_text(self) -> str:
        return read_text(find(self._driver, "reportsFromDateEdit"))

    def to_date_text(self) -> str:
        return read_text(find(self._driver, "reportsToDateEdit"))

    # -- share chart data surfaces -------------------------------------------
    def group_slices(self) -> list[tuple[str, int, float]]:
        """Group pie slices as (label, row count, percent) - from the list."""
        slices = []
        for text in ReadoutList(self._driver, "groupShareList").items():
            match = GROUP_SLICE_RE.match(text)
            assert match, f"groupShareList item format: {text!r}"
            slices.append((match.group(1), int(match.group(2)),
                           float(match.group(3))))
        return slices

    def caption_text(self, control: str) -> str:
        return read_text(find(self._driver, control))

    def list_items(self, control: str) -> list[str]:
        return ReadoutList(self._driver, control).items()