"""Filter section page object: scoped combos + Search/Reset buttons.

Mirrors MainPage's section-scoped mechanics for the dashboard/reports
filter sections (spec 2026-10-20): combos resolve WITHIN the named small
UIA subtree (find performance architecture) and select via the popup
keyboard type-ahead (the only live-probed combo selection path).
"""
from __future__ import annotations

import time

from drivers import keyboard
from pageobjects.resolve_element import find_within


class FilterSection:
    def __init__(self, driver, section: str):
        self._driver = driver
        self._section = section

    def _el(self, control: str):
        return find_within(self._driver, self._section, control)

    def select_combo(self, control: str, value: str) -> None:
        """Select `value` in the combo `control` (popup keyboard type-ahead)."""
        el = self._el(control)
        el.click()              # popup opens and takes keyboard focus
        time.sleep(0.5)         # popup draw settle
        keyboard.type_text(value)
        keyboard.press_enter()  # commits the type-ahead item, popup closes
        time.sleep(0.6)         # cascade settle (city combo repopulates)

    def click(self, control: str) -> None:
        self._el(control).click()