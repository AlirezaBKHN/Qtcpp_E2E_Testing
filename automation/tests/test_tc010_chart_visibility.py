# TC010 — Chart Visibility: verify chart displayed.
def test_tc010_chart_visibility(tracker):
    tracker.chart().should_be_visible()
