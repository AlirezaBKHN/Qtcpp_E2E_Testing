"""Live happy-path probe of the full DSL (Task 7, ungated).

Verifies every binding-contract call against the running stack before the
tests are written. Run while the Appium server is up on :4723.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from dsl.tracker import SignalTrackerDsl  # noqa: E402


def main():
    tracker = SignalTrackerDsl().launch()
    print("launch OK")

    baseline = tracker.row_count()
    print(f"baseline row count: {baseline}")
    assert baseline == 1500

    tracker.province("Tehran").search()
    n_province = tracker.row_count()
    print(f"after province=Tehran: {n_province}")
    assert 0 < n_province < baseline

    tracker.city("Tehran").search()
    n_city = tracker.row_count()
    print(f"after city=Tehran: {n_city}")
    assert 0 < n_city <= n_province

    tracker.technology("5G").search()
    n_tech = tracker.row_count()
    print(f"after 5G: {n_tech}")
    assert n_tech > 0

    # Full-stack combos may have zero seeded rows by chance; the operator
    # leg of the probe runs on its own (operator filter only).
    tracker.reset().search()
    tracker.operator_name("Irancell").search()
    print("after operator=Irancell rows:", tracker.row_count())
    tracker.table().should_contain("Irancell")
    print("table contains Irancell OK")

    tracker.chart().should_be_visible()
    print("chart visible OK")

    tracker.export().should_create_file()
    print("export file OK")

    tracker.reset().search()
    print("after reset rows:", tracker.row_count())
    tracker.table().should_have_row_count(baseline)
    tracker.table().should_have_rows()
    print("reset restores baseline OK")

    tracker.refresh()
    tracker.table().should_have_row_count(baseline)
    print("refresh stable OK")

    tracker.quit()
    print("ALL DSL PROBE STEPS PASSED")


if __name__ == "__main__":
    main()
