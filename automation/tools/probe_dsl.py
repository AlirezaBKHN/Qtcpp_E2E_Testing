"""Live happy-path probe of the full DSL (Task 7, ungated).

Verifies every binding-contract call against the running stack before the
tests are written. Run while the Appium server is up on :4723.

Flow: launch -> register a one-off user -> log in -> home-card click-through
-> network flow -> logout -> admin login -> remove the one-off user. The
one-off user is removed at the end, so the committed DB stays byte-clean.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

# Console copies carry the pinned em dash ("Registered - log in."); Windows
# consoles default to cp1252 and would abort the probe on encode errors.
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from dsl.tracker import SignalTrackerDsl  # noqa: E402

PROBE_USER = "e2e_probe_user"
PROBE_PASSWORD = "probe123"


def p(*a):
    print(*a, flush=True)


def main():
    t0 = time.time()

    def mark(msg, *extra):
        p(f"[{time.time() - t0:5.1f}s] {msg}", *extra)
    tracker = SignalTrackerDsl().launch()
    mark("launch OK (sign-in window)")

    # Register an extra user: pinned success copy, then back to login mode.
    tracker.register(PROBE_USER, PROBE_PASSWORD)
    mark("register message:", tracker.register_message())
    assert tracker.register_message() == "Registered — log in.", \
        "pin: register success copy"

    # Empty-password rejection copy (owner-pinned): register() self-toggles
    # into register mode from the login screen the success copy left us on;
    # the rejection keeps the dialog in register mode.
    tracker.register("probe_pwcheck", "")
    mark("empty password case probed:", tracker.register_message())
    assert tracker.register_message() == "Password can not be empty.", \
        "pin: empty password copy"
    # Back to the login screen for the seeded-account login.
    tracker.toggle_register_mode()

    # Login with the seeded account.
    tracker.login("e2e_user", "e2e123")
    user = tracker.current_user()
    mark("current user:", user)
    assert user.startswith("user: "), f"user readout: {user!r}"

    # Home cards click-through.
    tracker.network()
    mark("network() OK")
    tracker.dashboard()
    mark("dashboard() OK")
    tracker.reports()
    mark("reports() OK")

    # Network flow (contract moved verbatim into .network()).
    net = tracker.network()
    baseline = net.row_count()
    mark(f"baseline row count: {baseline}")
    assert baseline == 10080

    net.province("Tehran").search()
    n_province = net.row_count()
    mark(f"after province=Tehran: {n_province}")
    assert 0 < n_province < baseline

    net.city("Tehran").search()
    n_city = net.row_count()
    mark(f"after city=Tehran: {n_city}")
    assert 0 < n_city <= n_province

    net.technology("5G").search()
    n_tech = net.row_count()
    mark(f"after 5G: {n_tech}")
    assert n_tech > 0

    net.reset().search()
    net.operator_name("Irancell").search()
    mark("after operator=Irancell rows:", net.row_count())
    net.table().should_contain("Irancell")
    mark("table contains Irancell OK")

    net.chart().should_be_visible()
    mark("chart visible OK")

    net.export().should_create_file()
    mark("export file OK")

    net.reset().search()
    mark("after reset rows:", net.row_count())
    net.table().should_have_row_count(baseline)
    net.table().should_have_rows()
    mark("reset restores baseline OK")

    net.refresh()
    net.table().should_have_row_count(baseline)
    mark("refresh stable OK")

    # Dashboard readouts.
    dash = tracker.dashboard()
    kpi = dash.kpi("avgQuality")
    mark("avgQuality KPI:", kpi)
    cleaned = kpi.replace(".", "").replace(",", "").rstrip("%")
    assert cleaned.isdigit(), f"kpi {kpi!r}"
    trend = dash.trend_points()
    mark("trend points:", trend)
    assert trend == 7, "7 day buckets pinned"
    worst = dash.worst_cells_rows()
    mark("worst cells rows:", worst)
    assert worst >= 1

    # Logout returns to the sign-in dialog; current user readout is gone.
    tracker.logout()
    mark("logout OK (sign-in window back)")

    # Admin: cleanup the registered one-off user (keeps the committed DB
    # exactly as committed; guest/admin removals are TC territory).
    tracker.login("admin", "admin")
    users = tracker.users()
    mark("users list:", users.list())
    assert PROBE_USER in users.list()
    users.remove(PROBE_USER)
    assert PROBE_USER not in users.list()
    mark("probe user removed; committed DB left byte-clean")

    tracker.quit()
    mark("ALL DSL PROBE STEPS PASSED")


if __name__ == "__main__":
    main()
