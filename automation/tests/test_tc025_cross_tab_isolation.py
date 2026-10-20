# TC025 — Cross-tab filter isolation + the pinned empty state: an
# out-of-era dashboard date range zeroes every dashboard widget in the
# pinned formats, while the Reports and Network tabs keep their own
# (independent) baselines — no filter state leaks across tabs.
#
# The reports baseline rides the groupShare caption (pinned "Rows by
# <Group> - N slices"), NOT realized-row counting: the summary table is
# partially virtualized at full-province height (see SummaryTable).
def _slice_count(reports) -> int:
    caption = reports.caption_text("groupShareCaption")
    # "Rows by Province - 10 slices"
    return int(caption.rsplit(" - ", 1)[1].split()[0])


def test_tc025_cross_tab_isolation(signed):
    # Baselines BEFORE any filter, on their own tabs.
    rep0 = signed.reports()
    baseline_reports = _slice_count(rep0)
    assert baseline_reports > 0
    net0 = signed.network()
    baseline_network = net0.row_count()
    assert baseline_network == 10080

    # Dashboard: out-of-era dates -> the pinned empty state.
    dash = signed.dashboard()
    dash.from_date("2020-01-01").to_date("2020-01-02").search()
    assert dash.kpi("cells") == "0 cells"
    assert dash.kpi("bestOperator") == ""
    assert dash.kpi("worstOperator") == ""
    assert dash.trend_points() == 0
    assert dash.worst_cells_rows() == 0
    assert dash.caption_text("operatorShareCaption") == (
        "Cells by operator - 0 slices")
    assert dash.caption_text("techQualityCaption") == (
        "Avg quality by technology - 0 groups")
    assert dash.pie_slices() == []
    assert dash.bar_groups() == []

    # No leakage: Reports and Network keep their baselines.
    assert _slice_count(signed.reports()) == baseline_reports
    assert signed.network().row_count() == baseline_network