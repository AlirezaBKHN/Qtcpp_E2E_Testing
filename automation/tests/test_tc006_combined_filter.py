# TC006 — Combined Filter: Province + City + Technology.
def test_tc006_combined_filter(signed):
    net = signed.network()
    net.province("Tehran").search().city("Tehran").search().technology("5G").search()
    net.table().should_have_rows()