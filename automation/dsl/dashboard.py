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