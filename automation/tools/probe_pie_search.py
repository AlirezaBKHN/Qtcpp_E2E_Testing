"""Bug 1 evidence: per-tab visible-state pie geometry dumps.

Dashboard: route, wait, Search (sync dump fires while visible), shot.
Reports: route, wait, Search (sync dump fires while visible), shot.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config as cfg  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402

OUT = Path(__file__).resolve().parent.parent / "probe_shots"
OUT.mkdir(exist_ok=True)

tracker = SignalTrackerDsl().launch()
try:
    tracker.login(cfg.E2E_USER, cfg.E2E_PASSWORD)
    dash = tracker.dashboard()
    time.sleep(2.0)
    dash.search()
    time.sleep(0.5)
    tracker._driver.save_screenshot(str(OUT / "dash_searched.png"))
    rep = tracker.reports()
    time.sleep(2.0)
    rep.search()
    time.sleep(0.5)
    tracker._driver.save_screenshot(str(OUT / "reports_searched.png"))
    print("done")
finally:
    tracker.quit()