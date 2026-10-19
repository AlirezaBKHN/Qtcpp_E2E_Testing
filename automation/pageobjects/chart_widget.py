"""Signal chart page object: visibility validation."""
from __future__ import annotations

from pageobjects.main_page import MainPage


class SignalChart:
    """Responsible for chart validation and visibility validation."""

    def __init__(self, manager: MainPage):
        self._chart = manager._el("signalChart")

    def should_be_visible(self) -> "SignalChart":
        assert self._chart.is_displayed(), "signalChart is not displayed"
        return self
