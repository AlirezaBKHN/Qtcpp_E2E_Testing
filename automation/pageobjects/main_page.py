"""Filter section, buttons, and navigation."""
from __future__ import annotations

import time

from drivers import keyboard
from pageobjects.resolve_element import find, find_within

COMBOS = ("provinceCombo", "cityCombo", "technologyCombo", "operatorCombo")

WINDOW_XPATH = "//Window"


class MainPage:
    """Responsible for filters, buttons, and top-level navigation.

    Filter-row controls resolve WITHIN the app's named `filterSection`
    (small UIA subtree; probed 0.02s scoped vs 11-38s from the root).
    Everything else (status bar, table, chart) resolves from the root -
    the status bar and the data widgets live outside the section, and the
    handle cache amortizes their first finds.
    """

    WINDOW_XPATH = WINDOW_XPATH  # re-export as class attribute

    SECTION_ID = "filterSection"
    _SECTION_MEMBERS = COMBOS + ("searchButton", "refreshButton",
                                 "resetButton", "exportButton")

    def __init__(self, driver):
        self._driver = driver
        self._elements: dict[str, object] = {}

    # -- lookup --------------------------------------------------------
    def _el(self, control: str):
        if control not in self._elements:
            if control in self._SECTION_MEMBERS:
                self._elements[control] = find_within(
                    self._driver, self.SECTION_ID, control)
            else:
                self._elements[control] = find(self._driver, control)
        return self._elements[control]

    # -- filters -------------------------------------------------------
    def select_combo(self, control: str, value: str) -> None:
        """Select `value` in the combo `control` (cascades happen on change).

        Popup KEYBOARD selection (live-probed 2026-10-20): clicking the
        combo opens the popup and hands it keyboard focus; typing the
        value's text runs Qt's type-ahead to the matching item and Enter
        commits it. NO WAD calls happen while the popup is open - finds on
        this tab cost ~11s each (the 10,080-row table bloats the UIA tree;
        XPath ListItem finds measured 40s), while the OS-level keystrokes
        are free. Verified end-to-end: Tehran -> 'Showing 1025 rows'.

        This supersedes BOTH older rulings: keyboard type-ahead into a
        CLOSED combo is dead in attached sessions (keystrokes land
        nowhere), and popup-item clicks (find //ListItem[@Name=...]) are
        too slow to keep.
        """
        el = self._el(control)  # cached handle: click is ~0.2s
        el.click()              # popup opens and takes keyboard focus
        time.sleep(0.5)         # popup draw settle
        keyboard.type_text(value)
        keyboard.press_enter()  # commits the type-ahead item, popup closes
        time.sleep(0.6)         # cascade settle (city combo repopulates)

    def apply_filters(self, province: str | None = None, city: str | None = None,
                      technology: str | None = None, operator: str | None = None) -> None:
        if province:
            self.select_combo("provinceCombo", province)
        if city:
            self.select_combo("cityCombo", city)
        if technology:
            self.select_combo("technologyCombo", technology)
        if operator:
            self.select_combo("operatorCombo", operator)

    # -- buttons -------------------------------------------------------
    # Note: element.click() is only safe because launch() forces the app to
    # the OS foreground first (see drivers/foreground.py). Without it the
    # click and the keystrokes land on whatever window covers the app (a
    # terminal, a chat), which is exactly the failure mode that masqueraded
    # as "Invoke pattern dropped the click" for hours.
    def click_search(self) -> None:
        self._el("searchButton").click()

    def click_refresh(self) -> None:
        self._el("refreshButton").click()

    def click_reset(self) -> None:
        self._el("resetButton").click()

    def click_export(self) -> None:
        self._el("exportButton").click()

    # -- readouts ------------------------------------------------------
    def is_visible(self) -> bool:
        for control in COMBOS + ("searchButton", "signalTable", "signalChart"):
            try:
                if not self._el(control).is_displayed():
                    return False
            except Exception:  # noqa: BLE001
                return False
        return True

    def row_count_text(self) -> int:
        """Stable, fast row count from the status bar ('Showing N rows').

        The statusText handle is cached like every other control (finds
        cost ~11s on this tab); get_attribute("Name") reads the LIVE text
        through the cached ref (probed 0.01s).
        """
        el = find(self._driver, "statusText")
        name = el.get_attribute("Name") or ""
        if name.startswith("Showing "):
            parts = name.split()
            if len(parts) >= 3 and parts[2].startswith("rows"):
                return int(parts[1])
        raise AssertionError("no 'Showing N rows' status message found")
