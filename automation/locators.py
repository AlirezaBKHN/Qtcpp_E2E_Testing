"""Stable locator candidates per control.

Every entry is an ordered list of (by, value) candidates; the page objects
walk the list from strongest to weakest. No coordinates anywhere
(Review Focus #4).

Frozen from live discovery (automation/tools/inspect_ui.py, Appium session
2026-10-19): Qt exposes accessibleName as UIA `Name`, which exactly equals
the stable identifier. AutomationId is PREFIXED for nested widgets
(`QApplication.MainWindow.QWidget.provinceCombo`), so plain `id` matching
only works for some widgets (e.g. signalTable) and stays as fallback.
"""
from __future__ import annotations

import config as C

CONTROLS: dict[str, list[tuple[str, str]]] = {key: [(C.BY_NAME, key), (C.BY_ID, key)]
                                              for key in (
    "provinceCombo", "cityCombo", "technologyCombo", "operatorCombo",
    "searchButton", "refreshButton", "resetButton", "exportButton",
    "signalTable", "signalChart", "chartGroupBy",
)}


def candidates(control: str) -> list[tuple[str, str]]:
    return CONTROLS[control]
