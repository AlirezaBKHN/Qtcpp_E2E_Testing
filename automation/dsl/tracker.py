"""The human-readable DSL facade. Tests must only speak this language.

Binding contract (from the handoff PRD):

    tracker.launch()
    tracker.province("Tehran")
    tracker.city("Tehran")
    tracker.technology("5G")
    tracker.search()
    tracker.table().should_have_rows()
    tracker.table().should_contain("Irancell")
    tracker.chart().should_be_visible()
    tracker.export().should_create_file()

Additive helpers only: should_have_row_count(n), operator_name(...),
reset(), refresh(), quit().
"""
from __future__ import annotations

import time
from pathlib import Path

import config as C
from drivers.factory import get_driver, quit_driver
from drivers.foreground import focus_app
from dsl.export_result import ExportResult
from pageobjects.chart_widget import SignalChart
from pageobjects.main_page import MainPage
from pageobjects.table_widget import SignalTable


class SignalTrackerDsl:
    def __init__(self):
        self._driver = None
        self._main = None

    # -- lifecycle ------------------------------------------------------
    def launch(self) -> "SignalTrackerDsl":
        self._driver = get_driver()
        self._main = MainPage(self._driver)
        main = self._driver.find_elements("xpath", MainPage.WINDOW_XPATH)
        deadline = time.time() + 30
        while not main and time.time() < deadline:
            time.sleep(0.5)
            main = self._driver.find_elements("xpath", MainPage.WINDOW_XPATH)
        assert main, "Iran Signal Tracker window did not appear"
        # The window may open BEHIND other application windows (a terminal,
        # the agent's chat). Every later keystroke/click would land there
        # (observed: typing landed in a chat window). Force the OS foreground
        # to the app before any interaction in the session.
        focus_app()
        return self

    def quit(self) -> None:
        quit_driver()
        self._driver = None
        self._main = None

    # -- filters (binding names) ---------------------------------------
    def province(self, value: str) -> "SignalTrackerDsl":
        self._main.select_combo("provinceCombo", value)
        return self

    def city(self, value: str) -> "SignalTrackerDsl":
        self._main.select_combo("cityCombo", value)
        return self

    def technology(self, value: str) -> "SignalTrackerDsl":
        self._main.select_combo("technologyCombo", value)
        return self

    def operator_name(self, value: str) -> "SignalTrackerDsl":
        self._main.select_combo("operatorCombo", value)
        return self

    # -- actions --------------------------------------------------------
    def search(self) -> "SignalTrackerDsl":
        self._main.click_search()
        return self

    def reset(self) -> "SignalTrackerDsl":
        self._main.click_reset()
        return self

    def refresh(self) -> "SignalTrackerDsl":
        self._main.click_refresh()
        return self

    # -- sub-views ------------------------------------------------------
    def table(self) -> SignalTable:
        return SignalTable(self._main)

    def chart(self) -> SignalChart:
        return SignalChart(self._main)

    def export(self) -> ExportResult:
        C.EXPORT_DIR.mkdir(parents=True, exist_ok=True)
        before = {p.name for p in C.EXPORT_DIR.glob("export_*.csv")}
        self._main.click_export()
        return ExportResult(C.EXPORT_DIR, before)

    # -- additive helpers ------------------------------------------------
    def row_count(self) -> int:
        return self._main.row_count_text()
