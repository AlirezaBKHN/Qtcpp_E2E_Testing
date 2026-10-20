"""Top bar + tab activation + home routing cards."""
from __future__ import annotations

from pageobjects.main_page import MainPage
from pageobjects.resolve_element import find, find_within, read_text

WINDOW_XPATH = MainPage.WINDOW_XPATH

# QTabBar items expose the tab TEXT (not the page ids); switching pages
# goes through them because Home's cards exist only on the homeTab page:
# WAD prunes the hidden pages' widgets from the control view.
# locators.py carries each item as `tabItem_<pageId>` (cached per phase).

# Exact window name disambiguates the main window from the sign-in dialog
# ('Sign in - Iran Signal Tracker'): encoded in the tabItem locators.


class TabsBar:
    """Home-card based navigation and top-bar readouts.

    Cards live on the homeTab, so navigation is always performed from Home:
    the DSL resets the tab to Home before routing (tracker's _route_home).
    """

    def __init__(self, driver):
        self._driver = driver

    def _el(self, control: str):
        return find(self._driver, control)

    # -- actions -------------------------------------------------------
    def click_tab(self, page_id: str) -> None:
        """Switch the main window to `page_id` via its QTabBar item.

        Only page ids present for the current role may be requested
        (usersTab exists for admin sessions only). The item handle comes
        from the per-phase cache (a root find on the fat Network tab
        costs ~11s; TabItem handles live as long as the window).
        """
        try:
            find(self._driver, f"tabItem_{page_id}").click()
        except (KeyError, LookupError) as exc:
            raise LookupError(
                f"tab item for page {page_id!r} not found (is the tab "
                f"mounted for the current role?)") from exc

    def click_home_card(self, card: str) -> None:
        self._el(card).click()

    def click_logout(self) -> None:
        find_within(self._driver, "headerSection", "logoutButton").click()

    # -- readers -------------------------------------------------------
    def current_user(self) -> str:
        """Text of the currentUserLabel readout; '' when not present
        (e.g. after logout, when the sign-in dialog is back)."""
        try:
            el = find_within(self._driver, "headerSection", "currentUserLabel")
        except Exception:  # noqa: BLE001 - absent after logout
            return ""
        return read_text(el)
