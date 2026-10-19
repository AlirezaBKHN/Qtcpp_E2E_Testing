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
