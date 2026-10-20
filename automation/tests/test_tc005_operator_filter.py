# TC005 — Operator Filter: select Irancell, search, verify results contain
# Irancell.
def test_tc005_operator_filter(signed):
    net = signed.network()
    net.operator_name("Irancell").search()
    net.table().should_have_rows()
    net.table().should_contain("Irancell")