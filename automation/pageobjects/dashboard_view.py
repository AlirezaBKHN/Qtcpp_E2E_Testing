"""Dashboard tab page object: KPI readouts + plain-text caption counts.

All driver access for the DashboardDsl lives here (page objects own the
driver through the resolve_element seam; the DSL only delegates).
"""
from __future__ import annotations

import re
import time

from drivers import keyboard
from pageobjects.date_edit import set_date
from pageobjects.filter_section import FilterSection
from pageobjects.main_page import MainPage
from pageobjects.readout_list import ReadoutList
from pageobjects.resolve_element import find, read_text

KPI_IDS = {
    "avgQuality": "kpiAvgQuality",
    "cells": "kpiCells",
    "bestOperator": "kpiBestOperator",
    "worstOperator": "kpiWorstOperator",
}


def _caption_number(driver, contains: str, pattern: str, fail: str) -> int:
    captions = driver.find_elements(
        "xpath", f"{MainPage.WINDOW_XPATH}//*[contains(@Name,'{contains}')]")
    for el in captions:
        match = re.search(pattern, read_text(el))
        if match:
            return int(match.group(1))
    raise AssertionError(fail)


# Pinned readout formats (plan Global Constraints).
SLICE_RE = re.compile(r"^(.+): (\d+) cells \((\d+\.\d)%\)$")
GROUP_RE = re.compile(r"^(.+): (\d+\.\d)%$")


class DashboardView:
    def __init__(self, driver):
        self._driver = driver
        self._filters = FilterSection(driver, "dashboardFilterSection")

    def kpi(self, name: str) -> str:
        """Text of a KPI readout; keys per KPI_IDS."""
        try:
            control = KPI_IDS[name]
        except KeyError as exc:  # unknown KPI name is a test-author bug
            raise KeyError(
                f"unknown KPI {name!r}; known: {sorted(KPI_IDS)}") from exc
        return read_text(find(self._driver, control))

    def trend_points(self) -> int:
        """Bucket count from the plain-text chart caption ('... - N points').

        The chart itself is a UIA custom control (no points exposed); the
        caption QLabel above it carries the count in its Name.
        """
        return _caption_number(self._driver, "points", r"(\d+)\s*points",
                               "no trend caption with 'N points' found")

    def weakest_rows(self) -> int:
        """Row count from the 'Weakest cells - N rows' caption.

        Counting UIA DataItems is unreliable on virtualized tables (repo
        quirk); the caption makes the count readable and unambiguous
        against the network status bar's 'Showing N rows'.
        """
        return _caption_number(self._driver, "Weakest cells", r"(\d+)\s*rows",
                               "no 'Weakest cells - N rows' caption found")

    # -- filter section (spec 2026-10-20) --------------------------------
    def set_date(self, control: str, date: str) -> None:
        """Set a QDateEdit (YYYY-MM-DD) via the pinned send_keys recipe."""
        set_date(self._driver, control, date)

    def from_date_text(self) -> str:
        """The from-date readout's displayed text ('YYYY-MM-DD')."""
        return read_text(find(self._driver, "dashboardFromDateEdit"))

    def to_date_text(self) -> str:
        """The to-date readout's displayed text ('YYYY-MM-DD')."""
        return read_text(find(self._driver, "dashboardToDateEdit"))

    # -- filter actions ---------------------------------------------------
    def select_combo(self, control: str, value: str) -> None:
        self._filters.select_combo(control, value)

    def search(self) -> None:
        self._filters.click("dashboardSearchButton")

    def reset(self) -> None:
        self._filters.click("dashboardResetButton")

    # -- share chart data surfaces ----------------------------------------
    def list_items(self, control: str) -> list[str]:
        return ReadoutList(self._driver, control).items()

    def pie_slices(self) -> list[tuple[str, int, float]]:
        """Operator pie slices as (label, count, percent) - from the list."""
        slices = []
        for text in self.list_items("operatorShareList"):
            match = SLICE_RE.match(text)
            assert match, f"operatorShareList item format: {text!r}"
            slices.append((match.group(1), int(match.group(2)),
                           float(match.group(3))))
        return slices

    def bar_groups(self) -> list[tuple[str, float]]:
        """Technology bar values as (label, percent) - from the list."""
        groups = []
        for text in self.list_items("techQualityList"):
            match = GROUP_RE.match(text)
            assert match, f"techQualityList item format: {text!r}"
            groups.append((match.group(1), float(match.group(2))))
        return groups

    def caption_text(self, control: str) -> str:
        """A caption's full text (Name = displayed text)."""
        return read_text(find(self._driver, control))