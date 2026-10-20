"""Driver factory: Appium 3 (windows driver) is the primary path per the
PRD architecture; a direct-WinAppDriver session is the hidden fallback
(DRIVER_MODE=direct, port 4729). Tests never touch this module.

PHASE-ATTACH RULE (live-verified 2026-10-20): WAD pins a session to ONE
top-level window handle. With the auth gate the first window is the
sign-in dialog and it is DESTROYED at every login/logout - a session
created via the app capability cannot see anything after that (probed:
session element count collapses to the stale pane while the app shows the
main window). Sessions therefore attach by `appTopLevelWindow` and the DSL
re-attaches at each top-level transition (sign-in -> main -> sign-in).
Find latency through an attached session is ~0.2s (vs ~9s for a desktop
Root session, also probed - Root was rejected for that reason).

The app process is factory-owned: launched here (killing any lingering
instance first, the launcher's old behavior) and terminated when the
factory's session quits - session churn across phases never kills or
restarts the app.
"""
from __future__ import annotations

import ctypes
import subprocess
import time
from typing import Any

import config as C

_driver = None
_process = None  # the app process we own in this session

_LINGERING_APP = "IranSignalTracker.exe"

_SIGNIN_WINDOW_TITLE = "Sign in - Iran Signal Tracker"
_MAIN_WINDOW_TITLE = "Iran Signal Tracker"

# Title of the window the CURRENT session is attached to (page-object
# handle cache is keyed per phase; None until the first attach).
CURRENT_TITLE: str | None = None


def current_title() -> str | None:
    return CURRENT_TITLE


def _kill_lingering() -> None:
    """Never hand a session a leftover app window."""
    subprocess.run(["taskkill", "/F", "/IM", _LINGERING_APP],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


# Public alias (conftest kills lingering apps before snapshotting the DB).
kill_app = _kill_lingering


def _start_app() -> None:
    """Launch the app under test; sessions attach to its windows."""
    global _process
    if _process is not None and _process.poll() is None:
        return
    _process = subprocess.Popen([str(C.APP_EXE)])


def _find_window(title: str) -> int:
    return ctypes.windll.user32.FindWindowW(None, title)


def wait_hwnd(title: str, timeout: float = 30.0) -> int:
    """Poll for a top-level window with the exact title; return its HWND."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        hwnd = _find_window(title)
        if hwnd:
            return hwnd
        time.sleep(0.3)
    raise RuntimeError(f"window {title!r} not found within {timeout}s")


def _ensure_wad_running() -> None:
    """In direct mode we start the WinAppDriver service ourselves.

    WAD needs a REAL console: with redirected stdout it prints
    'Press ENTER to exit', reads EOF and exits immediately (observed), so
    the process gets a fresh console instead of piped handles.
    """
    import socket

    host, port = "127.0.0.1", int(C.DIRECT_URL.rsplit(":", 1)[1])
    try:
        with socket.create_connection((host, port), timeout=1):
            return  # already running
    except OSError:
        wad = r"C:\Program Files (x86)\Windows Application Driver\WinAppDriver.exe"
        subprocess.Popen([wad, str(port)],
                         creationflags=subprocess.CREATE_NEW_CONSOLE)


def attach(window_title: str, timeout: float = 30.0):
    """(Re)open the driver session attached to the app window `title`.

    Closes the previous phase session (the app process keeps running -
    sessions created by appTopLevelWindow own nothing). Raises when the
    window never appears.
    """
    global _driver, CURRENT_TITLE
    if _driver is not None:
        try:
            _driver.quit()  # phase teardown only; the app process survives
        except Exception:  # noqa: BLE001 - a dead window must not block
            pass
        _driver = None
    if _process is None or _process.poll() is not None:
        _kill_lingering()
        _start_app()
    hwnd = wait_hwnd(window_title, timeout)
    CURRENT_TITLE = window_title
    caps: dict[str, Any] = {
        "platformName": "Windows",
        "appium:automationName": "Windows",
        "appTopLevelWindow": hex(hwnd),
    }
    if C.DRIVER_MODE == "direct":
        _ensure_wad_running()
        from selenium.webdriver import Remote
        from selenium.webdriver.common.options import ArgOptions

        opts = ArgOptions()
        for key, value in caps.items():
            opts.set_capability(key, value)
        _driver = Remote(command_executor=C.DIRECT_URL, options=opts,
                         keep_alive=False)
    else:
        from appium import webdriver
        from appium.options.windows import WindowsOptions

        options = WindowsOptions()
        options.load_capabilities(caps)
        options.implicit_wait = C.implicit_timeout_ms / 1000
        _driver = webdriver.Remote(C.APPIUM_URL, options=options)
    return _driver


def get_driver():
    """Compatibility entry: attach to the CURRENT app phase.

    New code should call attach(<title>) explicitly (phases matter with the
    auth gate); this stays for tools that only need the sign-in screen.
    """
    return attach(_SIGNIN_WINDOW_TITLE)


def quit_driver() -> None:
    global _driver, _process
    try:
        if _driver is not None:
            _driver.quit()
    finally:
        _driver = None
        # Factory-owned process: the session never outlives the app (the
        # launcher's old ms:forcequit contract).
        if _process is not None:
            try:
                _process.terminate()
                _process.wait(timeout=5)
            except Exception:  # noqa: BLE001 - teardown must never raise
                _kill_lingering()
            _process = None