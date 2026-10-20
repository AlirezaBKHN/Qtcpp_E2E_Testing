"""Users tab DSL (admin only): list users, remove one."""
from __future__ import annotations

from pageobjects.resolve_element import find
from pageobjects.users_table import UsersTable


class UsersDsl:
    def __init__(self, driver, route_home):
        self._driver = driver
        self._table = UsersTable(driver)
        route_home("homeCardUsers")

    def _el(self, control: str):
        return find(self._driver, control)

    # -- actions -------------------------------------------------------
    def remove(self, username: str) -> "UsersDsl":
        """Click a user's row, then the remove button.

        Removing `admin` is refused by the app (Db + view guard): this DSL
        merely performs - the TEST asserts the row remains.
        """
        self._table.click_named(username)
        self._el("removeUserButton").click()
        return self

    # -- readers -------------------------------------------------------
    def list(self) -> list[str]:
        """Usernames as realized cells of the users table (sorted by app)."""
        return self._table.names()