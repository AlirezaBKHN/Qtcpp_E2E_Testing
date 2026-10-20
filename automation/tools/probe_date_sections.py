"""Section-mapping experiment for QDateEdit automation.

For each navigation route, determines which section holds focus by
observing what a single VK_DOWN changes (year vs day), then tries full
date typing via WAD send_keys vs OS type_text.
"""
from __future__ import annotations

import ctypes
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config as cfg  # noqa: E402
from drivers import factory, foreground, keyboard  # noqa: E402
from pageobjects.resolve_element import find, read_text  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402

user32 = ctypes.windll.user32


def val(driver) -> str:
    return read_text(find(driver, "dashboardFromDateEdit"))


def down(driver, tag: str) -> None:
    user32.keybd_event(0x28, 0, 0, 0)
    user32.keybd_event(0x28, 0, 2, 0)
    time.sleep(0.35)
    print(f"[{tag}] after DOWN: {val(driver)}")


def main() -> int:
    app = SignalTrackerDsl()
    app.launch()
    try:
        app.login(cfg.E2E_USER, cfg.E2E_PASSWORD)
        app.dashboard()
        driver = factory.attach("Iran Signal Tracker")
        foreground.focus_app()

        el = find(driver, "dashboardFromDateEdit")
        el.click()
        time.sleep(0.4)
        print("[1] plain click ->", val(driver))
        down(driver, "1")

        keyboard.press_left(1)
        down(driver, "2 after one left")

        el.click()
        time.sleep(0.4)
        keyboard.press_left(2)
        down(driver, "3 after two lefts")

        # Full typing experiment A: WAD send_keys with 8 digits.
        el.click()
        time.sleep(0.4)
        el.send_keys("20260115")
        time.sleep(0.5)
        print("[A] send_keys 20260115 ->", val(driver))

        # Full typing experiment B: OS type_text with 8 digits, one left first.
        el.click()
        time.sleep(0.4)
        keyboard.press_left(1)
        keyboard.type_text("20260220")
        time.sleep(0.5)
        print("[B] type_text 20260220 after left1 ->", val(driver))
    finally:
        app.quit()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())