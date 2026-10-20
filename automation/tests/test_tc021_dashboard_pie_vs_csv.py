# TC021 — Dashboard operator pie matches the CSV-derived share: with
# province Tehran applied on BOTH tabs, every pie slice count equals the
# operator distribution of the network CSV export under the same filter,
# and the caption carries the slice count.
import csv
from collections import Counter


def test_tc021_dashboard_pie_vs_csv(signed):
    net = signed.network()
    net.province("Tehran")
    result = net.export().should_create_file()
    with result.newest_file().open(newline="", encoding="utf-8") as fh:
        reader = csv.reader(fh)
        next(reader)  # header
        per_operator = Counter(row[3] for row in reader)
    assert len(per_operator) == 4, f"expected 4 operators: {per_operator}"

    dash = signed.dashboard()
    dash.province("Tehran").search()

    slices = dash.pie_slices()
    assert {label: count for label, count, _pct in slices} == dict(per_operator)

    assert dash.caption_text("operatorShareCaption") == (
        f"Cells by operator - {len(slices)} slices")