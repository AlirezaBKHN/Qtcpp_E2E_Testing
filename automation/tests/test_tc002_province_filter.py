# TC002 — Province Filter: select Tehran, search, verify the filter narrows
# the table (relative assertion per the plan; exact seeded counts live in
# the probe).
def test_tc002_province_filter(signed):
    net = signed.network()
    baseline = net.row_count()
    net.province("Tehran").search()
    net.table().should_have_rows()
    assert net.row_count() < baseline, "filter must reduce row count"