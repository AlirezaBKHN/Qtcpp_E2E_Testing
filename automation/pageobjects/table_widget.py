"""Signal table page object: reading rows, searching contents, counting."""
from __future__ import annotations

from pageobjects.main_page import MainPage


class SignalTable:
    """Responsible for reading rows, searching contents, counting rows."""

    def __init__(self, manager: MainPage):
        self._main = manager
        self._driver = manager._driver  # engine dependency, not a style leak
        self._table = manager._el("signalTable")

    def _data_items(self, limit: int | None = None) -> list[str]:
        """Names of realized DataItem nodes (cells) in the table."""
        items = self._driver.find_elements("xpath", "//DataItem")
        names: list[str] = []
        for el in items:
            name = el.get_attribute("Name")
            if name and (limit is None or len(names) < limit):
                names.append(name)
        return names

    # -- assertions (fluent style per the DSL contract) ----------------
    def should_have_rows(self) -> "SignalTable":
        count = self._main.row_count_text()
        assert count > 0, f"table expected rows > 0, got {count}"
        return self

    def should_have_row_count(self, expected: int) -> "SignalTable":
        count = self._main.row_count_text()
        assert count == expected, f"table expected {expected} rows, got {count}"
        return self

    def should_contain(self, text: str) -> "SignalTable":
        names = self._data_items()
        assert any(text in n for n in names), \
            f"table data does not contain {text!r}; realized cells: {names[:20]}"
        return self

    # -- row-id readout (spec 2026-10-20) -------------------------------
    def ids(self) -> list[int]:
        """Realized ID-column cells of the CURRENT page (row-major stride
        7, header row excluded). Small bounded read: pages hold at most
        the selected page size."""
        names = self._data_items()
        if names and names[0] == "ID":  # header cell, usersTable-style
            names = names[1:]
        return [int(n) for n in names[0::7]]

    def realized_row_count(self) -> int:
        """Rows actually realized in the UIA tree for the current page."""
        return len(self.ids())

    def should_contain_row(self, id: int) -> "SignalTable":
        realized = self.ids()
        assert id in realized, \
            f"table page does not contain row id {id}; realized ids: {realized[:20]}"
        return self

    def should_not_contain_row(self, id: int) -> "SignalTable":
        realized = self.ids()
        assert id not in realized, \
            f"table page unexpectedly contains row id {id}; realized ids: {realized[:20]}"
        return self
