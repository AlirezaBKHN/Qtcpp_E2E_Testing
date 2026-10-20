# TC020 — Dashboard filters narrow the KPIs: province Fars via the
# dashboard filter section (Search applies), KPI 'cells' equals the
# network-tab CSV export row count under the SAME filter (cross-tab
# consistency of one filter semantics), trend stays within the era.
import csv


def test_tc020_dashboard_filters(signed):
    dash = signed.dashboard()  # routes via homeCardDashboard

    base = int(dash.kpi("cells").split()[0])
    assert base == 10080, f"baseline cells: {base!r}"

    dash.province("Fars").search()
    cells = int(dash.kpi("cells").split()[0])
    assert 0 < cells < base, f"filtered cells must narrow: {cells}"

    # The trend honors the filter and stays within the 7-day era. Read it
    # while the dashboard tab is attached - hidden tab pages are pruned
    # from the attached-session UIA tree (repo quirk).
    pts = dash.trend_points()
    assert 1 <= pts <= 7, f"trend points: {pts}"

    # Cross-check: the network tab's CSV export under the same filter.
    net = signed.network()
    net.province("Fars")
    result = net.export().should_create_file()
    with result.newest_file().open(newline="", encoding="utf-8") as fh:
        csv_rows = sum(1 for _ in csv.reader(fh)) - 1
    assert cells == csv_rows, (
        f"dashboard kpiCells {cells} != network CSV rows {csv_rows}")

    # Back on the dashboard: the filter state survived the tab switch.
    assert int(signed.dashboard().kpi("cells").split()[0]) == cells

    # The date readouts still expose the era defaults (no dates typed).
    assert dash.from_date_text() != "" and dash.to_date_text() != ""