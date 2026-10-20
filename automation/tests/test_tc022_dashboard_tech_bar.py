# TC022 — Dashboard avg-quality-by-technology bar chart: the readable
# groups equal the CSV-derived quality per technology through the shared
# (dBm + 120) / 0.7 transform, to the pinned one-decimal display.
import csv
from collections import defaultdict


def _quality(avg_dbm: float) -> float:
    return (avg_dbm + 120.0) / 0.7


def test_tc022_dashboard_tech_bar(signed):
    net = signed.network()
    result = net.export().should_create_file()
    with result.newest_file().open(newline="", encoding="utf-8") as fh:
        reader = csv.reader(fh)
        next(reader)  # header
        sums: dict[str, float] = defaultdict(float)
        counts: dict[str, int] = defaultdict(int)
        for row in reader:
            tech = row[4]
            sums[tech] += float(row[5])
            counts[tech] += 1
    expected = {tech: _quality(sums[tech] / counts[tech]) for tech in sums}

    dash = signed.dashboard()
    groups = dash.bar_groups()
    assert {label for label, _pct in groups} == set(expected)

    for label, pct in groups:
        assert abs(pct - expected[label]) <= 0.06, (
            f"{label}: readout {pct} vs CSV-derived {expected[label]}")

    assert dash.caption_text("techQualityCaption") == (
        f"Avg quality by technology - {len(groups)} groups")