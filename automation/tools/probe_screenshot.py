"""Evidence: what is actually foreground/on-screen at a cold session start?

Saves a screenshot right after launch, then again after clicking export,
and reports the window's BoundingRectangle for cross-checking.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from dsl.tracker import SignalTrackerDsl  # noqa: E402

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("reports/export/..")

def full_status(driver) -> str:
    els = driver.find_elements(
        "xpath", "//*[contains(@Name,'Showing') or contains(@Name,'xported')]")
    return els[0].get_attribute("Name") if els else "(no status)"

tracker = SignalTrackerDsl().launch()
driver = tracker._driver
try:
    win = driver.find_elements("xpath", "//Window")[0]
    p0 = OUT / "evidence_cold.png"
    driver.save_screenshot(str(p0.resolve()))
    print("screenshot saved:", p0.resolve(), "| rect:", win.get_attribute("BoundingRectangle"))
    btn = driver.find_elements("xpath", "//*[@Name='exportButton']")[0]
    btn.click()
    time.sleep(1.0)
    p1 = OUT / "evidence_after_click.png"
    driver.save_screenshot(str(p1.resolve()))
    print("after export click:", full_status(driver))
finally:
    tracker.quit()
