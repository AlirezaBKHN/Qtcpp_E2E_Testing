"""Locate controls by trying each candidate strategy in order.

Finds are CACHED at module level, keyed by (window phase, control id).
Measured 2026-10-20 (dbg_find_cost/dbg_ref_cache): WAD re-walks the whole
window tree on EVERY find - ~0.2s on small tabs but ~11s on the Network
tab, whose UIA tree carries the 10,080-row table (page_source: 6.9MB) -
while interactions through a CACHED element handle cost 0.01-0.8s and the
handle survives tab switches (Qt pages live for the window's lifetime).
So: resolve each control once per phase, then work through the handle.

invalidate() MUST run whenever the driver session changes (a WAD element
reference dies with its session - phase transitions re-attach).
"""
from __future__ import annotations

import os
import sys
import time
from typing import Any

import locators
from locators import READOUT_IDS

_DEBUG = os.environ.get("IST_FIND_DEBUG") == "1"

# (window phase title, control id) -> element handle
_handles: dict[tuple[str | None, str], Any] = {}


def invalidate() -> None:
    """Drop every cached handle (call on session/phase change)."""
    _handles.clear()


def _cache_key(control: str):
    from drivers import factory  # lazy: no cycle at import time

    return factory.current_title(), control


def find(driver, control: str):
    key = _cache_key(control)
    handle = _handles.get(key)
    if handle is not None:
        return handle
    last_error: Exception | None = None
    for by, value in locators.candidates(control):
        t0 = time.time()
        try:
            elements = driver.find_elements(by, value)
            if elements:
                if _DEBUG:
                    print(f"[find {control}: {by} "
                          f"{time.time() - t0:.2f}s]", file=sys.stderr,
                          flush=True)
                _handles[key] = elements[0]
                return elements[0]
        except Exception as exc:  # noqa: BLE001
            last_error = exc
        if _DEBUG:
            print(f"[find {control}: {by} MISS "
                  f"{time.time() - t0:.2f}s]", file=sys.stderr, flush=True)
    raise LookupError(f"cannot locate control {control!r}") from last_error


def _scoped_xpaths(control: str) -> list[str]:
    """Relative-XPath candidates for a scoped (section-subtree) lookup."""
    if control in locators.SPINNER_IDS:
        return [f".//Spinner[contains(@AutomationId,'{control}')]"]
    if control in READOUT_IDS:
        return [f".//Edit[contains(@AutomationId,'{control}')]"]
    return [f".//*[@Name='{control}']"]


def find_within(driver, section: str, control: str):
    """Resolve `control` scoped to the NAMED SECTION's UIA subtree.

    The section-narrowing contract (probed 2026-10-20): WAD honors subtree
    scoping - a scoped miss in a tiny subtree answers in 0.02s while the
    same global miss costs ~38s - but the cost scales with the SUBTREE
    size, so sections stay small (filter row, top bar; never the table).
    Falls back to the root candidates when the scoped pass misses.
    """
    key = _cache_key(control)
    handle = _handles.get(key)
    if handle is not None:
        return handle
    section_el = find(driver, section)
    last_error: Exception | None = None
    for value in _scoped_xpaths(control):
        t0 = time.time()
        try:
            elements = section_el.find_elements("xpath", value)
            if elements:
                if _DEBUG:
                    print(f"[find {control} within {section}: {value} "
                          f"{time.time() - t0:.2f}s]", file=sys.stderr,
                          flush=True)
                _handles[key] = elements[0]
                return elements[0]
        except Exception as exc:  # noqa: BLE001
            last_error = exc
        if _DEBUG:
            print(f"[find {control} within {section}: MISS "
                  f"{time.time() - t0:.2f}s]", file=sys.stderr, flush=True)
    return find(driver, control)  # root fallback (raises if truly absent)


def read_text(el) -> str:
    """Read a control's displayed text through whatever UIA surface has it.

    Readout widgets keep the stable id in objectName/AutomationId and mirror
    the displayed text in the accessibleName - which is the UIA Name, the
    only readable surface WAD offers (probed: no attribute key can read a
    QLineEdit's content - Value/HelpText/Text all None). Plain QLabels
    (captions) expose their text as the Name directly. The Qt fallback for
    an ID-less QLineEdit is the class name, filtered here.
    """
    for attr in ("Value", "HelpText", "Name"):
        try:
            value = el.get_attribute(attr)
        except Exception:  # noqa: BLE001 - attribute may be unsupported
            value = None
        if value and value not in ("QLineEdit", "QLabel", "QTextEdit"):
            return value
    return ""