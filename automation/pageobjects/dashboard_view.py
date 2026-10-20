"""Dashboard tab page object: KPI readouts + plain-text caption counts.

All driver access for the DashboardDsl lives here (page objects own the
driver through the resolve_element seam; the DSL only delegates).
"""
from __future__ import annotations

import re
import time

from drivers import keyboard
from pageobjects.main_page import MainPage
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


class DashboardView:
    def __init__(self, driver):
        self._driver = driver

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
        """Set a QDateEdit (YYYY-MM-DD) via WAD element-targeted send_keys.

        Pinned recipe (fresh-state probe, tools/probe_date_fresh.py E1):
        WAD send_keys UIA-focuses the element, focus lands on the YEAR
        section, and the 8 digits auto-advance year -> month -> day.
        OS-level keybd_event digits DO NOT land (probe E3: value unchanged)
        and are not used. The 'send_keys APPENDS' WAD quirk does not apply
        to section-typed date edits, but the recipe assumes the edit
        carries no in-progress typing state (fresh app or after Reset).
        """
        el = find(self._driver, control)
        el.send_keys(date.replace("-", ""))
        time.sleep(0.3)              # date-changed settle

    def from_date_text(self) -> str:
        """The from-date readout's displayed text ('YYYY-MM-DD')."""
        return read_text(find(self._driver, "dashboardFromDateEdit"))

    def to_date_text(self) -> str:
        """The to-date readout's displayed text ('YYYY-MM-DD')."""
        return read_text(find(self._driver, "dashboardToDateEdit"))