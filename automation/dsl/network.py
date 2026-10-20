"""Network data tab DSL: today's filter/table/chart/export API, verbatim.

The main window's networkTab hosts the original screen; the DSL activates
the tab via its home card and then exposes exactly the binding contract the
tests already speak (province/city/technology/operator_name/search/
reset/refresh + table/chart/export, row_count).
"""
from __future__ import annotations

import config as C
from dsl.export_result import ExportResult
from pageobjects.chart_widget import SignalChart
from pageobjects.main_page import MainPage
from pageobjects.table_widget import SignalTable


class NetworkDsl:
    def __init__(self, driver, route_home):
        self._main = MainPage(driver)
        self._driver = driver
        # Activate the network tab once per session (the first call routes
        # from the Home tab; later calls are no-ops).
        route_home("homeCardNetwork")

    # -- filters (binding names) ---------------------------------------
    def province(self, value: str) -> "NetworkDsl":
        self._main.select_combo("provinceCombo", value)
        return self

    def city(self, value: str) -> "NetworkDsl":
        self._main.select_combo("cityCombo", value)
        return self

    def technology(self, value: str) -> "NetworkDsl":
        self._main.select_combo("technologyCombo", value)
        return self

    def operator_name(self, value: str) -> "NetworkDsl":
        self._main.select_combo("operatorCombo", value)
        return self

    # -- actions --------------------------------------------------------
    def search(self) -> "NetworkDsl":
        self._main.click_search()
        return self

    def reset(self) -> "NetworkDsl":
        self._main.click_reset()
        return self

    def refresh(self) -> "NetworkDsl":
        self._main.click_refresh()
        return self

    # -- sub-views ------------------------------------------------------
    def table(self) -> SignalTable:
        return SignalTable(self._main)

    def chart(self) -> SignalChart:
        return SignalChart(self._main)

    def export(self):
        C.EXPORT_DIR.mkdir(parents=True, exist_ok=True)
        before = {p.name for p in C.EXPORT_DIR.glob("export_*.csv")}
        self._main.click_export()
        return ExportResult(C.EXPORT_DIR, before)

    def row_count(self) -> int:
        return self._main.row_count_text()
