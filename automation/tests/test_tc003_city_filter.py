# TC003 — City Filter (cascade): select province Fars, city Shiraz, search.
def test_tc003_city_filter(tracker):
    tracker.province("Fars").search()
    tracker.city("Shiraz").search()  # cascade: Shiraz must be offered
    tracker.table().should_have_rows()
