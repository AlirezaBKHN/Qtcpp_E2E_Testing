"""Per-suite fixtures for the Iran Signal Tracker E2E suite."""
from __future__ import annotations

import sys
from pathlib import Path

# The automation package keeps flat module names (config, dsl, pageobjects).
# Make tests runnable both from repo root and via pytest.ini testpaths.
AUTOMATION_DIR = Path(__file__).resolve().parent
if str(AUTOMATION_DIR) not in sys.path:
    sys.path.insert(0, str(AUTOMATION_DIR))

import allure  # noqa: E402
import pytest  # noqa: E402

import config as cfg  # noqa: E402
from dsl.tracker import SignalTrackerDsl  # noqa: E402
from drivers import factory  # noqa: E402

STACK_HINT = (
    "Appium server must listen on %s (or DRIVER_MODE=direct with WinAppDriver "
    "on %s). See docs/env-setup.md and ci/README.md." % (cfg.APPIUM_URL, cfg.DIRECT_URL)
)


def _server_reachable(url: str) -> bool:
    import socket
    from urllib.parse import urlparse
    parsed = urlparse(url)
    host = parsed.hostname or "127.0.0.1"
    port = parsed.port or (4723 if "appium" in url else 4729)
    try:
        with socket.create_connection((host, port), timeout=2):
            return True
    except OSError:
        return False


@pytest.fixture(scope="session")
def stack_available() -> bool:
    """True when the chosen driver endpoint answers."""
    url = cfg.DIRECT_URL if cfg.DRIVER_MODE == "direct" else cfg.APPIUM_URL
    return _server_reachable(url)


@pytest.fixture()
def tracker(stack_available: bool):
    """One fresh application per E2E test; DSL handles the session."""
    if not stack_available:
        pytest.skip(f"automation stack not reachable. {STACK_HINT}")
    app = SignalTrackerDsl()
    app.launch()
    yield app
    try:
        app.quit()
    finally:
        pass


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    """Attach a screenshot to every failing E2E step."""
    outcome = yield
    report = outcome.get_result()
    if report.when == "call" and report.failed:
        try:
            driver = factory.get_driver()
            png = driver.get_screenshot_as_png()
            allure.attach(png, name=f"{item.name}-failure",
                          attachment_type=allure.attachment_type.PNG)
        except Exception:  # noqa: BLE001 - screenshot must never mask the test error
            pass
