"""Filter section, buttons, and navigation."""
from __future__ import annotations

from pageobjects.resolve_element import find

COMBOS = ("provinceCombo", "cityCombo", "technologyCombo", "operatorCombo")

WINDOW_XPATH = "//Window"


class MainPage:
    """Responsible for filters, buttons, and top-level navigation."""

    WINDOW_XPATH = WINDOW_XPATH  # re-export as class attribute

    def __init__(self, driver):
        self._driver = driver
        self._elements: dict[str, object] = {}

    # -- lookup --------------------------------------------------------
    def _el(self, control: str):
        if control not in self._elements:
            self._elements[control] = find(self._driver, control)
        return self._elements[control]

    # -- filters -------------------------------------------------------
    def select_combo(self, control: str, value: str) -> None:
        """Select `value` in the combo `control` (cascades happen on change).

        Keyboard-first: typing into a closed non-editable QComboBox selects
        the matching item (type-ahead) and fires currentIndexChanged, which
        also drives the city cascade. This avoids the popup-item ambiguity
        of element clicks: once the table is visible, table cells share the
        item's Name (e.g. 'Tehran'), so finding elements by Name can match
        and click a table cell instead of the popup item.

        Requires the app to be foreground (enforced in launch()): WAD
        delivers keystrokes to whatever window has keyboard focus.
        """
        self._el(control).send_keys(value)

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

        Note: WAD's XPath lacks `starts-with` (silently empty); `contains`
        is supported and safe here because the app only emits this phrase
        in the status bar.
        """
        texts = self._driver.find_elements(
            "xpath", f"{WINDOW_XPATH}//*[contains(@Name,'Showing')]")
        for el in texts:
            name = el.get_attribute("Name")
            if name and name.startswith("Showing "):
                parts = name.split()
                if len(parts) >= 3 and parts[2].startswith("rows"):
                    return int(parts[1])
        raise AssertionError("no 'Showing N rows' status message found")
