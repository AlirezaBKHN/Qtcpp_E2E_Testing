"""Locate controls by trying each candidate strategy in order."""
from __future__ import annotations

import locators


def find(driver, control: str):
    last_error: Exception | None = None
    for by, value in locators.candidates(control):
        try:
            elements = driver.find_elements(by, value)
            if elements:
                return elements[0]
        except Exception as exc:  # noqa: BLE001
            last_error = exc
    raise LookupError(f"cannot locate control {control!r}") from last_error
