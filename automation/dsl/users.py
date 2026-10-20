"""Users tab DSL (admin only): list users, remove one."""
from __future__ import annotations

from pageobjects.main_page import MainPage
from pageobjects.resolve_element import find


class UsersDsl:
    def __init__(self, driver, route_home):
        self._driver = driver
        self._main = MainPage(driver)
        route_home("homeCardUsers")

    def _el(self, control: str):
        return find(self._driver, control)

    # -- actions -------------------------------------------------------
    def remove(self, username: str) -> "UsersDsl":
        """Click a user's row, then the remove button.

        Removing `admin` is refused by the app (Db + view guard): this DSL
        merely performs - the TEST asserts the row remains.
        """
        users_table = self._el("usersTable")
        rows = users_table.find_elements("xpath", ".//DataItem")
        for row in rows:
            name = row.get_attribute("Name") or ""
            if name == username:
                row.click()
                self._el("removeUserButton").click()
                return self
        raise AssertionError(
            f"row for {username!r} not realized in usersTable; "
            f"realized: {[ (r.get_attribute('Name') or '') for r in rows ]}")

    # -- readers -------------------------------------------------------
    def list(self) -> list[str]:
        """Usernames as realized cells of the users table (sorted by app)."""
        users_table = self._el("usersTable")
        rows = users_table.find_elements("xpath", ".//DataItem")
        names = [(r.get_attribute("Name") or "") for r in rows]
        if "Username" in names:  # header cell
            names.remove("Username")
        return names
