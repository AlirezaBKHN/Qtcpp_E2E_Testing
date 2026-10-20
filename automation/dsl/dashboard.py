"""Dashboard tab DSL: KPI readouts, trend granularity, weakest cells."""
from __future__ import annotations

from pageobjects.dashboard_view import DashboardView


class DashboardDsl:
    def __init__(self, driver, route_home):
        self._view = DashboardView(driver)
        route_home("homeCardDashboard")

    # -- readers (the TEST asserts) ------------------------------------
    def kpi(self, name: str) -> str:
        """Text of a KPI readout; keys per KPI_IDS."""
        return self._view.kpi(name)

    def trend_points(self) -> int:
        """Bucket count from the plain-text chart caption ('... - N points')."""
        return self._view.trend_points()

    def worst_cells_rows(self) -> int:
        """Row count from the 'Weakest cells - N rows' caption."""
        return self._view.weakest_rows()

    # -- filter section (spec 2026-10-20) --------------------------------
    def province(self, value: str) -> "DashboardDsl":
        self._view.select_combo("dashboardProvinceCombo", value)
        return self

    def city(self, value: str) -> "DashboardDsl":
        self._view.select_combo("dashboardCityCombo", value)
        return self

    def technology(self, value: str) -> "DashboardDsl":
        self._view.select_combo("dashboardTechnologyCombo", value)
        return self

    def operator_name(self, value: str) -> "DashboardDsl":
        self._view.select_combo("dashboardOperatorCombo", value)
        return self

    def from_date(self, value: str) -> "DashboardDsl":
        self._view.set_date("dashboardFromDateEdit", value)
        return self

    def to_date(self, value: str) -> "DashboardDsl":
        self._view.set_date("dashboardToDateEdit", value)
        return self

    def search(self) -> "DashboardDsl":
        self._view.search()
        return self

    def reset(self) -> "DashboardDsl":
        self._view.reset()
        return self

    def from_date_text(self) -> str:
        """The dashboard from-date readout's displayed text."""
        return self._view.from_date_text()

    def to_date_text(self) -> str:
        """The dashboard to-date readout's displayed text."""
        return self._view.to_date_text()

    # -- share chart data surfaces (the TEST asserts) ----------------------
    def pie_slices(self) -> list[tuple[str, int, float]]:
        """Operator pie slices as (label, cell count, percent)."""
        return self._view.pie_slices()

    def bar_groups(self) -> list[tuple[str, float]]:
        """Technology bar values as (label, quality percent)."""
        return self._view.bar_groups()

    def list_items(self, control: str) -> list[str]:
        """Raw item texts of a readout list (operatorShare/techQuality)."""
        return self._view.list_items(control)

    def caption_text(self, control: str) -> str:
        """A caption's full text ('Cells by operator - 4 slices')."""
        return self._view.caption_text(control)