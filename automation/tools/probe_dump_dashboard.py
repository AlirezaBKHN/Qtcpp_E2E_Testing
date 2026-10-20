"""One-off probe: launch the app, log in, dump the Dashboard page_source.

Exposes how QDateEdit / QListWidget / QChartView / QComboBox present to
WAD (control types, AutomationId prefixes, Name values).
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config as cfg  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402
from drivers import factory  # noqa: E402


def main() -> int:
    app = SignalTrackerDsl()
    app.launch()
    try:
        app.login(cfg.E2E_USER, cfg.E2E_PASSWORD)
        app.dashboard()  # routes via the home card
        driver = factory.attach("Iran Signal Tracker")
        src = driver.page_source
        out = Path(__file__).resolve().parent.parent / "probe_dump.xml"
        out.write_text(src, encoding="utf-8")
        print("dumped:", out, len(src))
    finally:
        app.quit()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())