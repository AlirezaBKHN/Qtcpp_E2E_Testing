"""Bug evidence: screenshots of the Dashboard pie area + Reports pie area.

Launch, login, route to Dashboard (screenshot), Reports (screenshot).
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config as cfg  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402

OUT = Path(__file__).resolve().parent.parent / "probe_shots"
OUT.mkdir(exist_ok=True)

tracker = SignalTrackerDsl().launch()
try:
    tracker.login(cfg.E2E_USER, cfg.E2E_PASSWORD)
    tracker.dashboard()
    tracker._driver.save_screenshot(str(OUT / "dashboard.png"))
    print("dashboard:", OUT / "dashboard.png")
    tracker.reports()
    tracker._driver.save_screenshot(str(OUT / "reports.png"))
    print("reports:", OUT / "reports.png")
finally:
    tracker.quit()