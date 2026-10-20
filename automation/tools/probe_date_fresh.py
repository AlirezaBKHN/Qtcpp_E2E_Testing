"""Fresh-state per-step QDateEdit input experiments.

Each experiment restarts the app (fresh era defaults) so every observation
is unambiguous. Paths tried:
  E1: WAD send_keys digits only ("20260115")
  E2: WAD send_keys LEFT x2 (Selenium codes) then digits
  E3: click, then OS type_text digits (no arrows)
  E4: WAD send_keys DOWN per section (map which section UIA focus lands on)
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config as cfg  # noqa: E402
from drivers import factory, foreground, keyboard  # noqa: E402
from pageobjects.resolve_element import find, read_text  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402

LEFT = "\ue012"
DOWN = "\ue013"


def val(driver) -> str:
    return read_text(find(driver, "dashboardFromDateEdit"))


def fresh() -> tuple:
    app = SignalTrackerDsl()
    app.launch()
    app.login(cfg.E2E_USER, cfg.E2E_PASSWORD)
    app.dashboard()
    driver = factory.attach("Iran Signal Tracker")
    foreground.focus_app()
    return app, driver


def main() -> int:
    for tag in ("E1", "E2", "E3", "E4"):
        app, driver = fresh()
        try:
            el = find(driver, "dashboardFromDateEdit")
            if tag == "E1":
                el.send_keys("20260115")
                time.sleep(0.5)
                print(f"[{tag}] send_keys digits -> {val(driver)}")
            elif tag == "E2":
                el.send_keys(LEFT + LEFT + "20260115")
                time.sleep(0.5)
                print(f"[{tag}] send_keys LEFTx2+digits -> {val(driver)}")
            elif tag == "E3":
                el.click()
                time.sleep(0.4)
                keyboard.type_text("20260115")
                time.sleep(0.5)
                print(f"[{tag}] click + OS digits -> {val(driver)}")
            elif tag == "E4":
                el.send_keys(DOWN)
                time.sleep(0.4)
                v1 = val(driver)
                el.send_keys(LEFT + DOWN)
                time.sleep(0.4)
                v2 = val(driver)
                el.send_keys(LEFT + LEFT + DOWN)
                time.sleep(0.4)
                v3 = val(driver)
                print(f"[{tag}] DOWN={v1} LEFT+DOWN={v2} LEFTx2+DOWN={v3} "
                      f"(fresh was 2026-10-14)")
        finally:
            app.quit()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())