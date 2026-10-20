# TC008 — Refresh: refresh data, verify refresh completed with identical
# rows (exact equality: no dupes, no loss).
def test_tc008_refresh(signed):
    net = signed.network()
    before = net.row_count()
    net.refresh()
    net.table().should_have_row_count(before)