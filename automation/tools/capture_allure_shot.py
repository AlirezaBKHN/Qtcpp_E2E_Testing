"""Capture the Allure report browser window for the thesis figure."""
from __future__ import annotations

import ctypes
import ctypes.wintypes
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

OUT = (Path(__file__).resolve().parent.parent.parent / "latex" / "BoostanSample"
       / "Pic" / "fig_allure.png")
OUT.parent.mkdir(exist_ok=True)

user32 = ctypes.windll.user32
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:  # noqa: BLE001
    user32.SetProcessDPIAware()

PS_SHOT = """
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$bmp = New-Object System.Drawing.Bitmap({w}, {h})
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen({x}, {y}, 0, 0, $bmp.Size)
$bmp.Save('{path}')
"""

# Wait for a browser window titled with the Allure report.
deadline = time.time() + 30
found = None
while time.time() < deadline and found is None:
    hits: list = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.wintypes.HWND, ctypes.wintypes.LPARAM)
    def cb(hwnd, lparam):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, buf, 256)
        if "Allure" in buf.value and user32.IsWindowVisible(hwnd):
            rect = ctypes.wintypes.RECT()
            if ctypes.windll.dwmapi.DwmGetWindowAttribute(
                    hwnd, 9, ctypes.byref(rect), ctypes.sizeof(rect)) == 0:
                hits.append((buf.value, rect.left, rect.top,
                             rect.right - rect.left, rect.bottom - rect.top))
        return True

    user32.EnumWindows(cb, 0)
    if hits:
        found = max(hits, key=lambda h: h[3] * h[4])
    else:
        time.sleep(1.0)

if found is None:
    print("no Allure browser window found")
    raise SystemExit(1)

title, x, y, w, h = found
print("found:", title, (x, y, w, h))
cmd = PS_SHOT.format(w=w, h=h, x=x, y=y, path=str(OUT).replace("\\", "/"))
subprocess.run(["powershell", "-NoProfile", "-Command", cmd], check=True,
               capture_output=True)
print("saved", OUT)