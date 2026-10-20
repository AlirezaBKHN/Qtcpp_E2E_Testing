# TC004 — Technology Filter: select 5G, search, verify rows > 0.
def test_tc004_technology_filter(signed):
    net = signed.network()
    net.technology("5G").search()
    net.table().should_have_rows()