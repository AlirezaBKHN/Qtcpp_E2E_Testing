"""Stepwise QDateEdit keyboard probe: where does focus land, what sticks?"""
from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config as cfg  # noqa: E402
from drivers import factory, foreground, keyboard  # noqa: E402
from pageobjects.resolve_element import find, read_text  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402


def report(driver, tag: str) -> None:
    el = find(driver, "dashboardFromDateEdit")
    print(f"[{tag}] Name={read_text(el)!r} focus={el.get_attribute('HasKeyboardFocus')}")


def main() -> int:
    app = SignalTrackerDsl()
    app.launch()
    try:
        app.login(cfg.E2E_USER, cfg.E2E_PASSWORD)
        app.dashboard()
        driver = factory.attach("Iran Signal Tracker")

        el = find(driver, "dashboardFromDateEdit")
        el.click()
        time.sleep(0.5)
        foreground.focus_app()
        print("foreground:", foreground.is_foreground())
        report(driver, "after click")

        keyboard.press_left(1)
        report(driver, "after left1")
        keyboard.press_left(1)
        report(driver, "after left2")
        keyboard.press_left(1)
        report(driver, "after left3")

        keyboard.type_text("2024")
        time.sleep(0.4)
        report(driver, "after year 2024")

        keyboard.type_text("01")
        time.sleep(0.4)
        report(driver, "after month 01")

        keyboard.type_text("15")
        time.sleep(0.4)
        report(driver, "after day 15")

        # Control: does ANY synthetic key reach the widget? Arrow-down
        # increments the focused section by one step.
        import ctypes
        user32 = ctypes.windll.user32
        el = find(driver, "dashboardFromDateEdit")
        el.click()
        time.sleep(0.4)
        user32.keybd_event(0x28, 0, 0, 0)
        user32.keybd_event(0x28, 0, 2, 0)
        time.sleep(0.4)
        report(driver, "after VK_DOWN")

        # Control: WAD element-targeted send_keys on the Spinner.
        el.send_keys("2024")
        time.sleep(0.4)
        report(driver, "after send_keys 2024")
    finally:
        app.quit()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())