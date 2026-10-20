# TC024 — Reports drill-down under filters: province Tehran narrows the
# summary to Tehran only; the first (label-ascending) row selects into the
# drill trend chart, which stays displayed; the export writes exactly the
# filtered summary rows.
import csv


def test_tc024_reports_drill_filtered(signed):
    rep = signed.reports()
    rep.province("Tehran").search()
    rep.group_by("Operator")
    rep.select_row(0)

    # Operators sort ascending: Irancell first.
    assert rep.row_label(0) == "Irancell", f"first label: {rep.row_label(0)!r}"
    assert rep.drill_chart_visible(), "drill trend chart must be displayed"

    # Export follows the CURRENT (filtered) summary: group Operator over
    # Tehran lists the 4 operators ascending, Irancell first.
    result = rep.export().should_create_file()
    with result.newest_file().open(newline="", encoding="utf-8") as fh:
        reader = csv.reader(fh)
        next(reader)  # header
        rows = list(reader)
    assert len(rows) == rep.summary_rows()
    assert rows[0][0] == "Irancell", f"first exported label: {rows[0][0]!r}"