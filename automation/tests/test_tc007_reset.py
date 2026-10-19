# TC007 — Reset Filters: apply filters, press reset, verify all results return.
def test_tc007_reset(tracker):
    baseline = tracker.row_count()
    tracker.province("Mazandaran").operator_name("MCI").search()
    assert tracker.row_count() < baseline, "filters must reduce row count"
    tracker.reset()
    tracker.table().should_have_row_count(baseline)
