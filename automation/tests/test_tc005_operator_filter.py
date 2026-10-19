# TC005 — Operator Filter: select Irancell, search, verify results contain Irancell.
def test_tc005_operator_filter(tracker):
    tracker.operator_name("Irancell").search()
    tracker.table().should_have_rows()
    tracker.table().should_contain("Irancell")
