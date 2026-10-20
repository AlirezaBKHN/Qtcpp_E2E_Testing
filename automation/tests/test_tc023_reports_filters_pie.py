# TC023 — Reports filters + group share pie: operator Irancell applied on
# the reports filter section, Technology group; the pie slices equal the
# per-technology row counts of the network CSV export under the SAME
# filter, and the summary narrows to the same label set.
import csv
from collections import Counter


def test_tc023_reports_filters_pie(signed):
    net = signed.network()
    net.operator_name("Irancell")
    result = net.export().should_create_file()
    with result.newest_file().open(newline="", encoding="utf-8") as fh:
        reader = csv.reader(fh)
        next(reader)  # header
        per_technology = Counter(row[4] for row in reader)
    assert len(per_technology) == 4, f"expected 4 technologies: {per_technology}"

    rep = signed.reports()
    rep.operator_name("Irancell").search()
    rep.group_by("Technology")

    slices = rep.group_slices()
    assert {label: count for label, count, _pct in slices} == dict(per_technology)

    assert rep.summary_rows() == len(slices)
    assert rep.caption_text("groupShareCaption") == (
        f"Rows by Technology - {len(slices)} slices")