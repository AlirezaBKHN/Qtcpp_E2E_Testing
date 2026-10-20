"""Dashboard tab DSL: KPI readouts, trend granularity, weakest cells."""
from __future__ import annotations

import re

from pageobjects.main_page import MainPage
from pageobjects.resolve_element import find
from pageobjects.signin_page import _read_text

KPI_IDS = {
    "avgQuality": "kpiAvgQuality",
    "cells": "kpiCells",
    "bestOperator": "kpiBestOperator",
    "worstOperator": "kpiWorstOperator",
}


class DashboardDsl:
    def __init__(self, driver, route_home):
        self._driver = driver
        self._main = MainPage(driver)
        route_home("homeCardDashboard")

    def _el(self, control: str):
        return find(self._driver, control)

    # -- readers (the TEST asserts) ------------------------------------
    def kpi(self, name: str) -> str:
        """Text of a KPI readout; keys per KPI_IDS."""
        try:
            control = KPI_IDS[name]
        except KeyError as exc:  # unknown KPI name is a test-author bug
            raise KeyError(
                f"unknown KPI {name!r}; known: {sorted(KPI_IDS)}") from exc
        return _read_text(self._el(control))

    def trend_points(self) -> int:
        """Bucket count from the plain-text chart caption ('... - N points').

        The chart itself is a UIA custom control (no points exposed); the
        caption QLabel above it carries the count in its Name.
        """
        captions = self._driver.find_elements(
            "xpath",
            f"{MainPage.WINDOW_XPATH}//*[contains(@Name,'points')]")
        for el in captions:
            name = el.get_attribute("Name") or ""
            match = re.search(r"(\d+)\s*points", name)
            if match:
                return int(match.group(1))
        raise AssertionError("no trend caption with 'N points' found")

    def worst_cells_rows(self) -> int:
        """Row count from the 'Weakest cells - N rows' caption.

        Counting UIA DataItems is unreliable on virtualized tables (repo
        quirk); the caption makes the count readable and unambiguous
        against the network status bar's 'Showing N rows'.
        """
        captions = self._driver.find_elements(
            "xpath",
            f"{MainPage.WINDOW_XPATH}//*[contains(@Name,'Weakest cells')]")
        for el in captions:
            name = el.get_attribute("Name") or ""
            match = re.search(r"(\d+)\s*rows", name)
            if match:
                return int(match.group(1))
        raise AssertionError("no 'Weakest cells - N rows' caption found")
