# TC003 — City Filter (cascade): select province Fars, city Shiraz, search.
# The city list is repopulated by the province cascade, so Shiraz must be
# offered after Fars is chosen.
def test_tc003_city_filter(signed):
    net = signed.network()
    net.province("Fars").search()
    net.city("Shiraz").search()  # cascade: Shiraz must be offered
    net.table().should_have_rows()