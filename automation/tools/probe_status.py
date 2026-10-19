"""Find how the status-bar text is exposed to WAD."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from dsl.tracker import SignalTrackerDsl  # noqa: E402

tracker = SignalTrackerDsl().launch()
driver = tracker._driver
for expr in ("//Text", "//*[contains(@Name,'Showing')]", "//*[contains(@Name,'Ready')]"):
    try:
        els = driver.find_elements("xpath", expr)
        print(expr, "->", len(els))
        for el in els[:12]:
            try:
                print("   *", repr(el.get_attribute("Name")))
            except Exception as e:  # noqa: BLE001
                print("   * <attr err>", e)
    except Exception as e:  # noqa: BLE001
        print(expr, "-> ERROR", e.__class__.__name__, str(e)[:120])
tracker.quit()
