"""Thesis figure capture: APP-WINDOW screenshots for latex/BoostanSample/Pic.

Captures the app window rectangle only (FindWindowW + GetWindowRect ->
PowerShell CopyFromScreen crop): sign-in, network tab (Fars filter),
dashboard, reports.
"""
from __future__ import annotations

import ctypes
import ctypes.wintypes
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config as cfg  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402
from drivers import foreground  # noqa: E402

OUT = Path(__file__).resolve().parent.parent.parent / "latex" / "BoostanSample" / "Pic"
OUT.mkdir(exist_ok=True)

user32 = ctypes.windll.user32
# GetWindowRect returns virtualized coords without DPI awareness.
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001 - older Windows fallback
    user32.SetProcessDPIAware()

PS_SHOT = """
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$bmp = New-Object System.Drawing.Bitmap({w}, {h})
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen({x}, {y}, 0, 0, $bmp.Size)
$bmp.Save('{path}')
"""


def window_rect(title: str) -> tuple[int, int, int, int]:
    """Rect of the VISIBLE top-level window matching `title`.

    FindWindowW alone can hit a small hidden/auxiliary window with the
    same title (observed 121x182 for the main window during captures) -
    enumerate instead and take the largest visible match.
    """
    best: tuple[int, int, int, int] | None = None

    @ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.wintypes.HWND, ctypes.wintypes.LPARAM)
    def cb(hwnd, lparam):
        nonlocal best
        buf = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, buf, 256)
        if buf.value == title and user32.IsWindowVisible(hwnd):
            rect = ctypes.wintypes.RECT()
            if ctypes.windll.dwmapi.DwmGetWindowAttribute(
                    hwnd, 9, ctypes.byref(rect),
                    ctypes.sizeof(rect)) != 0:
                user32.GetWindowRect(hwnd, ctypes.byref(rect))
            w, h = rect.right - rect.left, rect.bottom - rect.top
            if best is None or w * h > best[2] * best[3]:
                best = (rect.left, rect.top, w, h)
        return True

    user32.EnumWindows(cb, 0)
    if best is None:
        raise RuntimeError(f"no visible window titled {title!r}")
    return best


def shoot(title: str, name: str) -> None:
    x, y, w, h = window_rect(title)
    path = OUT / f"{name}.png"
    cmd = PS_SHOT.format(w=w, h=h, x=x, y=y, path=str(path).replace("\\", "/"))
    subprocess.run(["powershell", "-NoProfile", "-Command", cmd],
                   check=True, capture_output=True)
    print("saved", path, f"({w}x{h})")


MAIN_TITLE = "Iran Signal Tracker"
SIGNIN_TITLE = "Sign in - Iran Signal Tracker"

tracker = SignalTrackerDsl().launch()
try:
    # 1) Sign-in gate.
    foreground.focus_app()
    time.sleep(0.5)
    shoot(SIGNIN_TITLE, "fig_signin")

    # Login and route.
    tracker.login(cfg.E2E_USER, cfg.E2E_PASSWORD)

    # 2) Network tab: Fars filter + search for a state worth showing.
    net = tracker.network()
    net.province("Fars").search()
    time.sleep(0.5)
    foreground.focus_app()
    time.sleep(0.5)
    shoot(MAIN_TITLE, "fig_network")
    net.reset()  # clear the status-bar message for later shots

    # 3) Dashboard (default filters).
    tracker.dashboard()
    time.sleep(1.0)
    foreground.focus_app()
    time.sleep(0.5)
    shoot(MAIN_TITLE, "fig_dashboard")

    # 4) Reports (default state).
    tracker.reports()
    time.sleep(1.0)
    foreground.focus_app()
    time.sleep(0.5)
    shoot(MAIN_TITLE, "fig_reports")
finally:
    tracker.quit()