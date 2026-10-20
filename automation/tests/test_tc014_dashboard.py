# TC014 — Dashboard: routed via the home card; KPI readouts carry the
# pinned format, the trend has exactly 7 daily buckets, weakest cells has
# rows.
import re


def test_tc014_dashboard(signed):
    dash = signed.dashboard()  # routes via homeCardDashboard

    # KPI format: quality as a percentage, cells as 'N cells' (the readout
    # mirrors the DISPLAYED text, probed '10080 cells'), operators
    # non-empty.
    avg = dash.kpi("avgQuality")
    assert re.fullmatch(r"\d+(\.\d+)?%", avg), f"avgQuality format: {avg!r}"
    cells = dash.kpi("cells")
    assert re.fullmatch(r"[\d,]+ cells", cells), f"cells format: {cells!r}"
    assert dash.kpi("bestOperator"), "bestOperator must be non-empty"
    assert dash.kpi("worstOperator"), "worstOperator must be non-empty"

    assert dash.trend_points() == 7
    assert dash.worst_cells_rows() >= 1