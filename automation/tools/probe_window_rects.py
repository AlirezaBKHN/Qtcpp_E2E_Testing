"""Diagnose window-rect weirdness: enumerate app windows while alive."""
from __future__ import annotations

import ctypes
import ctypes.wintypes
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config as cfg  # noqa: E402
from drivers import factory, foreground  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402

user32 = ctypes.windll.user32
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    user32.SetProcessDPIAware()

results: list = []


@ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.wintypes.HWND, ctypes.wintypes.LPARAM)
def cb(hwnd, lparam):
    buf = ctypes.create_unicode_buffer(256)
    user32.GetWindowTextW(hwnd, buf, 256)
    if "Iran Signal Tracker" in buf.value:
        r1 = ctypes.wintypes.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(r1))
        r2 = ctypes.wintypes.RECT()
        dwm = ctypes.windll.dwmapi.DwmGetWindowAttribute(
            hwnd, 9, ctypes.byref(r2), ctypes.sizeof(r2))
        results.append((buf.value, hex(hwnd), user32.IsWindowVisible(hwnd),
                        (r1.left, r1.top, r1.right - r1.left, r1.bottom - r1.top),
                        "dwm:" + str((r2.left, r2.top, r2.right - r2.left, r2.bottom - r2.top))
                        if dwm == 0 else f"dwmerr {dwm}"))
    return True


tracker = SignalTrackerDsl().launch()
try:
    foreground.focus_app()
    time.sleep(0.5)
    user32.EnumWindows(cb, 0)
    for row in results:
        print(row)
    tracker.login(cfg.E2E_USER, cfg.E2E_PASSWORD)
    time.sleep(1.0)
    results.clear()
    user32.EnumWindows(cb, 0)
    print("--- after login ---")
    for row in results:
        print(row)
finally:
    tracker.quit()