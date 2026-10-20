# TC007 — Reset Filters: apply filters, press reset, verify all results
# return (exact baseline equality).
def test_tc007_reset(signed):
    net = signed.network()
    baseline = net.row_count()
    net.province("Mazandaran").operator_name("MCI").search()
    assert net.row_count() < baseline, "filters must reduce row count"
    net.reset()
    net.table().should_have_row_count(baseline)