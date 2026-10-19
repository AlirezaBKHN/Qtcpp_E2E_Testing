"""Live UIA discovery for the Iran Signal Tracker window.

Opens a session through the factory (Appium path), then dumps every element
with its Name / AutomationId / ClassName so the real locator strategy can be
frozen into locators.py. Run: python automation/tools/inspect_ui.py
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from drivers.factory import get_driver, quit_driver  # noqa: E402


def main() -> int:
    driver = get_driver()
    try:
        windows = driver.find_elements("xpath", "//Window")
        print(f"windows found: {len(windows)}")
        elements = driver.find_elements("xpath", "//*")
        print(f"total elements: {len(elements)}")
        for i, el in enumerate(elements):
            attrs = {}
            for attr in ("Name", "AutomationId", "ControlType", "ClassName"):
                try:
                    attrs[attr] = el.get_attribute(attr)
                except Exception as exc:  # noqa: BLE001
                    attrs[attr] = f"<err {exc.__class__.__name__}>"
            print(f"[{i:3d}] id={attrs['AutomationId']!r:24} "
                  f"name={attrs['Name']!r:20} type={attrs['ControlType']!r:16} "
                  f"class={attrs['ClassName']!r}")
    finally:
        quit_driver()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
