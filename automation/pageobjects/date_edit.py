"""The pinned QDateEdit automation recipe (plan Task 7, probed).

WAD send_keys UIA-focuses the element, focus lands on the YEAR section,
and the 8 digits auto-advance year -> month -> day (fresh-state probe
tools/probe_date_fresh.py E1). OS-level keybd_event digits do NOT land
(E3: value unchanged). The 'send_keys APPENDS' WAD quirk does not apply
to section-typed date edits, but the recipe assumes the edit carries no
in-progress typing state (fresh app or after the Reset button).
"""
from __future__ import annotations

import time

from pageobjects.resolve_element import find


def set_date(driver, control: str, date: str) -> None:
    """Set `control` (a QDateEdit) to `date` ('YYYY-MM-DD')."""
    el = find(driver, control)
    el.send_keys(date.replace("-", ""))
    time.sleep(0.3)  # date-changed settle