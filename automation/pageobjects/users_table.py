"""Users-table page object (admin view; small table, all rows realized)."""
from __future__ import annotations

from pageobjects.resolve_element import find, read_text


class UsersTable:
    def __init__(self, driver):
        self._driver = driver

    def _rows(self):
        return find(self._driver, "usersTable").find_elements(
            "xpath", ".//DataItem")

    def names(self) -> list[str]:
        """Usernames as realized cells (sorted by app)."""
        names = [read_text(r) for r in self._rows()]
        if "Username" in names:  # header cell
            names.remove("Username")
        return names

    def click_named(self, username: str) -> None:
        """Click the row whose realized text equals `username`."""
        for row in self._rows():
            if read_text(row) == username:
                row.click()
                return
        raise AssertionError(
            f"row for {username!r} not realized in usersTable; "
            f"realized: {self.names()}")