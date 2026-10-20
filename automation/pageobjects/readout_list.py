"""Readout list page object: the one-column QListWidget data surface.

The share charts (operator pie, technology bar, reports group pie) are
UIA custom controls - WAD cannot see slices or bars. The app mirrors
every slice/bar into a QListWidget whose item text is the pinned data
format (plan Global Constraints); the list is the ONLY data surface the
E2E stack reads. Items are real ListItem children (probed via
tools/probe_dump_dashboard.py) - no virtualization at these sizes.
"""
from __future__ import annotations

from pageobjects.resolve_element import find


class ReadoutList:
    def __init__(self, driver, control: str):
        self._driver = driver
        self._control = control

    def items(self) -> list[str]:
        """Ordered item texts (the list's display order == app order)."""
        el = find(self._driver, self._control)
        return [item.get_attribute("Name") or ""
                for item in el.find_elements("xpath", ".//ListItem")]

    def count(self) -> int:
        return len(self.items())