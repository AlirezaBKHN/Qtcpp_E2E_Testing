# TC004 — Technology Filter: select 5G, search, verify rows > 0.
def test_tc004_technology_filter(tracker):
    tracker.technology("5G").search()
    tracker.table().should_have_rows()
