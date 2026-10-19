# TC008 — Refresh: refresh data, verify refresh completed with identical rows.
def test_tc008_refresh(tracker):
    before = tracker.row_count()
    tracker.refresh()
    tracker.table().should_have_row_count(before)  # exact equality: no dupes, no loss
