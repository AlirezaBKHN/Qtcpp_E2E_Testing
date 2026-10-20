"""Per-suite fixtures for the Iran Signal Tracker E2E suite."""
from __future__ import annotations

import shutil
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
def db_snapshot(tmp_path_factory) -> Path:
    """One pristine copy of the committed DB per session (outside the repo).

    Taken before ANY test can run so no app mutation can leak into it; the
    committed database/signaltracker.db must stay untouched across runs.
    """
    factory.kill_app()
    dest = tmp_path_factory.mktemp("istdb") / cfg.DB_PATH.name
    shutil.copy2(cfg.DB_PATH, dest)
    return dest


@pytest.fixture(autouse=False)
def isolate_db(db_snapshot: Path):
    """Per-test isolation (owner requirement): restore the pristine DB and
    purge the exports dir BEFORE every test - and again on teardown so the
    tree stays clean even after the LAST test of the session.

    A crashed/aborted test is also covered: with fixtures, the restore point
    is this fixture's setup of the NEXT test, not the dying test's own
    cleanup.
    """

    def restore() -> None:
        shutil.copy2(db_snapshot, cfg.DB_PATH)
        cfg.EXPORT_DIR.mkdir(parents=True, exist_ok=True)
        for csv_file in cfg.EXPORT_DIR.glob("*.csv"):
            csv_file.unlink()

    restore()
    yield
    restore()


@pytest.fixture(scope="session")
def stack_available() -> bool:
    """True when the chosen driver endpoint answers."""
    url = cfg.DIRECT_URL if cfg.DRIVER_MODE == "direct" else cfg.APPIUM_URL
    return _server_reachable(url)


@pytest.fixture()
def tracker(stack_available: bool, isolate_db):
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


@pytest.fixture()
def signed(stack_available: bool, isolate_db):
    """An app session already logged in as the seeded e2e user."""
    if not stack_available:
        pytest.skip(f"automation stack not reachable. {STACK_HINT}")
    app = SignalTrackerDsl()
    app.launch()
    app.login(cfg.E2E_USER, cfg.E2E_PASSWORD)
    yield app
    try:
        app.quit()
    finally:
        pass


@pytest.fixture()
def admin(stack_available: bool, isolate_db):
    """An app session already logged in as the seeded admin."""
    if not stack_available:
        pytest.skip(f"automation stack not reachable. {STACK_HINT}")
    app = SignalTrackerDsl()
    app.launch()
    app.login(cfg.ADMIN_USER, cfg.ADMIN_PASSWORD)
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
