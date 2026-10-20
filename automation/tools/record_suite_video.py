"""Record the full E2E suite run as MP4 for the thesis (media9 embed).

ffmpeg (bundled via imageio-ffmpeg) captures the primary screen at 2 fps
while the suite runs in a VISIBLE console window; the app windows launch
and close on screen during the run. Output: latex/BoostanSample/Pic/
fig_suite_run.mp4.
"""
from __future__ import annotations

import ctypes
import ctypes.wintypes
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import imageio_ffmpeg  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent
OUT = ROOT / "latex" / "BoostanSample" / "Pic" / "fig_suite_run.mp4"
OUT.parent.mkdir(exist_ok=True)

ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()

# Record ONLY the primary monitor: gdigrab "desktop" spans the whole
# virtual desktop (owner has two monitors side by side; the left one
# shows unrelated desktop content and must not appear in the thesis
# video). Coordinates are virtual-desktop relative.
user32 = ctypes.windll.user32
user32.SetProcessDPIAware()


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


class MONITORINFO(ctypes.Structure):
    _fields_ = [("cbSize", ctypes.c_ulong), ("rcMonitor", RECT),
                ("rcWork", RECT), ("dwFlags", ctypes.c_ulong)]


MONITORINFOF_PRIMARY = 1
_primary = None


@ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
def _mon_cb(hmonitor, hdc, lprect):
    global _primary
    mi = MONITORINFO()
    mi.cbSize = ctypes.sizeof(MONITORINFO)
    if user32.GetMonitorInfoW(hmonitor, ctypes.byref(mi)) and \
            (mi.dwFlags & MONITORINFOF_PRIMARY):
        _primary = mi.rcMonitor
    return True


user32.EnumDisplayMonitors(0, 0, _mon_cb, 0)
assert _primary is not None, "primary monitor not found"
mx, my = _primary.left, _primary.top
mw, mh = _primary.right - _primary.left, _primary.bottom - _primary.top
print(f"primary monitor: ({mx},{my}) {mw}x{mh}")

# Enlarge ONLY the suite console (owner ruling: never resize the app —
# resizing app windows mid-test breaks WAD interactions; observed: the
# maximizer spamming the sign-in dialog failed the whole run).
SW_MAXIMIZE = 3


@ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.wintypes.HWND, ctypes.wintypes.LPARAM)
def _max_cb(hwnd, lparam):
    buf = ctypes.create_unicode_buffer(128)
    user32.GetWindowTextW(hwnd, buf, 128)
    if buf.value == "IST-Suite-Run" and user32.IsWindowVisible(hwnd):
        user32.ShowWindow(hwnd, SW_MAXIMIZE)
    return True


def _maximize_poller(stop: list) -> None:
    while not stop:
        user32.EnumWindows(_max_cb, 0)
        time.sleep(1.0)

cmd = [
    ffmpeg, "-y",
    "-f", "gdigrab", "-framerate", "2",
    "-offset_x", str(mx), "-offset_y", str(my),
    "-video_size", f"{mw}x{mh}",
    "-i", "desktop",
    "-vf", "scale=1280:-2",
    "-c:v", "libx264", "-preset", "veryfast", "-crf", "30",
    "-pix_fmt", "yuv420p", "-movflags", "+faststart",
    str(OUT),
]
print("recording:", OUT)
rec = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2.0)

# Small demo suite (owner ruling): 7 tests incl. the intentional failure.
DEMO_TESTS = [
    "test_tc001_launch.py",
    "test_tc012_login.py",
    "test_tc002_province_filter.py",
    "test_tc009_export_csv.py",
    "test_tc010_chart_visibility.py",
    "test_tc014_dashboard.py",
    "test_tc019_demo_failure.py",
]
pytest_cmd = "python -m pytest automation/tests/" + " automation/tests/".join(DEMO_TESTS)

bat = Path(__file__).resolve().parent / "_run_suite.bat"
bat.write_text("@echo off\r\n"
               "cd /d C:\\git\\final_project\r\n"
               'start "IST-Suite-Run" /wait powershell -NoProfile -Command '
               f'"{pytest_cmd}"\r\n',
               encoding="ascii")

import threading
stop = []
poller = threading.Thread(target=_maximize_poller, args=(stop,), daemon=True)
poller.start()
runner = subprocess.Popen(["cmd", "/c", str(bat)],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
runner.wait()
stop.append(True)
poller.join(timeout=2)
bat.unlink(missing_ok=True)

rec.communicate(b"q")  # graceful stop (writes the mp4 moov atom)
rec.wait(timeout=30)
print("done:", OUT, OUT.stat().st_size // 1024, "KB")