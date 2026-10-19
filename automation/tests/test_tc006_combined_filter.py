# TC006 — Combined Filter: Province + City + Technology.
def test_tc006_combined_filter(tracker):
    tracker.province("Tehran").search().city("Tehran").search().technology("5G").search()
    tracker.table().should_have_rows()
