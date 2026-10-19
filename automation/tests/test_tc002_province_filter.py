# TC002 — Province Filter: select Tehran, search, verify rows > 0.
def test_tc002_province_filter(tracker):
    baseline = tracker.row_count()
    tracker.province("Tehran").search()
    tracker.table().should_have_rows()
    assert tracker.row_count() < baseline, "filter must reduce row count"
