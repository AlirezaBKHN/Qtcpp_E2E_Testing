"""Driver factory: Appium 3 (windows driver) is the primary path per the
PRD architecture; a direct-WinAppDriver session is the hidden fallback
(DRIVER_MODE=direct, port 4729). Tests never touch this module.
"""
from __future__ import annotations

import subprocess
from typing import Any

import config as C

_driver = None


def _caps_appium() -> dict[str, Any]:
    return {
        "platformName": "Windows",
        "appium:automationName": "Windows",
        "appium:app": str(C.APP_EXE),
    }


def _ensure_wad_running() -> None:
    """In direct mode we start the WinAppDriver service ourselves."""
    import socket

    host, port = "127.0.0.1", int(C.DIRECT_URL.rsplit(":", 1)[1])
    try:
        with socket.create_connection((host, port), timeout=1):
            return  # already running
    except OSError:
        import pathlib
        import tempfile

        wad = r"C:\Program Files (x86)\Windows Application Driver\WinAppDriver.exe"
        log = pathlib.Path(tempfile.gettempdir()) / "wad_direct.log"
        stdout = open(log, "w")
        subprocess.Popen([wad, str(port)], stdout=stdout,
                         stderr=subprocess.STDOUT)
        # Give the driver a moment to bind.
        import time
        for _ in range(50):
            try:
                with socket.create_connection((host, port), timeout=1):
                    return
            except OSError:
                time.sleep(0.1)
        raise RuntimeError(f"WinAppDriver failed to bind {host}:{port}")


def get_driver():
    """Create (or return) the singleton driver session for the app."""
    global _driver
    if _driver is not None:
        return _driver

    if C.DRIVER_MODE == "direct":
        _ensure_wad_running()
        from selenium.webdriver import Remote
        from selenium.webdriver.common.options import ArgOptions

        opts = ArgOptions()
        for key, value in _caps_direct().items():
            opts.set_capability(key, value)
        _driver = Remote(command_executor=C.DIRECT_URL, options=opts,
                         keep_alive=False)
        return _driver

    from appium import webdriver
    from appium.options.windows import WindowsOptions

    options = WindowsOptions()
    options.load_capabilities(_caps_appium())
    options.implicit_wait = C.implicit_timeout_ms / 1000
    _driver = webdriver.Remote(C.APPIUM_URL, options=options)
    return _driver


def quit_driver() -> None:
    global _driver
    if _driver is not None:
        try:
            _driver.quit()
        finally:
            _driver = None
